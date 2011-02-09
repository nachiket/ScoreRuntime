//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 1999 The Regents of the University of California 
// Permission to use, copy, modify, and distribute this software and
// its documentation for any purpose, without fee, and without a
// written agreement is hereby granted, provided that the above copyright 
// notice and this paragraph and the following two paragraphs appear in
// all copies. 
//
// IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO ANY PARTY FOR
// DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING
// LOST PROFITS, ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION,
// EVEN IF THE UNIVERSITY OF CALIFORNIA HAS BEEN ADVISED OF THE POSSIBILITY OF
// SUCH DAMAGE. 
//
// THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY WARRANTIES,
// INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
// AND FITNESS FOR A PARTICULAR PURPOSE. THE SOFTWARE PROVIDED HEREUNDER IS ON
// AN "AS IS" BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATIONS TO
// PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS. 
//
//////////////////////////////////////////////////////////////////////////////
//
// BRASS source file
//
// SCORE runtime support
// $Revision: 2.70 $
//
//////////////////////////////////////////////////////////////////////////////

/* TODO List 
   1. garbage collection
      a. when "free" a stream, we need to set the corresponding consumer
         operator input to NULL
      b. when "close" a stream, we need to set the corresponding producer
         operator output to NULL 	 
   2. do type checking
      a. when a stream is constructed, it is passed in a type:
         width, signed, unsigned, fixed
	 There variables will be set in the constructor
      b. need to write out new methods called bindInput and bindOutput
         when these routine is called, a TYPE is passed in. Need to check
	 the class type variables (width,fixed,signed...) 
   3. need to make ScoreStream scale, i.e., when user write more token
      than available space (N_SLOTS), we need to increase N_SLOTS
   4. give warning message when stream fanout: coredump
   5. even though N_SLOTS is big, we need a way to simulate limited stream
      buffer space; this will cause bufflock in some cases and scheduler
      need to deal with it
   6. add FIFO support
   7. add timing support
   8. add time accounting
*/

#ifndef _ScoreStream_H

#define _ScoreStream_H

#include <stdio.h>
#include <iostream>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/errno.h>
#include <semaphore.h>
#include "ScoreType.h"
#include "ScoreGlobalCounter.h"
#include "ScoreThreadCounter.h"
#include "ScoreConfig.h"
#include "ScoreCustomStack.h"
//#include "ScoreGraphNode.h"
#include "LEDA/core/list.h"
#include <iostream>
#include <fstream>

using leda::list;

using std::cout;
using std::endl;
using std::cerr;

#define NO_STREAM (ScoreStream *)-1
#define DEFAULT_N_SLOTS SCORE_INPUTFIFO_CAPACITY
#define ARRAY_FIFO_SIZE 256
// LLU suffix for C99 support in C++ and GNU compilers! Allah is great! Allahu akbar!
#define EOS 0xffffffffffffffffLLU
#define EOFR 0xefffffffffffffffLLU
//#define EOS 0xdeadbeef
#define DONE_MUTEX 2
#define STREAM_OPERATOR_TYPE 1
#define STREAM_PAGE_TYPE     2
#define STREAM_SEGMENT_TYPE  3

#define NOT_USER_STREAM   0
#define USER_WRITE_STREAM 1
#define USER_READ_STREAM  2

typedef unsigned int AllocationTag;

#define ALLOCTAG_SHARED          0x98765432
#define ALLOCTAG_PRIVATE         0x23456789

/* **********no depth_hint*********** */
#define NEW_SCORE_STREAM() (new ScoreStream(64,0,DEFAULT_N_SLOTS,SCORE_STREAM_UNTYPED,NOT_USER_STREAM))
#define NEW_SIGNED_SCORE_STREAM(w) \
   (new TypedScoreStream<SCORE_STREAM_SIGNED_TYPE>(w,0,DEFAULT_N_SLOTS,NOT_USER_STREAM))
#define NEW_UNSIGNED_SCORE_STREAM(w) \
   (new TypedScoreStream<SCORE_STREAM_UNSIGNED_TYPE>(w,0,DEFAULT_N_SLOTS,NOT_USER_STREAM))
#define NEW_BOOLEAN_SCORE_STREAM() \
   (new TypedScoreStream<SCORE_STREAM_BOOLEAN_TYPE>(1,0,DEFAULT_N_SLOTS,NOT_USER_STREAM))
#define NEW_SIGNED_FIXED_STREAM(i,f) \
   (new TypedScoreStream<SCORE_STREAM_SIGNED_FIXED_TYPE>(i,f,DEFAULT_N_SLOTS,NOT_USER_STREAM))
#define NEW_UNSIGNED_FIXED_STREAM(i,f) \
   (new TypedScoreStream<SCORE_STREAM_UNSIGNED_FIXED_TYPE>(i,f,DEFAULT_N_SLOTS,NOT_USER_STREAM))
// Added by Nachiket to support floating-point streams
#define NEW_FLOAT_SCORE_STREAM() \
   (new TypedScoreStream<SCORE_STREAM_FLOAT_TYPE>(32,0,DEFAULT_N_SLOTS,NOT_USER_STREAM))
#define NEW_DOUBLE_SCORE_STREAM() \
   (new TypedScoreStream<SCORE_STREAM_DOUBLE_TYPE>(64,0,DEFAULT_N_SLOTS,NOT_USER_STREAM))


