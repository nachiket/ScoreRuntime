#ifndef _ScoreStream_H
#define _ScoreStream_H

#include <sys/types.h>
#include <semaphore.h>
#include "xilscoretype.h"
#include "xilscorenode.h"

// Depth if not defined..
#define DEFAULT_SIZE 256
#define ARRAY_FIFO_SIZE 256

// LLU suffix for C99 support in C++ and GNU compilers! Allah is great! Allahu akbar!
#define EOS 0xffffffffffffffffLLU
#define EOFR 0xefffffffffffffffLLU

//----------------------------------------------------
//			CONSTRUCTORS
//----------------------------------------------------
// **********no depth_hint*********** 
#define NEW_SCORE_STREAM() 		(new ScoreStream(64,0,DEFAULT_SIZE,SCORE_STREAM_UNTYPED,NOT_USER_STREAM))
#define NEW_SIGNED_SCORE_STREAM(w) 	(new TypedScoreStream<SCORE_STREAM_SIGNED_TYPE>(w,0,DEFAULT_SIZE,NOT_USER_STREAM))
#define NEW_UNSIGNED_SCORE_STREAM(w) 	(new TypedScoreStream<SCORE_STREAM_UNSIGNED_TYPE>(w,0,DEFAULT_SIZE,NOT_USER_STREAM))
#define NEW_BOOLEAN_SCORE_STREAM() 	(new TypedScoreStream<SCORE_STREAM_BOOLEAN_TYPE>(1,0,DEFAULT_SIZE,NOT_USER_STREAM))
#define NEW_FLOAT_SCORE_STREAM() 	(new TypedScoreStream<SCORE_STREAM_FLOAT_TYPE>(32,0,DEFAULT_SIZE,NOT_USER_STREAM))
#define NEW_DOUBLE_SCORE_STREAM() 	(new TypedScoreStream<SCORE_STREAM_DOUBLE_TYPE>(64,0,DEFAULT_SIZE,NOT_USER_STREAM))
// ********depth_hint macros********* 
#define NEW_SCORE_STREAM_DEPTH_HINT(dh) 		(new ScoreStream(64,0,DEFAULT_SIZE,SCORE_STREAM_UNTYPED,NOT_USER_STREAM,dh))
#define NEW_SIGNED_SCORE_STREAM_DEPTH_HINT(w,dh) 	(new TypedScoreStream<SCORE_STREAM_SIGNED_TYPE>(w,0,DEFAULT_SIZE,NOT_USER_STREAM,dh))
#define NEW_UNSIGNED_SCORE_STREAM_DEPTH_HINT(w,dh) 	(new TypedScoreStream<SCORE_STREAM_UNSIGNED_TYPE>(w,0,DEFAULT_SIZE,NOT_USER_STREAM,dh))
#define NEW_BOOLEAN_SCORE_STREAM_DEPTH_HINT(dh) 	(new TypedScoreStream<SCORE_STREAM_BOOLEAN_TYPE>(1,0,DEFAULT_SIZE,NOT_USER_STREAM, dh))
#define NEW_FLOAT_SCORE_STREAM_DEPTH_HINT(dh) 		(new TypedScoreStream<SCORE_STREAM_FLOAT_TYPE>(32,0,DEFAULT_SIZE,NOT_USER_STREAM, dh))
#define NEW_DOUBLE_SCORE_STREAM_DEPTH_HINT(dh) 		(new TypedScoreStream<SCORE_STREAM_DOUBLE_TYPE>(64,0,DEFAULT_SIZE,NOT_USER_STREAM, dh))

//----------------------------------------------------
//			CONNECTIONS
//----------------------------------------------------
#define STREAM_BIND_SRC(x,y,z, zz) (x->stream_bind_src(y,z,zz))
#define STREAM_BIND_SINK(x,y,z, zz) (x->stream_bind_sink(y,z,zz))
#define STREAM_UNBIND_SRC(x) (x->stream_unbind_src())
#define STREAM_UNBIND_SINK(x) (x->stream_unbind_sink())

//----------------------------------------------------
//		READ-WRITE OPERATIONS
//----------------------------------------------------
#define STREAM_READ_NOACC(x) (x->stream_read()) 
#define STREAM_WRITE_NOACC(x,y) (x->stream_write(y))

#define STREAM_READ_BOOL(x) (x->stream_read_bool()) 
#define STREAM_WRITE_BOOL(x,y) (x->stream_write_bool(y))

#define STREAM_READ_INT(x) (x->stream_read_int()) 
#define STREAM_WRITE_INT(x,y) (x->stream_write_int(y))

#define STREAM_WRITE_DOUBLE(x,y) (x->stream_write_double(y))
#define STREAM_WRITE_FLOAT(x,y) (x->stream_write_float(y))

