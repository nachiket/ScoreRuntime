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
  // FIX ME! THIS DOES NOT WORK!!! ALSO, MAKE SURE TO UPDATE READCOUT AND
  //         WRITECOUNT APPROPRIATELY!
  // AND FIX THE STREAM_READ and STREAM_WRITE to ARRAY versions
  cerr << "SEQREADWRITE DOES NOT WORK!" << endl;
  exit(1);
#if 0
  
  long long int data, *atable=(long long int *)dataPtr;

  if (this_segment_is_done) {
    return(0);
  }

  if (sim_isFaulted) {
    if (checkIfAddrFault(sim_faultedAddr)) {
      if (VERBOSEDEBUG || DEBUG) {
	cout << "   SEG SEQSRCSINK: faulting on address " << sim_faultedAddr << 
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
	cout << "   SEG SEQSRCSINK: firing - data is " << data << endl;
      }
      DATASTREAM->stream_write(data);
      fire++;
    }
  } else if (readAddr == segLength) { // SEQSRC is empty
    if (VERBOSEDEBUG || DEBUG) {
      cout << "   SEG SEQSRCSINK: firing EOS input" << endl;
    }
    fire++;

    stream_close(DATASTREAM);

    this_segment_is_done = 1;
      
    return(0);
  } else if (!DATASTREAM->stream_full()) {

    if (VERBOSEDEBUG || DEBUG) {
      cout << "   SEG SEQSRCSINK: firing - address is " << writeAddr << endl;
    }

    // check if the address is within bounds of the mapped address block.
    if (checkIfAddrFault(writeAddr)) {
      sim_isFaulted = 1;
      sim_faultedAddr = writeAddr;

      if (VERBOSEDEBUG || DEBUG) {
	cout << "   SEG SEQSRCSINK: faulting on address " 
	     << sim_faultedAddr << endl;
      }

      stall++;

      return(1);
    }

    // writing data to DATASTREAM
    data = atable[readAddr];
    readCount++;
    readAddr++;
    if (VERBOSEDEBUG || DEBUG) {
      cout << "   SEG SEQSRCSINK: firing - data is " << data << endl;
    }
    DATASTREAM->stream_write(data);
    fire++;
  } else {
    if (VERBOSEDEBUG) {
      cout << "   SEG SEQSRCSINK: not firing" << endl;
    }
    stall++;
  }
#endif
  return(1);
}