#define NEW_READ_SCORE_STREAM() (new ScoreStream(64,0,DEFAULT_N_SLOTS,SCORE_STREAM_UNTYPED,USER_READ_STREAM))
#define NEW_READ_SIGNED_SCORE_STREAM(w) \
   (new TypedScoreStream<SCORE_STREAM_SIGNED_TYPE>(w,0,DEFAULT_N_SLOTS,USER_READ_STREAM))
#define NEW_READ_UNSIGNED_SCORE_STREAM(w) \
   (new TypedScoreStream<SCORE_STREAM_UNSIGNED_TYPE>(w,0,DEFAULT_N_SLOTS,USER_READ_STREAM))
#define NEW_READ_BOOLEAN_SCORE_STREAM() \
   (new TypedScoreStream<SCORE_STREAM_BOOLEAN_TYPE>(1,0,DEFAULT_N_SLOTS,USER_READ_STREAM))
#define NEW_READ_SIGNED_FIXED_STREAM(i,f) \
   (new TypedScoreStream<SCORE_STREAM_SIGNED_FIXED_TYPE>(i,f,DEFAULT_N_SLOTS,USER_READ_STREAM))
#define NEW_READ_UNSIGNED_FIXED_STREAM(i,f) \
   (new TypedScoreStream<SCORE_STREAM_UNSIGNED_FIXED_TYPE>(i,f,DEFAULT_N_SLOTS,USER_READ_STREAM))

#define NEW_WRITE_SCORE_STREAM() (new ScoreStream(64,0,DEFAULT_N_SLOTS,SCORE_STREAM_UNTYPED,USER_WRITE_STREAM))
#define NEW_WRITE_SIGNED_SCORE_STREAM(w) \
   (new TypedScoreStream<SCORE_STREAM_SIGNED_TYPE>(w,0,DEFAULT_N_SLOTS,USER_WRITE_STREAM))
#define NEW_WRITE_UNSIGNED_SCORE_STREAM(w) \
   (new TypedScoreStream<SCORE_STREAM_UNSIGNED_TYPE>(w,0,DEFAULT_N_SLOTS,USER_WRITE_STREAM))
#define NEW_WRITE_BOOLEAN_SCORE_STREAM() \
   (new TypedScoreStream<SCORE_STREAM_BOOLEAN_TYPE>(1,0,DEFAULT_N_SLOTS,USER_WRITE_STREAM))
#define NEW_WRITE_SIGNED_FIXED_STREAM(i,f) \
   (new TypedScoreStream<SCORE_STREAM_SIGNED_FIXED_TYPE>(i,f,DEFAULT_N_SLOTS,USER_WRITE_STREAM))
#define NEW_WRITE_UNSIGNED_FIXED_STREAM(i,f) \
   (new TypedScoreStream<SCORE_STREAM_UNSIGNED_FIXED_TYPE>(i,f,DEFAULT_N_SLOTS,USER_WRITE_STREAM))

/* ********depth_hint macros********* */

#define NEW_SCORE_STREAM_DEPTH_HINT(dh) (new ScoreStream(64,0,DEFAULT_N_SLOTS,SCORE_STREAM_UNTYPED,NOT_USER_STREAM,dh))
#define NEW_SIGNED_SCORE_STREAM_DEPTH_HINT(w,dh) \
   (new TypedScoreStream<SCORE_STREAM_SIGNED_TYPE>(w,0,DEFAULT_N_SLOTS,NOT_USER_STREAM,dh))
#define NEW_UNSIGNED_SCORE_STREAM_DEPTH_HINT(w,dh) \
   (new TypedScoreStream<SCORE_STREAM_UNSIGNED_TYPE>(w,0,DEFAULT_N_SLOTS,NOT_USER_STREAM,dh))
#define NEW_BOOLEAN_SCORE_STREAM_DEPTH_HINT(dh) \
   (new TypedScoreStream<SCORE_STREAM_BOOLEAN_TYPE>(1,0,DEFAULT_N_SLOTS,NOT_USER_STREAM, dh))
#define NEW_SIGNED_FIXED_STREAM_DEPTH_HINT(i,f,dh) \
   (new TypedScoreStream<SCORE_STREAM_SIGNED_FIXED_TYPE>(i,f,DEFAULT_N_SLOTS,NOT_USER_STREAM,dh))
#define NEW_UNSIGNED_FIXED_STREAM_DEPTH_HINT(i,f,dh) \
   (new TypedScoreStream<SCORE_STREAM_UNSIGNED_FIXED_TYPE>(i,f,DEFAULT_N_SLOTS,NOT_USER_STREAM,dh))


#define NEW_READ_SCORE_STREAM_DEPTH_HINT(dh) (new ScoreStream(64,0,DEFAULT_N_SLOTS,SCORE_STREAM_UNTYPED,USER_READ_STREAM,dh))
#define NEW_READ_SIGNED_SCORE_STREAM_DEPTH_HINT(w,dh) \
   (new TypedScoreStream<SCORE_STREAM_SIGNED_TYPE>(w,0,DEFAULT_N_SLOTS,USER_READ_STREAM,dh))
#define NEW_READ_UNSIGNED_SCORE_STREAM_DEPTH_HINT(w,dh) \
   (new TypedScoreStream<SCORE_STREAM_UNSIGNED_TYPE>(w,0,DEFAULT_N_SLOTS,USER_READ_STREAM,dh))