#define STREAM_READ_DOUBLE(x) (x->stream_read_double())
#define STREAM_READ_FLOAT(x) (x->stream_read_float())

//----------------------------------------------------
//			STATUS
//----------------------------------------------------
#define STREAM_FREE(x) (stream_free(x))
#define STREAM_CLOSE(x) (stream_close(x))
#define FRAME_CLOSE(x) (stream_frame_close(x))
#define STREAM_EOS(x) (x->stream_eos())
#define STREAM_EOFR(x) (x->stream_eofr())
#define STREAM_FULL(x) (x->stream_full())
#define STREAM_EMPTY(x) (x->stream_empty())
#define STREAM_TOKENS_PRODUCED(x) (x->get_stream_tokens_written())
#define STREAM_TOKENS_CONSUMED(x) (x->get_stream_tokens_read())
#define STREAM_NUMTOKENS(x) (x->get_numtokens())

#ifndef _SYS_SEM_BUF_H
#define _SYS_SEM_BUF_H  1

#endif /* _SYS_SEM_BUF_H */

class ScoreGraphNode;
class ScoreStreamType;

class ScoreStream {

 public:
  // Constructors..
  void *operator new(size_t);
  void operator delete(void*,size_t);
  ScoreStream();
  ScoreStream(int,int,int,ScoreType, unsigned int, int depth_hint = 0); 
              //width, fixed, length, type, user stream type
  ~ScoreStream();

  // Read-Write operations
  void stream_write(long long int, int writingEOS = 0);
  void stream_write_bool(bool, int writingEOS=0);
  void stream_write_int(int, int writingEOS=0);
  void stream_write_float(float, int writingEOS=0);
  void stream_write_double(double, int writingEOS=0);
  long long int stream_read();
  bool stream_read_bool();
  int stream_read_int();
  float stream_read_float();
  double stream_read_double();

  // Status indications
  int stream_eos();
  int stream_eofr();
  int stream_data_any();

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

  int get_stream_tokens_eos() { return token_eos;}
  int get_stream_tokens_written() { return token_written;}
  int get_stream_tokens_read() { return token_read;}
  int get_length() {return length;}
  void set_length(int);
  int get_width() {return width;}
  int get_fixed() {return fixed;}
  
  int get_depth_hint() {return depth_hint;}

  ScoreType get_type() {return type;}
  int get_numtokens();

  int streamID;
  int producerClosed;
  int consumerFreed;
  ScoreGraphNode *src;
  ScoreGraphNode *sink;
  struct sembuf acquire,release;
  int srcFunc;
  int snkFunc;
  int srcNum;
  int snkNum;
  char srcIsDone;
  char sinkIsDone;

 protected:

  // crucial for proper operation and implementation
  int depth_hint;
  int head, tail;

  // what are these?
  int width;
  int length;
  int fixed;

  // statistics?
  int token_written, token_read, token_eos, token_eofr;
  enum {AVAIL_SLOTS, TO_CONSUME};
  ushort start_val[3];
  ScoreType type;

  // have to figure out how to select between one of these..
  bool bool_buffer[ARRAY_FIFO_SIZE+1+1];
  int int_buffer[ARRAY_FIFO_SIZE+1+1];
  float float_buffer[ARRAY_FIFO_SIZE+1+1];
  double double_buffer[ARRAY_FIFO_SIZE+1+1]; // LOL@double_buffer
};

void stream_free(ScoreStream *);
void stream_close(ScoreStream *);
void stream_frame_close(ScoreStream *);


template <ScoreType ScoreType_t>
class TypedScoreStream : public ScoreStream
{
 public:
  TypedScoreStream(int width_t_, int fixed_t_, int length_t_, unsigned int usr_stream_type, int depth_hint_t_ = 0) :
    ScoreStream(width_t_, fixed_t_, length_t_, ScoreType_t, usr_stream_type, depth_hint_t_) 
    {}
};

typedef ScoreStream* SCORE_STREAM;
typedef TypedScoreStream<SCORE_STREAM_FLOAT_TYPE>* FLOAT_SCORE_STREAM;
typedef TypedScoreStream<SCORE_STREAM_DOUBLE_TYPE>* DOUBLE_SCORE_STREAM;
typedef TypedScoreStream<SCORE_STREAM_BOOLEAN_TYPE>* BOOLEAN_SCORE_STREAM;
typedef TypedScoreStream<SCORE_STREAM_SIGNED_TYPE>* SIGNED_SCORE_STREAM;
typedef TypedScoreStream<SCORE_STREAM_UNSIGNED_TYPE>* UNSIGNED_SCORE_STREAM;

