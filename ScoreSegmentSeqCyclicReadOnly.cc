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
// $Revision: 1.15 $
//
//////////////////////////////////////////////////////////////////////////////

#include <stdlib.h>
#include "ScoreSegmentSeqCyclicReadOnly.h"
#include "ScoreConfig.h"


#define DATASTREAM out[SCORE_CMB_SEQSRC_DATA_OUTNUM]


void *ScoreSegmentSeqCyclicReadOnly::operator new(size_t size) {
  return((void *) malloc(size));
}


void ScoreSegmentSeqCyclicReadOnly::constructorHelper(unsigned int dwidth, 
                                                unsigned int awidth,
					        unsigned int nelem,
					        ScoreSegment* segPtr, 
					        ScoreStream* data_t) {
  segmentType = SCORE_SEGMENT_READONLY;

  if (VERBOSEDEBUG || DEBUG) {
    cout << "attach address is: " << segPtr << endl;
  }

  // Second, attach the data segment to this process 
   while ((dataPtr=(void *)shmat(segPtr->dataID, 0, 0))==(void *) -1) {
      perror("dataPtr -- seg constructor helper -- attach ");
      if (errno != EINTR)
	exit(errno);
   }


//   cout << "dataPtr inside Segment=" << dataPtr << endl;

  // Third, copy some data over
  segLength = segPtr->segLength;
  readAddr = 0;
  writeAddr = 0;

  if (nelem != NOCHECK) // sanity check
    if (segLength != nelem) {
      perror("nelem -- seg constructor helper -- not agree with segLength ");
      exit(errno);
    }

  segWidth = segPtr->segWidth;

  if (dwidth != NOCHECK) // sanity check
    if (segWidth != dwidth) {
      perror("dwidth -- seg constructor helper -- not agree with segWidth ");
      exit(errno);
    }
      
  segSize = segPtr->segSize;
  semid = segPtr->semid;

  // Fourth, set the page inputs and outputs 
  declareIO(0,1);
  bindOutput(SCORE_CMB_SEQSRC_DATA_OUTNUM,
	     data_t,new ScoreStreamType(0,dwidth));

  // Finally, initialize variables
  mode = SCORE_CMB_SEQSRC;
  sched_mode = SCORE_CMB_SEQSRC;
  sched_old_mode = SCORE_CMB_SEQSRC;

  long long int *atable = (long long int *)dataPtr;

  if (VERBOSEDEBUG) {
    for (unsigned int index = 0; index < segLength; index++) {
      printf("   SEG SEQSRC: data in %d is %Ld\n", index, atable[index]);
    }
  }

  // set the input mask.
  sim_segmentInputMask = 0;

  shouldUseUSECOUNT = segPtr->shouldUseUSECOUNT;
}


ScoreSegmentSeqCyclicReadOnly::~ScoreSegmentSeqCyclicReadOnly() {
  // FIX ME! NEED TO DETACH FROM SHARED SEGMENT!
  cerr << "ScoreSegmentSeqCyclicReadOnlyERR: NEED TO DETACH FROM SHARED SEGMENT!" << 
    endl;
}

int ScoreSegmentSeqCyclicReadOnly::step() {
  long long int *atable = (long long int *)dataPtr;
	if(!DATASTREAM->stream_full()) {
		// get address
		long long int data=atable[readAddr];
		//printf("Read %g from %lu\n", data, readAddr);
		// recycle to start and resume operation		
		readAddr = (readAddr+1)%segLength;
		// Sep 21 2011: Don't want this behavior for KLU solve.. Ahem!
		if(readAddr==segLength-1) {
			readAddr++;
			DATASTREAM->stream_write(data);
			// not sure about EOFR insertion at end..
			// 7th September 2011
			// 21st September 2011: Back in again for now..
			DATASTREAM->stream_write(EOFR);
		} else {
			readAddr++;
			// write data
			DATASTREAM->stream_write(data);
		}
//		cout << "Seg:" << readAddr << endl;
	}

}

/*
  long long int data, *atable=(long long int *)dataPtr;

  if (this_segment_is_done) {
    return(0);
  }

  if (!STREAM_FULL(DATASTREAM)) {
    if (sim_isFaulted) {
      if (checkIfAddrFault(sim_faultedAddr)) {
	if (VERBOSEDEBUG || DEBUG) {
	  cout << "   SEG SEQSRC: faulting on address " << sim_faultedAddr << 
	    endl;
	}
	
	stall++;
	return(1);
      } else { 
	sim_isFaulted = 0;
	sim_faultedAddr = 0;
	
	// move data to DATASTREAM
	data = atable[readAddr];
	readCount++;
	readAddr++; // increment the address counter
	if (VERBOSEDEBUG || DEBUG) {
	  cout << "   SEG SEQSRC: firing - data is " << data << endl;
	}
	STREAM_WRITE_ARRAY(DATASTREAM, data);
	fire++;
      }
    } else if (readAddr == segLength) { // SEQSRC is empty
      if (VERBOSEDEBUG || DEBUG) {
	cout << "   SEG SEQSRC: firing EOS input" << endl;
      }
      fire++;
      STREAM_CLOSE(DATASTREAM);
      this_segment_is_done = 1;
      return(0);
    } else {
      
      if (VERBOSEDEBUG || DEBUG) {
	cout << "   SEG SEQSRC: firing - address is " << readAddr << endl;
      }
      
      // check if the address is within bounds of the mapped address block.
      if (checkIfAddrFault(readAddr)) {
	sim_isFaulted = 1;
	sim_faultedAddr = readAddr;
	
	if (VERBOSEDEBUG || DEBUG) {
	cout << "   SEG SEQSRC: faulting on address " << sim_faultedAddr << 
	  endl;
	}
	stall++;
	return(1);
      }
      
      // writing data to DATASTREAM
      data = atable[readAddr];
      readCount++;
      readAddr++;
      if (VERBOSEDEBUG || DEBUG) {
	cout << "   SEG SEQSRC: firing - data is " << data << endl;
      }
      STREAM_WRITE_ARRAY(DATASTREAM, data);
      fire++;
    } 
  } else {
    if (VERBOSEDEBUG) {
      cout << "   SEG SEQSRC: not firing" << endl;
    }
    stall++;
  }
   
  return(1);
}
*/