#define NEW_READ_BOOLEAN_SCORE_STREAM_DEPTH_HINT(dh) \
   (new TypedScoreStream<SCORE_STREAM_BOOLEAN_TYPE>(1,0,DEFAULT_N_SLOTS,USER_READ_STREAM,dh))
#define NEW_READ_SIGNED_FIXED_STREAM_DEPTH_HINT(i,f,dh) \
   (new TypedScoreStream<SCORE_STREAM_SIGNED_FIXED_TYPE>(i,f,DEFAULT_N_SLOTS,USER_READ_STREAM,dh))
#define NEW_READ_UNSIGNED_FIXED_STREAM_DEPTH_HINT(i,f,dh) \
   (new TypedScoreStream<SCORE_STREAM_UNSIGNED_FIXED_TYPE>(i,f,DEFAULT_N_SLOTS,USER_READ_STREAM,dh))

#define NEW_WRITE_SCORE_STREAM_DEPTH_HINT(dh) (new ScoreStream(64,0,DEFAULT_N_SLOTS,SCORE_STREAM_UNTYPED,USER_WRITE_STREAM,dh))
#define NEW_WRITE_SIGNED_SCORE_STREAM_DEPTH_HINT(w,dh) \
   (new TypedScoreStream<SCORE_STREAM_SIGNED_TYPE>(w,0,DEFAULT_N_SLOTS,USER_WRITE_STREAM,dh))
#define NEW_WRITE_UNSIGNED_SCORE_STREAM_DEPTH_HINT(w,dh) \
   (new TypedScoreStream<SCORE_STREAM_UNSIGNED_TYPE>(w,0,DEFAULT_N_SLOTS,USER_WRITE_STREAM,dh))
#define NEW_WRITE_BOOLEAN_SCORE_STREAM_DEPTH_HINT(dh) \
   (new TypedScoreStream<SCORE_STREAM_BOOLEAN_TYPE>(1,0,DEFAULT_N_SLOTS,USER_WRITE_STREAM,dh))
#define NEW_WRITE_SIGNED_FIXED_STREAM_DEPTH_HINT(i,f,dh) \
   (new TypedScoreStream<SCORE_STREAM_SIGNED_FIXED_TYPE>(i,f,DEFAULT_N_SLOTS,USER_WRITE_STREAM,dh))
#define NEW_WRITE_UNSIGNED_FIXED_STREAM_DEPTH_HINT(i,f,dh) \
   (new TypedScoreStream<SCORE_STREAM_UNSIGNED_FIXED_TYPE>(i,f,DEFAULT_N_SLOTS,USER_WRITE_STREAM,dh))
/* end of hint_depth macros */

/////////////////////////////////////////////////////
/* **********no depth_hint*********** */
#define NEW_SCORE_STREAM_ARRAY() (new(ALLOCTAG_PRIVATE) ScoreStream(64,0,ARRAY_FIFO_SIZE,SCORE_STREAM_UNTYPED,NOT_USER_STREAM))
#define NEW_SIGNED_SCORE_STREAM_ARRAY(w) \
   (new(ALLOCTAG_PRIVATE) TypedScoreStream<SCORE_STREAM_SIGNED_TYPE>(w,0,ARRAY_FIFO_SIZE,NOT_USER_STREAM))
#define NEW_UNSIGNED_SCORE_STREAM_ARRAY(w) \
   (new(ALLOCTAG_PRIVATE) TypedScoreStream<SCORE_STREAM_UNSIGNED_TYPE>(w,0,ARRAY_FIFO_SIZE,NOT_USER_STREAM))
#define NEW_BOOLEAN_SCORE_STREAM_ARRAY() \
   (new(ALLOCTAG_PRIVATE) TypedScoreStream<SCORE_STREAM_BOOLEAN_TYPE>(1,0,ARRAY_FIFO_SIZE,NOT_USER_STREAM))
#define NEW_SIGNED_FIXED_STREAM_ARRAY(i,f) \
   (new(ALLOCTAG_PRIVATE) TypedScoreStream<SCORE_STREAM_SIGNED_FIXED_TYPE>(i,f,ARRAY_FIFO_SIZE,NOT_USER_STREAM))
#define NEW_UNSIGNED_FIXED_STREAM_ARRAY(i,f) \
   (new(ALLOCTAG_PRIVATE) TypedScoreStream<SCORE_STREAM_UNSIGNED_FIXED_TYPE>(i,f,ARRAY_FIFO_SIZE,NOT_USER_STREAM))


#define NEW_READ_SCORE_STREAM_ARRAY() \
   (new(ALLOCTAG_PRIVATE) ScoreStream(64,0,ARRAY_FIFO_SIZE,SCORE_STREAM_UNTYPED,USER_READ_STREAM))
#define NEW_READ_SIGNED_SCORE_STREAM_ARRAY(w) \
   (new(ALLOCTAG_PRIVATE) TypedScoreStream<SCORE_STREAM_SIGNED_TYPE>(w,0,ARRAY_FIFO_SIZE,USER_READ_STREAM))
#define NEW_READ_UNSIGNED_SCORE_STREAM_ARRAY(w) \
   (new(ALLOCTAG_PRIVATE) TypedScoreStream<SCORE_STREAM_UNSIGNED_TYPE>(w,0,ARRAY_FIFO_SIZE,USER_READ_STREAM))
