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
// $Revision: 1.56 $
//
//////////////////////////////////////////////////////////////////////////////
#ifndef _ScoreSegment_H

#define _ScoreSegment_H

#include <signal.h>
#include <errno.h>
#include <sys/ipc.h>
#include <sys/sem.h>
//#include <sys/sem_buf.h>
#include <sys/shm.h>
#include <sys/mman.h> /* for mprotect call */
#include <limits.h> /* for the page size limit */
#include "ScoreGraphNode.h"
#include "ScoreSegmentTable.h"
#include <unistd.h>
#include "ScoreType.h"

#define NOCHECK 0

#define SCORE_SEGMENT_ID int
#define SEG_USECOUNT 0
#define OBJECT_SIZE 400
#define NUMOFSHAREDSEG 126 // this is a current system limit

#ifndef PAGESIZE
#define PAGESIZE 4096
#endif


// the various modes a segment can be in.
#define SCORE_CMB_SEQSRC           0
#define SCORE_CMB_SEQSINK          1
#define SCORE_CMB_SEQSRCSINK       2
#define SCORE_CMB_RAMSRC           3
#define SCORE_CMB_RAMSINK          4
#define SCORE_CMB_RAMSRCSINK       5

// for each of the various modes, this tells which input and output numbers
// the logical streams correspond to.
#define SCORE_CMB_SEQSRC_DATA_OUTNUM             0
#define SCORE_CMB_SEQSINK_DATA_INNUM             0
#define SCORE_CMB_SEQSRCSINK_DATAW_INNUM         0
#define SCORE_CMB_SEQSRCSINK_DATAR_OUTNUM        0
#define SCORE_CMB_RAMSRC_ADDR_INNUM              0
#define SCORE_CMB_RAMSRC_DATA_OUTNUM             0
#define SCORE_CMB_RAMSINK_ADDR_INNUM             0
#define SCORE_CMB_RAMSINK_DATA_INNUM             1
#define SCORE_CMB_RAMSRCSINK_ADDR_INNUM          0
#define SCORE_CMB_RAMSRCSINK_DATAW_INNUM         1
#define SCORE_CMB_RAMSRCSINK_WRITE_INNUM         2
#define SCORE_CMB_RAMSRCSINK_DATAR_OUTNUM        0
#define SCORE_CMB_STITCH_DATAW_INNUM             0
#define SCORE_CMB_STITCH_DATAR_OUTNUM            0

// the various types of ScoreSegment derivations so that the delete
// operator knows what to do.
#define SCORE_SEGMENT_PLAIN        1
#define SCORE_SEGMENT_READONLY     2
#define SCORE_SEGMENT_STITCH       3


// ScoreSegment: a memory segment.
class ScoreSegment : public ScoreGraphNode {
public:

  void *dataPtr;
  // used to store mode/bounds information.
  int mode;
  unsigned int maxAddr;             // this is the size of the loaded
                                    // segment block.
                                    // (address based on token width).
  unsigned int traAddr;             // this is the lower bound virtual
                                    // address for the loaded segment block.
                                    // (address based on token width).
  unsigned int pboAddr;             // this is the lower bound physical
                                    // address for the loaded segment block.
                                    // (address based on bytes).
  unsigned int readAddr;
  unsigned int writeAddr;
  unsigned int readCount;
  unsigned int writeCount;

  char this_segment_is_done;

  struct sembuf incrementuse, waitfornouse, decrementuse;
  int semid; // made static by Nachiket 1/16/2010
  union semun arg;
  static void *dataPtrTable[NUMOFSHAREDSEG];
  static void *dataRangeTable[NUMOFSHAREDSEG];
  static ScoreSegment *segPtrTable[NUMOFSHAREDSEG];

  unsigned int segSize;
  int dataID;
  size_t segLength;
  unsigned int segWidth;
  SCORE_SEGMENT_ID segmentID;

  char sim_isFaulted;
  unsigned int sim_faultedAddr;
  unsigned int segmentType;

  unsigned int getSegmentType() {return(segmentType);}
  ScoreType segType;

  int copyIn, copyOut;


  //////////////////////////////////////////////////////
  // BEGIN SCHEDULER VARIABLES
  //////////////////////////////////////////////////////

  // used to store the scheduler version of the  mode/bounds information.
  int sched_mode;
  unsigned int sched_maxAddr;
  unsigned int sched_traAddr;
  unsigned int sched_pboAddr;
  unsigned int sched_readAddr;
  unsigned int sched_writeAddr;
  unsigned int sched_readCount;
  unsigned int sched_writeCount;

  char sched_this_segment_is_done;

  int sched_old_mode;

  // indicates whether this segment is a stitch buffer or not.
  char sched_isStitch;

  // stores (for scheduler) whether or not the segment has faulted (and if
  // so, which address caused the fault).
  char sched_isFaulted;
  unsigned int sched_faultedAddr;

  // stores the pointers to the memory segment block for the data and
  // state.
  // FIX ME! REALLY SHOULD ALLOW MULTIPLE CACHING OF DATA AND STATE!
  // FOR NOW, WE ASSUME THAT EACH SEGMENT IS STORED IN A CMB ONLY WHEN IT
  // IS RESIDENT (ONLY IN ONE PLACE), AND IS UNCACHED WHEN SEGMENT IS UNLOADED!
  // dumpSegmentBlock is there so that a segment still knows where to dump its
  //   resident fifo even if it is getting evicted.
  ScoreSegmentBlock *sched_cachedSegmentBlock;
  ScoreSegmentBlock *sched_dumpSegmentBlock;

  // provides a place to store the dumped FIFO information.
  void *sched_fifoBuffer;
  char sched_isFIFOBufferValid;

  // indicates if this needs to be dumped when this segment is marked done.
  char sched_dumpOnDone;

