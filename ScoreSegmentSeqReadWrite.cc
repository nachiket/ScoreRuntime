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
// $Revision: 1.14 $
//
//////////////////////////////////////////////////////////////////////////////

#include <stdlib.h>
#include "ScoreSegmentSeqReadWrite.h"



#define DATAWSTREAM in[SCORE_CMB_SEQSRCSINK_DATAW_INNUM]
#define DATARSTREAM out[SCORE_CMB_SEQSRCSINK_DATAR_OUTNUM]


void *ScoreSegmentSeqReadWrite::operator new(size_t size) {
  return((void *) malloc(size));
}


void ScoreSegmentSeqReadWrite::constructorHelper(unsigned int dwidth, 
                                                 unsigned int awidth,
						 unsigned int nelem,
						 ScoreSegment* segPtr, 
						 ScoreStream* dataR_t,
						 ScoreStream* dataW_t) {
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
  declareIO(1,1);
  bindInput(SCORE_CMB_SEQSRCSINK_DATAW_INNUM,
	    dataW_t,new ScoreStreamType(0,dwidth));
  bindOutput(SCORE_CMB_SEQSRCSINK_DATAR_OUTNUM,
	     dataR_t,new ScoreStreamType(0,dwidth));

  // Finally, initialize variables
  mode = SCORE_CMB_SEQSRCSINK;
  sched_mode = SCORE_CMB_SEQSRCSINK;
  sched_old_mode = SCORE_CMB_SEQSRCSINK;

  long long int *atable = (long long int *)dataPtr;

  if (VERBOSEDEBUG) {
    for (unsigned int index = 0; index < segLength; index++) {
      printf("   SEG SEQSRCSINK: data in %d is %Ld\n", index, atable[index]);
    }
  }

  // set the input mask.
  sim_segmentInputMask = (1<<SCORE_CMB_SEQSRCSINK_DATAW_INNUM);

  shouldUseUSECOUNT = segPtr->shouldUseUSECOUNT;
}


ScoreSegmentSeqReadWrite::~ScoreSegmentSeqReadWrite() {
  // FIX ME! NEED TO DETACH FROM SHARED SEGMENT!
  cerr << "ScoreSegmentSeqReadWriteERR: NEED TO DETACH FROM SHARED SEGMENT!" << 
    endl;
}

int ScoreSegmentSeqReadWrite::step() {
/*
  // FIX ME! THIS DOES NOT WORK!!! ALSO, MAKE SURE TO UPDATE READCOUT AND
  //         WRITECOUNT APPROPRIATELY!
  // AND FIX THE STREAM_READ and STREAM_WRITE to ARRAY versions
  cerr << "SEQREADWRITE DOES NOT WORK!" << endl;
  exit(1);
*/

// on April 22nd Nachiket's changes for making this work
// read() and write() should strictly alternate.. 
// arbitrarily read goes first.

  long long int data, *atable=(long long int *)dataPtr;
  if(!DATARSTREAM->stream_full()) {
  	// check address
	// read should be ahead of write
	if(readAddr==writeAddr) {
		// read can proceed..
		data=atable[readAddr];
		if(readAddr==segLength-1) {
			readAddr=0;
			DATARSTREAM->stream_write(data);
			DATARSTREAM->stream_write(EOFR);
		} else {
//  			cout << "wrote data" << endl;
			readAddr++;
			DATARSTREAM->stream_write(data);
		}
	} else {
		// read should wait for a write
	}
  }

  if(!DATAWSTREAM->stream_empty()) {
  	// check and skip EOFRs
	if(STREAM_EOFR(DATAWSTREAM)) {
		STREAM_READ_NOACC(DATAWSTREAM);
	} else {
  		// check address
		// write trails read
		if( (writeAddr==readAddr-1 && readAddr!=0) ||
			(writeAddr==segLength-1 && readAddr==0)) {
			// write can proceed
			data=STREAM_READ_NOACC(DATAWSTREAM); // ouch, what about type?
			atable[writeAddr]=data;
			if(writeAddr==segLength-1) {
				writeAddr=0;
			} else {
				writeAddr++;
			}
		} else {
			// write must wait for read
		}
	}
  }
  return(1);
}