#define NEW_READ_BOOLEAN_SCORE_STREAM_ARRAY() \
   (new(ALLOCTAG_PRIVATE) TypedScoreStream<SCORE_STREAM_BOOLEAN_TYPE>(1,0,ARRAY_FIFO_SIZE,USER_READ_STREAM))
#define NEW_READ_SIGNED_FIXED_STREAM_ARRAY(i,f) \
   (new(ALLOCTAG_PRIVATE) TypedScoreStream<SCORE_STREAM_SIGNED_FIXED_TYPE>(i,f,ARRAY_FIFO_SIZE,USER_READ_STREAM))
#define NEW_READ_UNSIGNED_FIXED_STREAM_ARRAY(i,f) \
   (new(ALLOCTAG_PRIVATE) TypedScoreStream<SCORE_STREAM_UNSIGNED_FIXED_TYPE>(i,f,ARRAY_FIFO_SIZE,USER_READ_STREAM))

#define NEW_WRITE_SCORE_STREAM_ARRAY() \
   (new(ALLOCTAG_PRIVATE) ScoreStream(64,0,ARRAY_FIFO_SIZE,SCORE_STREAM_UNTYPED,USER_WRITE_STREAM))
#define NEW_WRITE_SIGNED_SCORE_STREAM_ARRAY(w) \
   (new(ALLOCTAG_PRIVATE) TypedScoreStream<SCORE_STREAM_SIGNED_TYPE>(w,0,ARRAY_FIFO_SIZE,USER_WRITE_STREAM))
#define NEW_WRITE_UNSIGNED_SCORE_STREAM_ARRAY(w) \
   (new(ALLOCTAG_PRIVATE) TypedScoreStream<SCORE_STREAM_UNSIGNED_TYPE>(w,0,ARRAY_FIFO_SIZE,USER_WRITE_STREAM))
#define NEW_WRITE_BOOLEAN_SCORE_STREAM_ARRAY() \
   (new(ALLOCTAG_PRIVATE) TypedScoreStream<SCORE_STREAM_BOOLEAN_TYPE>(1,0,ARRAY_FIFO_SIZE,USER_WRITE_STREAM))
#define NEW_WRITE_SIGNED_FIXED_STREAM_ARRAY(i,f) \
   (new(ALLOCTAG_PRIVATE) TypedScoreStream<SCORE_STREAM_SIGNED_FIXED_TYPE>(i,f,ARRAY_FIFO_SIZE,USER_WRITE_STREAM))
#define NEW_WRITE_UNSIGNED_FIXED_STREAM_ARRAY(i,f) \
   (new(ALLOCTAG_PRIVATE) TypedScoreStream<SCORE_STREAM_UNSIGNED_FIXED_TYPE>(i,f,ARRAY_FIFO_SIZE,USER_WRITE_STREAM))

/* ********depth_hint macros********* */

#define NEW_SCORE_STREAM_DEPTH_HINT_ARRAY(dh) \
   (new(ALLOCTAG_PRIVATE) ScoreStream(64,0,ARRAY_FIFO_SIZE,SCORE_STREAM_UNTYPED,NOT_USER_STREAM,dh))
#define NEW_SIGNED_SCORE_STREAM_DEPTH_HINT_ARRAY(w,dh) \
   (new(ALLOCTAG_PRIVATE) TypedScoreStream<SCORE_STREAM_SIGNED_TYPE>(w,0,ARRAY_FIFO_SIZE,NOT_USER_STREAM,dh))
#define NEW_UNSIGNED_SCORE_STREAM_DEPTH_HINT_ARRAY(w,dh) \
   (new(ALLOCTAG_PRIVATE) TypedScoreStream<SCORE_STREAM_UNSIGNED_TYPE>(w,0,ARRAY_FIFO_SIZE,NOT_USER_STREAM,dh))
#define NEW_BOOLEAN_SCORE_STREAM_DEPTH_HINT_ARRAY(dh) \
   (new(ALLOCTAG_PRIVATE) TypedScoreStream<SCORE_STREAM_BOOLEAN_TYPE>(1,0,ARRAY_FIFO_SIZE,NOT_USER_STREAM, dh))
#define NEW_SIGNED_FIXED_STREAM_DEPTH_HINT_ARRAY(i,f,dh) \
   (new(ALLOCTAG_PRIVATE) TypedScoreStream<SCORE_STREAM_SIGNED_FIXED_TYPE>(i,f,ARRAY_FIFO_SIZE,NOT_USER_STREAM,dh))
#define NEW_UNSIGNED_FIXED_STREAM_DEPTH_HINT_ARRAY(i,f,dh) \
   (new(ALLOCTAG_PRIVATE) TypedScoreStream<SCORE_STREAM_UNSIGNED_FIXED_TYPE>(i,f,ARRAY_FIFO_SIZE,NOT_USER_STREAM,dh))


#define NEW_READ_SCORE_STREAM_DEPTH_HINT_ARRAY(dh) \
   (new(ALLOCTAG_PRIVATE) ScoreStream(64,0,ARRAY_FIFO_SIZE,SCORE_STREAM_UNTYPED,USER_READ_STREAM,dh))