  // indicates the starting address and length of the resident section of the
  // segment.
  unsigned int sched_residentStart;
  unsigned int sched_residentLength;


  //////////////////////////////////////////////////////
  // END SCHEDULER VARIABLES  
  //////////////////////////////////////////////////////
  int recycleID1;

  ScoreIOMaskType sim_segmentInputMask;

  char shouldUseUSECOUNT;
  int recycleID0; // same as tempID, need to recycle this on exit

  char checkIfAddrFault(unsigned int newAddr);

  static int tempID;
  static SCORE_SEGMENT_ID currentID;
  static int initSig; // variable used to initialize initSigCatch only once

  static ScoreSegment *shmptr;
  ScoreSegment *segPtr;
  ushort start_val[1];

  void *operator new(size_t size);
  void operator delete(void *p, size_t size);
  ScoreSegment();
  ScoreSegment(int, int, ScoreType); //first arg is lenth, second arg is width

  // should return a pointer to data in segment
  void* data() {
//  printf("Case size of this %d\n",sizeof(*this));
//  printf("returning hiy: %d %lu,  %lu\n",hiy,&hiy,dataPtr);
    return(dataPtr);
  }

  // set data
  void setData(int address, long long int data) {
    ((long long int*)dataPtr)[address]=data;
  }

  // length of segment 
  // to humor Eylon, this means number of elements
  size_t length() {
    return(segLength);
  }

  // data width
  int width() {
    return(segWidth);
  }

  // size in bytes.
  int size() {
    return(segSize);
  }

  long MORON;
  SCORE_SEGMENT_ID id() {return segmentID;}  // return id (like ScoreStream)
  void noAccess() ; // remove access from parent process
                    //  maybe others, maybe should take args
  void returnAccess(); // put access back

  ~ScoreSegment();

  virtual int step() {return(0);}// should be overridden

  virtual NodeTags getTag() { return ScoreSegmentTag; }

};

//public
template <ScoreType ScoreType_t>
class TypedScoreSegment : public ScoreSegment
{
 public:
  TypedScoreSegment(int nlength_, int nwidth_) :
    ScoreSegment(nlength_, nwidth_, ScoreType_t) 
    {}
};

typedef TypedScoreSegment<SCORE_STREAM_UNTYPED>* SCORE_SEGMENT;
typedef TypedScoreSegment<SCORE_STREAM_BOOLEAN_TYPE>* BOOLEAN_SCORE_SEGMENT;
typedef TypedScoreSegment<SCORE_STREAM_SIGNED_TYPE>* SIGNED_SCORE_SEGMENT;
typedef TypedScoreSegment<SCORE_STREAM_UNSIGNED_TYPE>* UNSIGNED_SCORE_SEGMENT;
typedef TypedScoreSegment<SCORE_STREAM_SIGNED_FIXED_TYPE>* SIGNED_FIXED_SCORE_SEGMENT;
typedef TypedScoreSegment<SCORE_STREAM_UNSIGNED_FIXED_TYPE>* UNSIGNED_FIXED_SCORE_SEGMENT;
typedef TypedScoreSegment<SCORE_STREAM_DOUBLE_TYPE>* DOUBLE_SCORE_SEGMENT;
typedef TypedScoreSegment<SCORE_STREAM_FLOAT_TYPE>* FLOAT_SCORE_SEGMENT;

#define NEW_SCORE_SEGMENT(n,w) \
   (new TypedScoreSegment<SCORE_STREAM_UNTYPED>(n,w))
#define NEW_SIGNED_SCORE_SEGMENT(n,w) \
   (new TypedScoreSegment<SCORE_STREAM_SIGNED_TYPE>(n,w))
#define NEW_UNSIGNED_SCORE_SEGMENT(n,w) \
   (new TypedScoreSegment<SCORE_STREAM_UNSIGNED_TYPE>(n,w))
#define NEW_BOOLEAN_SCORE_SEGMENT(n) \
   (new TypedScoreSegment<SCORE_STREAM_BOOLEAN_TYPE>(n,1))
#define NEW_SIGNED_FIXED_SCORE_SEGMENT(n,w) \
   (new TypedScoreSegment<SCORE_STREAM_SIGNED_FIXED_TYPE>(n,w))
#define NEW_UNSIGNED_FIXED_SCORE_SEGMENT(n,w) \
   (new TypedScoreSegment<SCORE_STREAM_UNSIGNED_FIXED_TYPE>(n,w))
#define NEW_DOUBLE_SCORE_SEGMENT(n) \
   (new TypedScoreSegment<SCORE_STREAM_DOUBLE_TYPE>(n))
#define NEW_FLOAT_SCORE_SEGMENT(n) \
   (new TypedScoreSegment<SCORE_STREAM_FLOAT_TYPE>(n))

#define SEGMENT_OBJ_TO_ID(x) (segmentOBJ_to_ID(x))
#define SEGMENT_ID_TO_OBJ(x) (segmentID_to_OBJ(x))

#define GET_SEGMENT_DATA(x) (x->data())

#define SEGMENT_WRITE_DOUBLE(x,y,z) \
  { \
    unsigned long long* data = (unsigned long long*)x->data(); \
    data[z]=(long long int)y; \
  }

#define SEGMENT_WRITE(x,y,z) \
  { \
    unsigned long long* data = (unsigned long long*)x->data(); \
    data[z]=(long long int)y; \
  }

int initSigCatch();
void catchSig(int, siginfo_t *, void *);

SCORE_SEGMENT_ID segmentOBJ_to_ID(ScoreSegment *);
ScoreSegment *segmentID_to_OBJ(int nid);

// needed by LEDA for use with lists/etc.
int compare(ScoreSegment * const & left, ScoreSegment * const & right);

#endif

