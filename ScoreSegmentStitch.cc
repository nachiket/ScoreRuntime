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
// $Revision: 1.37 $
//
//////////////////////////////////////////////////////////////////////////////

#include <stdlib.h>
#include <signal.h>
#include "ScoreSegmentStitch.h"
#include "ScoreConfig.h"


#define INSTREAM in[0]
#define OUTSTREAM out[0]


void *ScoreSegmentStitch::operator new(size_t size) {
  return((void *) malloc(size));
}


// This constructor is used by the scheduler to create a ScoreSegment
// used for stitch buffering. This will not be shared among processes so
// does not have to have a shared memory segment.
ScoreSegmentStitch::ScoreSegmentStitch(int nlength, int nwidth, 
                                       ScoreStream* in_t, ScoreStream* out_t) {
  segmentType = SCORE_SEGMENT_STITCH;

  segLength = nlength;
  segWidth = nwidth;
  segSize = segLength * 8; // make everything long long integer sized for now.

  // create a data memory block.
  dataPtr = (void *) malloc(segSize);

  // set the page inputs and outputs 
  declareIO(1,1);
  if (in_t != NULL) {
    bindSchedInput(SCORE_CMB_STITCH_DATAW_INNUM,
	           in_t,new ScoreStreamType(0,16));
  } else {
    in_types[SCORE_CMB_STITCH_DATAW_INNUM] = new ScoreStreamType(0,16);
  }
  if (out_t != NULL) {
    bindSchedOutput(SCORE_CMB_STITCH_DATAR_OUTNUM,
	            out_t,new ScoreStreamType(0,16));
  } else {
    out_types[SCORE_CMB_STITCH_DATAR_OUTNUM] = new ScoreStreamType(0,16);
  }

  sched_stitchBufferNum = 0;
  sched_prevStitch = NULL;
  sched_nextStitch = NULL;
  hasReceivedEOS = 0;
  sched_isStitch = 1;
  sched_isNewStitch = 1;
  sched_isOnlyFeedProcessor = 0;

  // Finally, initialize variables
  readAddr = 0;
  writeAddr = 0;

  mode = SCORE_CMB_SEQSRCSINK;
  sched_mode = SCORE_CMB_SEQSRCSINK;
  sched_old_mode = SCORE_CMB_SEQSRCSINK;

  // set the input mask.
  sim_segmentInputMask = 0;

  input_freed = 0;
  output_closed = 0;

  sched_mustBeInDataFlow = 0;
  sched_isEmptyAndWillBeRemoved = 0;

#ifdef RANDOM_SCHEDULER
  sched_lastTimesliceConfigured = (unsigned int)(-2) ;
  sched_residentStitchListItem = SCORECUSTOMLINKEDLIST_NULL;
#endif
}


ScoreSegmentStitch::~ScoreSegmentStitch() {
  free(dataPtr);
  dataPtr = NULL;
}