#define NEW_READ_SIGNED_SCORE_STREAM_DEPTH_HINT_ARRAY(w,dh) \
   (new(ALLOCTAG_PRIVATE) TypedScoreStream<SCORE_STREAM_SIGNED_TYPE>(w,0,ARRAY_FIFO_SIZE,USER_READ_STREAM,dh))
#define NEW_READ_UNSIGNED_SCORE_STREAM_DEPTH_HINT_ARRAY(w,dh) \
   (new(ALLOCTAG_PRIVATE) TypedScoreStream<SCORE_STREAM_UNSIGNED_TYPE>(w,0,ARRAY_FIFO_SIZE,USER_READ_STREAM,dh))
#define NEW_READ_BOOLEAN_SCORE_STREAM_DEPTH_HINT_ARRAY(dh) \
   (new(ALLOCTAG_PRIVATE) TypedScoreStream<SCORE_STREAM_BOOLEAN_TYPE>(1,0,ARRAY_FIFO_SIZE,USER_READ_STREAM,dh))
#define NEW_READ_SIGNED_FIXED_STREAM_DEPTH_HINT_ARRAY(i,f,dh) \
   (new(ALLOCTAG_PRIVATE) TypedScoreStream<SCORE_STREAM_SIGNED_FIXED_TYPE>(i,f,ARRAY_FIFO_SIZE,USER_READ_STREAM,dh))
#define NEW_READ_UNSIGNED_FIXED_STREAM_DEPTH_HINT_ARRAY(i,f,dh) \
   (new(ALLOCTAG_PRIVATE) TypedScoreStream<SCORE_STREAM_UNSIGNED_FIXED_TYPE>(i,f,ARRAY_FIFO_SIZE,USER_READ_STREAM,dh))

#define NEW_WRITE_SCORE_STREAM_DEPTH_HINT_ARRAY(dh) \
   (new(ALLOCTAG_PRIVATE) ScoreStream(64,0,ARRAY_FIFO_SIZE,SCORE_STREAM_UNTYPED,USER_WRITE_STREAM,dh))
#define NEW_WRITE_SIGNED_SCORE_STREAM_DEPTH_HINT_ARRAY(w,dh) \
   (new(ALLOCTAG_PRIVATE) TypedScoreStream<SCORE_STREAM_SIGNED_TYPE>(w,0,ARRAY_FIFO_SIZE,USER_WRITE_STREAM,dh))
#define NEW_WRITE_UNSIGNED_SCORE_STREAM_DEPTH_HINT_ARRAY(w,dh) \
   (new(ALLOCTAG_PRIVATE) TypedScoreStream<SCORE_STREAM_UNSIGNED_TYPE>(w,0,ARRAY_FIFO_SIZE,USER_WRITE_STREAM,dh))
#define NEW_WRITE_BOOLEAN_SCORE_STREAM_DEPTH_HINT_ARRAY(dh) \
   (new(ALLOCTAG_PRIVATE) TypedScoreStream<SCORE_STREAM_BOOLEAN_TYPE>(1,0,ARRAY_FIFO_SIZE,USER_WRITE_STREAM,dh))
#define NEW_WRITE_SIGNED_FIXED_STREAM_DEPTH_HINT_ARRAY(i,f,dh) \
   (new(ALLOCTAG_PRIVATE) TypedScoreStream<SCORE_STREAM_SIGNED_FIXED_TYPE>(i,f,ARRAY_FIFO_SIZE,USER_WRITE_STREAM,dh))
#define NEW_WRITE_UNSIGNED_FIXED_STREAM_DEPTH_HINT_ARRAY(i,f,dh) \
   (new(ALLOCTAG_PRIVATE) TypedScoreStream<SCORE_STREAM_UNSIGNED_FIXED_TYPE>(i,f,ARRAY_FIFO_SIZE,USER_WRITE_STREAM,dh))
/* end of hint_depth macros */


#define SCORE_MARKSTREAM(x,y) (x->setThreadCounter(y))
#define SCORE_MARKREADSTREAM(x,y) (x->setReadThreadCounter(y))
#define SCORE_MARKWRITESTREAM(x,y) (x->setWriteThreadCounter(y))

#define SCORE_UNMARKREADSTREAM(x) (x->unsetReadThreadCounter())
#define SCORE_UNMARKWRITESTREAM(x) (x->unsetWriteThreadCounter())

#define STREAM_BIND_SRC(x,y,z, zz) (x->stream_bind_src(y,z,zz))
#define STREAM_BIND_SINK(x,y,z, zz) (x->stream_bind_sink(y,z,zz))
#define STREAM_UNBIND_SRC(x) (x->stream_unbind_src())
#define STREAM_UNBIND_SINK(x) (x->stream_unbind_sink())

#define STREAM_SCHED_BIND_SRC(x,y,z, zz) (x->stream_sched_bind_src(y,z,zz))
#define STREAM_SCHED_BIND_SINK(x,y,z, zz) (x->stream_sched_bind_sink(y,z,zz))
#define STREAM_SCHED_UNBIND_SRC(x) (x->stream_sched_unbind_src())
#define STREAM_SCHED_UNBIND_SINK(x) (x->stream_sched_unbind_sink())

#define STREAM_READ_ARRAY(x) (x->stream_read_array()) 
#define STREAM_WRITE_ARRAY(x,y) (x->stream_write_array(y))

#define STREAM_READ_RT(x,y) \
  { union { unsigned long long l; ScoreSyncEvent* p; } helper; \
    helper.l = x->stream_read(); y = helper.p; }

#define STREAM_WRITE_RT(x,y) \
  { union { unsigned long long l; ScoreSyncEvent* p; } helper; \
    helper.p = y; x->stream_write(helper.l); }

#define STREAM_READ_NOACC(x) (x->stream_read()) 
#define STREAM_WRITE_NOACC(x,y) (x->stream_write(y))

#define STREAM_WRITE_DOUBLE(x,y) \
  { \
    double y_val=y;  \
    long long int* y_ptr = (long long int*) &y_val; \
    x->stream_write(*y_ptr); \
  }

#define STREAM_READ_DOUBLE(x) (x->stream_read_double())

#define MACRO_OVERHEAD 162
#define STREAM_WRITE(x,y) \
  { \
    unsigned long long ___t; \
    unsigned long long ___u; \
    unsigned long long ___b, ___e; \
    x->stream_write(y,0,___b); \
  }

#define STREAM_READ(x,y) \
  { \
    unsigned long long ___t; \
    unsigned long long ___u; \
    unsigned long long ___b, ___e; \
    y = x->stream_read(___b); \
  }


#define STREAM_FREE(x) (stream_free(x))
#define STREAM_CLOSE(x) (stream_close(x))
#define FRAME_CLOSE(x) (stream_frame_close(x))
#define STREAM_FREE_HW(x) (stream_free_hw(x))
#define STREAM_CLOSE_HW(x) (stream_close_hw(x))
#define STREAM_EOS_ARRAY(x) (x->stream_eos_array())
#define STREAM_EOS(x) (x->stream_eos())
#define STREAM_EOFR(x) (x->stream_eofr())
#define STREAM_DATA(x) (x->stream_data())
#define STREAM_DATA_ARRAY(x) (x->stream_data_array())
#define STREAM_FULL(x) (x->stream_full())
#define STREAM_FULL_ARRAY(x) (x->stream_full())
#define STREAM_EMPTY(x) (x->stream_empty())
#define STREAM_OBJ_TO_ID(x) (streamOBJ_to_ID(x))
#define STREAM_ID_TO_OBJ(x) (streamID_to_OBJ(x)) 
#define STREAM_TOKENS_PRODUCED(x) (x->get_stream_tokens_written())
#define STREAM_TOKENS_CONSUMED(x) (x->get_stream_tokens_read())
#define STREAM_NUMTOKENS(x) (x->get_numtokens())

typedef int SCORE_STREAM_ID;

#ifndef _SYS_SEM_BUF_H
#define _SYS_SEM_BUF_H  1

union semun {
  int val;
  struct semid_ds *buf;
  ushort *array;
};

struct ScoreToken {
  long long int token;
  long long unsigned timeStamp;
};

#endif /* _SYS_SEM_BUF_H */

class ScoreGraphNode;
class ScoreStreamType;
class ScoreOperator;
class ScorePage;
class ScoreSegment;

class ScoreStream {
  // !!!! KEEP THIS TAG FIRST IN CLASS !!!
  AllocationTag tag_copy1;

 public:
  void *operator new(size_t, AllocationTag = ALLOCTAG_SHARED);
  void operator delete(void*,size_t);
  ScoreStream();
  ScoreStream(int,int,int,ScoreType, unsigned int, int depth_hint = 0); 
    //width, fixed, length, type, user stream type
  ~ScoreStream();
  void stream_write(long long int, int writingEOS = 0, 
		    long long unsigned _cTime = 0);
  void stream_write_array(long long int, int writingEOS = 0); 
  long long int stream_read(long long unsigned _cTime = 0);
  double stream_read_double(long long unsigned _cTime = 0);

  long long int stream_read_array();

  /*
  long long int stream_read_array() {

    if (consumerFreed) {
      cerr << "SCORESTREAMERR: ATTEMPTING TO READ FROM A FREED STREAM!" 
	   << endl;
      exit(1);
    }

    read_local_buffer = buffer[head].token;

    head = (head+1) % (length+1+1);
    token_read++;

    // increment the sink node's input consumption.
    if (snkFunc != STREAM_OPERATOR_TYPE) {
      sink->incrementInputConsumption(snkNum);
    }

    return read_local_buffer;
  };
  */

  int stream_eos_array() {
    if (buffer[head].token == (long long int)EOS) {
      incrementInputConsumption();
      return 1;
    } 
    return 0;
  };

  void incrementInputConsumption();

  int stream_eos();
  int stream_eofr();
  int stream_data();
  int stream_data_any();
  int stream_data_array() {
    return (head != tail);
  };

  int stream_full() {
    if (VERBOSEDEBUG)
      cout << "[SID=" << streamID << "]  "
	   << "   Entering stream_full \n";

    // if the head pointer is ahead of the tail pointer by one,
    // then the stream is full and the fucntion will return 1

    if (!sinkIsDone) {
      return (((head - tail) == 2) ||
	      ((tail == ((length+1+1)-1-1)) && (head == 0)) ||
	      ((tail == (length+1+1)-1) && (head == 1)) ||
	      ((head - tail) == 1) ||
	      ((tail == (length+1+1)-1) && (head == 0)));
    } else {
      return(0);
    }
  };

  int stream_empty() {
  if (VERBOSEDEBUG)
    cout << "[SID=" << streamID << "]  "
         << "   Entering stream_empty \n";

  // if the head pointer is equal to the tail pointer,
  // then the stream is empty and the fucntion will return 1

  return (head == tail);
  };