int ScoreSegmentStitch::step() {
  char isEmpty, isFull;
  
  sim_isFaulted = 0;

  if (this_segment_is_done) {
    return(0);
  }

  isEmpty = (writeAddr == readAddr);
  isFull = 
    ((writeAddr == (readAddr-1)) ||
     ((writeAddr == (segLength-1)) &&
      (readAddr == 0)));

  if (mode == SCORE_CMB_SEQSINK) {

    // if this stitch has signalled done, and it is a sink
    // then nothing can be done, until it is drained
    // (i.e. mode is switched to SEQSRCSINK)
    if (hasReceivedEOS)
      return 1;

    int address;
    long long int data, *atable=(long long int *)dataPtr;

    if (STREAM_DATA(INSTREAM)) {
      if (STREAM_EOS_ARRAY(INSTREAM)) {
	if (VERBOSEDEBUG || DEBUG || EXTRA_DEBUG) {
	  cerr << "   STITCHSEG SEQSINK: [" << (unsigned int) this <<
	    "] received EOS input" << endl;
	}

	hasReceivedEOS = 1;

	fire++;

        if (!input_freed) {
	  STREAM_FREE(INSTREAM);
          input_freed = 1;
        }

	// do NOT set this to done, since we can not close
	// the output at this point
        this_segment_is_done = 0;

	// this stitch is not done yet, it still must close the output stream,
	// therefore, do not tell scheduler that it is done
	return(1);
      } else {
	address = writeAddr;
	
	if (VERBOSEDEBUG || DEBUG) {
	  cerr << "   STITCHSEG SEQSINK: [" << (unsigned int) this << 
	    "] firing - address is " << address << endl;
	}
	
	// check if the stitch buffer is full.
	if (isFull) {
	  sim_isFaulted = 1;
	  
	  if (EXTRA_DEBUG) {
	    cerr << "   STITCHSEG SEQSINK: [" << (unsigned int) this <<
	      "] is FULL and faulted\n";
	  }

	  if (VERBOSEDEBUG) {
	    cerr << "   STITCHSEG SEQSINK: full!" << endl;
	  }

	  stall++;

	  sim_segmentInputMask = 0;

	  return(1);
	}

	// write data to memory.
	data = STREAM_READ_ARRAY(INSTREAM);
	if (VERBOSEDEBUG || DEBUG) {
	  cerr << "   STITCHSEG SEQSINK: data is " << data << endl;
	}
	atable[address] = data;
	writeCount++;
	fire++;
	
	// increment writeAddr.
	writeAddr++;
	writeAddr = writeAddr%segLength;
      }
    } else {
      if (VERBOSEDEBUG) {
	cerr << "   STITCHSEG SEQSINK: not firing!" << endl;
      }

      stall++;

      sim_segmentInputMask = (1<<SCORE_CMB_STITCH_DATAW_INNUM);
    }
  } else if (mode == SCORE_CMB_SEQSRCSINK) {
    int address;
    long long int data, *atable=(long long int *)dataPtr;

    // if this stitch buffer previously received an EOS token and is currently
    // empty then output an EOS on the output.
    if (hasReceivedEOS && isEmpty) {
      if (VERBOSEDEBUG || DEBUG || EXTRA_DEBUG) {
	cerr << "   STITCHSEG SEQSRCSINK: [" << (unsigned int) this <<
	  "] having emptied the buffer and " <<
	  "previously received an EOS, firing EOS to output" << endl;
      }
      fire++;
      
      if (!output_closed) {
        STREAM_CLOSE(OUTSTREAM);
        output_closed = 1;
      }

      this_segment_is_done = 1;

      return(0);
    } else {
      char didFire = 0;

      if (!STREAM_FULL(OUTSTREAM)) {
	if (!isEmpty) {
	  address = readAddr;
	  
	  if (VERBOSEDEBUG || DEBUG) {
	    cerr << "   STITCHSEG SEQSRCSINK: firing - read address is " << 
	      address << endl;
	  }
	  
	  // writing data to OUTSTREAM.
	  data = atable[address];
	  readCount++;
	  if (VERBOSEDEBUG || DEBUG) {
	    cerr << "   STITCHSEG SEQSRCSINK: firing - data is " << data << 
	      endl;
	  }
	  STREAM_WRITE_ARRAY(OUTSTREAM, data);
	  didFire = 1;
	  
	  // increment readAddr.
	  readAddr++;
	  readAddr = readAddr%segLength;
	} else {
	  if (VERBOSEDEBUG) {
	    cerr << "   STITCHSEG SEQSRCSINK: empty!" << endl;
	  }
	}
      }

      if (!hasReceivedEOS) {
	if (STREAM_DATA(INSTREAM)) {
	  if (STREAM_EOS_ARRAY(INSTREAM)) {
	    if (VERBOSEDEBUG || DEBUG || EXTRA_DEBUG) {
	      cerr << "   STITCHSEG SEQSRCSINK: [" << (unsigned int) this <<
		"] received EOS input" << endl;
	    }

	    hasReceivedEOS = 1;
	    
            if (!input_freed) {
	      STREAM_FREE(INSTREAM);
              input_freed = 1;
            }
	    
	    if (isEmpty) {
	      if (VERBOSEDEBUG || DEBUG || EXTRA_DEBUG) {
		cerr << "   STITCHSEG SEQSRCSINK: [" << (unsigned int) this <<
		  "] having emptied the " <<
		  "buffer and received an EOS, firing EOS to output" << endl;
	      }
	      fire++;
	      
              if (!output_closed) { 
	        STREAM_CLOSE(OUTSTREAM);
                output_closed = 1;
              }
	      
              this_segment_is_done = 0;

	      return(0);
	    }
	  } else {
	    if (!isFull) {
	      address = writeAddr;
	      
	      if (VERBOSEDEBUG || DEBUG) {
		cerr << "   STITCHSEG SEQSRCSINK: firing - write " <<
		  "address is " << address << endl;
	      }
	      
	      // read data
	      data = STREAM_READ_ARRAY(INSTREAM);
	      if (VERBOSEDEBUG || DEBUG) {
		cerr << "   STITCHSEG SEQSRCSINK: data is " << data << endl;
	      }
	      atable[address] = data;
	      writeCount++;
	      didFire = 1;
	      
	      // increment writeAddr.
	      writeAddr++;
	      writeAddr = writeAddr%segLength;
	    } else {
	      if (VERBOSEDEBUG) {
		cerr << "   STITCHSEG SEQSRCSINK: full! " <<
		  (unsigned int) this << endl;
	      }	    
	    }
	  }
	}
      }

      if (didFire) {
	fire++;
      } else {
	stall++;

	if (isFull) {
	  sim_segmentInputMask = 0;
	} else {
	  sim_segmentInputMask = (1<<SCORE_CMB_STITCH_DATAW_INNUM);
	}
      }

      if (isFull) {
	sim_isFaulted = 1;
	
	return(1);
      }
    }
  } else {
    cerr << "SCORESEGMENTSTITCHERR: ScoreSegmentStitch cannot be in mode " <<
      mode << "!" << endl;

    return(1);
  }

  return(1);
}


// required by LEDA for use with lists/etc.

// provides comparison operation between ScoreSegmentStitch.
// NOTE: Right now, we only say 2 stitch buffers are equal if their pointers
//       are equal. Otherwise, less than/greater than is determined
//       simply by their pointer values.
int compare(ScoreSegmentStitch * const & left, 
            ScoreSegmentStitch * const & right) {
  if (left == right) {
    return(0);
  } else if (left < right) {
    return(-1);
  } else {
    return(1);
  }
}