  void stream_bind_src(ScoreGraphNode *srcNode, ScoreStreamType *srcType,
		       int newSrcFunc) {
    /* check src unbound to start with      *
     * fill in src slot w/ node given       *
     * check widths (types) for consistency */

    if (VERBOSEDEBUG || DEBUG) {
      cout << "[SID=" << streamID << "]  "
	   << "   stream_bind_src got called" << endl;
    }

    if (src != NULL) { 
      cerr << "stream_bind_src: src node not NULL" << endl;
      exit(1);
    } else {
      sched_src = src = srcNode;
      sched_srcFunc = srcFunc = newSrcFunc;
    }

//    cout << "this=" << this << endl;
  }

  void stream_bind_sink(ScoreGraphNode *sinkNode, ScoreStreamType *sinkType,
			int newSnkFunc) {
    /* check sink unbound to start with     *
     * fill in sink slot w/ node given      *
     * check widths (types) for consistency */

    if (VERBOSEDEBUG || DEBUG) {
      cout << "[SID=" << streamID << "]  "
	   << "   stream_bind_snk got called" << endl;
    }

    if (sink != NULL) {
      cerr << "stream_bind_sink: sink node not NULL" << endl;
      exit(1);
    } else {
      sched_sink = sink = sinkNode;
      sched_snkFunc = snkFunc = newSnkFunc;
    }
  }

  void stream_unbind_src() {
    if (VERBOSEDEBUG || DEBUG) {
      cout << "[SID=" << streamID << "]  "
	   << "   stream_unbind_src got called" << endl;
    }

    sched_src = src = NULL;
    sched_srcFunc = srcFunc = 0;
  }

  void stream_unbind_sink() {
    if (VERBOSEDEBUG || DEBUG) {
      cout << "[SID=" << streamID << "]  "
	   << "   stream_unbind_snk got called" << endl;
    }

    sched_sink = sink = NULL;
    sched_snkFunc = snkFunc = 0;
  }

  void stream_sched_bind_src(ScoreGraphNode *srcNode, 
			     ScoreStreamType *srcType,
			     int newSrcFunc) {
    /* check src unbound to start with      *
     * fill in src slot w/ node given       *
     * check widths (types) for consistency */

    if (VERBOSEDEBUG || DEBUG) {
      cout << "[SID=" << streamID << "]  "
	   << "   stream_sched_bind_src got called" << endl;
    }

    if (sched_src != NULL) { 
      cerr << "stream_sched_bind_src: src node not NULL" << endl;
      exit(1);
    } else {
      sched_src = srcNode;
      sched_srcFunc = newSrcFunc;
    }
  }

  void stream_sched_bind_sink(ScoreGraphNode *sinkNode, 
			      ScoreStreamType *sinkType,
			      int newSnkFunc) {
    /* check sink unbound to start with     *
     * fill in sink slot w/ node given      *
     * check widths (types) for consistency */

    if (VERBOSEDEBUG || DEBUG) {
      cout << "[SID=" << streamID << "]  "
	   << "   stream_sched_bind_snk got called" << endl;
    }

    if (sched_sink != NULL) {
      cerr << "stream_sched_bind_sink: sink node not NULL" << endl;
      exit(1);
    } else {
      sched_sink = sinkNode;
      sched_snkFunc = newSnkFunc;
    }
  }

  void stream_sched_unbind_src() {
    if (VERBOSEDEBUG || DEBUG) {
      cout << "[SID=" << streamID << "]  "
	   << "   stream_sched_unbind_src got called" << endl;
    }

    sched_src = NULL;
    sched_srcFunc = 0;
  }

  void stream_sched_unbind_sink() {
    if (VERBOSEDEBUG || DEBUG) {
      cout << "[SID=" << streamID << "]  "
	   << "   stream_sched_unbind_snk got called" << endl;
    }

    sched_sink = NULL;
    sched_snkFunc = 0;
  }

  int get_stream_tokens_eos() { return token_eos;}
  int get_stream_tokens_written() { return token_written;}
  int get_stream_tokens_read() { return token_read;}
  int get_length() {return length;}
  void set_length(int);
  int get_width() {return width;}
  int get_fixed() {return fixed;}

  int get_head() {return head;}
  int get_tail() {return tail;}
  
  int get_depth_hint() {return depth_hint;}

  ScoreType get_type() {return type;}
  int get_numtokens();
  char isGCable();
  unsigned long long stream_head_futuretime();

  void setReadThreadCounter(ScoreThreadCounter *ptr) {
    readThreadCounter = ptr;
  }
  void setWriteThreadCounter(ScoreThreadCounter *ptr) {
    writeThreadCounter = ptr;
  }
  void setThreadCounter(ScoreThreadCounter *ptr) {
    threadCounterPtr = ptr;
  }
  void unsetReadThreadCounter() {
    readThreadCounter = NULL;
  }
  void unsetWriteThreadCounter() {
    writeThreadCounter = NULL;
  }
  ScoreThreadCounter *getThreadCounterPtr() {return threadCounterPtr;}

  void syncSchedToReal();

  void print(FILE *f);
  void plot(std::ofstream *f);

  char* name;
  void setName(char* name_arg) {
  	name=(char*)malloc(strlen(name_arg)); 
	sprintf(name, "%s",name_arg);
//  	printf("Setting stream anme to %s\n",name);
  }
  char* getName() {
  	if(name==NULL) {
		printf("NULL stream name found\n");
	}
  	return name;
  }
  int streamID;
  int recycleID;
  int semid;
  union semun arg;
  int interProcess;
  int producerClosed;
  int consumerFreed;
  int producerClosed_hw;
  int consumerFreed_hw;
  ScoreGraphNode *src;
  ScoreGraphNode *sink;
  char isCrossCluster;
  struct sembuf acquire,release;
  int srcFunc;
  int snkFunc;
  int srcNum;
  int snkNum;
  char srcIsDone;
  char sinkIsDone;
  ScoreThreadCounter *readThreadCounter;
  ScoreThreadCounter *writeThreadCounter;
  sem_t sem_AVAIL_SLOTS, sem_TO_CONSUME, sem_DONE_MUTEX;

  //////////////////////////////////////////////////////
  // BEGIN SCHEDULER VARIABLES
  //////////////////////////////////////////////////////

  // used by the scheduler to indicate whether or not this is a stitch
  // stream or not.
  char sched_isStitch;

  // used by the scheduler during graph operations.
  char sched_visited;

  // used by scheduler during bufferlock detection to mark if this
  // stream buffer is full/empty. 
  // (so that any complex determination only happens once).
  char sched_isPotentiallyFull;
  char sched_isPotentiallyEmpty;

  // set by scheduler so that ScoreStream can return spare stitch streams to
  // the spare list.
  ScoreCustomStack<ScoreStream *> *sched_spareStreamStitchList;

  // this is the scheduler's version of the connection information.
  // these are copied over when a connection is made... this is to guarantee
  // that simulator connection information is not disturbed while running.
  ScoreGraphNode *sched_src;
  ScoreGraphNode *sched_sink;
  char sched_isCrossCluster;
  int sched_srcFunc;
  int sched_snkFunc;
  int sched_srcNum;
  int sched_snkNum;
  char sched_srcIsDone;
  char sched_sinkIsDone;

  // indicates if this is a processor<->array stream.
  char sched_isProcessorArrayStream;

  static int doneSemId;

  //////////////////////////////////////////////////////
  // END SCHEDULER VARIABLES  
  //////////////////////////////////////////////////////

  unsigned long long realTimeRead[10];
  unsigned long long threadTimeRead[10];
  unsigned long long realTimeWrite[10];
  unsigned long long threadTimeWrite[10];

  char sim_sinkOnStallQueue;
  char sim_haveCheckedSinkUnstallTime;

  ScoreStream *memoized_runtimePtr;

 protected:
  ScoreThreadCounter *threadCounterPtr;
  static int currentID;
  static int tempID;
  int width;
  int fixed;
  int depth_hint;
  int head, tail;
  int token_written, token_read, token_eos, token_eofr;
  enum {AVAIL_SLOTS, TO_CONSUME};
  ushort start_val[3];
  int length;
  ScoreType type;
  long long read_local_buffer;

  // !!!! KEEP THIS ARRAY LAST IN CLASS!!!!
  struct ScoreToken buffer[ARRAY_FIFO_SIZE+1+1];
};

SCORE_STREAM_ID streamOBJ_to_ID(ScoreStream *);
ScoreStream *streamID_to_OBJ(SCORE_STREAM_ID);
void stream_free(ScoreStream *);
void stream_close(ScoreStream *);
void stream_frame_close(ScoreStream *);
void stream_free_hw(ScoreStream *);
void stream_close_hw(ScoreStream *);
void stream_gc(ScoreStream *);

template <ScoreType ScoreType_t>
class TypedScoreStream : public ScoreStream
{
 public:
  TypedScoreStream(int width_t_, int fixed_t_, int length_t_,
		   unsigned int usr_stream_type, int depth_hint_t_ = 0) :
    ScoreStream(width_t_, fixed_t_, length_t_,
		ScoreType_t, usr_stream_type, depth_hint_t_) 
    {}
};

typedef ScoreStream* SCORE_STREAM;
typedef TypedScoreStream<SCORE_STREAM_FLOAT_TYPE>* FLOAT_SCORE_STREAM;
typedef TypedScoreStream<SCORE_STREAM_DOUBLE_TYPE>* DOUBLE_SCORE_STREAM;
typedef TypedScoreStream<SCORE_STREAM_BOOLEAN_TYPE>* BOOLEAN_SCORE_STREAM;
typedef TypedScoreStream<SCORE_STREAM_SIGNED_TYPE>* SIGNED_SCORE_STREAM;
typedef TypedScoreStream<SCORE_STREAM_UNSIGNED_TYPE>* UNSIGNED_SCORE_STREAM;
typedef TypedScoreStream<SCORE_STREAM_SIGNED_FIXED_TYPE>* SIGNED_FIXED_STREAM;
typedef TypedScoreStream<SCORE_STREAM_UNSIGNED_FIXED_TYPE>* UNSIGNED_FIXED_STREAM;


// needed by LEDA for use with lists/etc.
int compare(ScoreStream * const & left, ScoreStream * const & right);



extern "C" {
// as streams are created, this variable will store the pointers
// to all streams that must be expanded
#ifndef STREAM_MAIN_FILE
extern
#endif
list<ScoreStream* > *streamsToExpand;
}

#endif
