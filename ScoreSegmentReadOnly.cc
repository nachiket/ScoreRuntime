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
// $Revision: 1.31 $
//
//////////////////////////////////////////////////////////////////////////////

#include <stdlib.h>
#include "ScoreSegmentReadOnly.h"
#include "ScoreConfig.h"


#define ADDRSTREAM in[SCORE_CMB_RAMSRC_ADDR_INNUM]
#define DATASTREAM out[SCORE_CMB_RAMSRC_DATA_OUTNUM]

void *ScoreSegmentReadOnly::operator new(size_t size) {
  return((void *) malloc(size));
}


void ScoreSegmentReadOnly::constructorHelper(unsigned int dwidth, 
					     unsigned int awidth, 
					     size_t nelem,
					     ScoreSegment* segPtr, 
					     ScoreStream* addr_t, 
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
   
  // Third,  copy some data over
  segLength = segPtr->segLength;

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
  bindInput(SCORE_CMB_RAMSRC_ADDR_INNUM,
	    addr_t,new ScoreStreamType(0,awidth));
  bindOutput(SCORE_CMB_RAMSRC_DATA_OUTNUM,
	     data_t,new ScoreStreamType(0,dwidth));

  // Finally, initialize variables
  mode = SCORE_CMB_RAMSRC;
  sched_mode = SCORE_CMB_RAMSRC;
  sched_old_mode = SCORE_CMB_RAMSRC;

  long long int *atable = (long long int *)dataPtr;

  if (VERBOSEDEBUG) {
    for (unsigned int index = 0; index < segLength; index++) {
      printf("   SEG RAMSRC: data in %d is %Ld\n", index, atable[index]);
    }
  }

  // set the input mask.
  sim_segmentInputMask = (1<<SCORE_CMB_RAMSRC_ADDR_INNUM);

  shouldUseUSECOUNT = segPtr->shouldUseUSECOUNT;
}


ScoreSegmentReadOnly::~ScoreSegmentReadOnly() {
  // FIX ME! NEED TO DETACH FROM SHARED SEGMENT!
  cerr << "SCORESEGMENTREADONLYERR: NEED TO DETACH FROM SHARED SEGMENT!" << 
    endl;
}

int ScoreSegmentReadOnly::step() {
  
  int address;
  long long int data, *atable=(long long int *)dataPtr;

  if (this_segment_is_done) {
    return(0);
  }

  if (!STREAM_FULL(DATASTREAM)) {
    if (sim_isFaulted) {
      if (checkIfAddrFault(sim_faultedAddr)) {
	if (VERBOSEDEBUG || DEBUG) {
	  cout << "   SEG RAMSRC: faulting on address " << sim_faultedAddr << 
	    endl;
	}
	
	stall++;
	
	return(1);
      } else { 
	address = sim_faultedAddr;
	
	sim_isFaulted = 0;
	sim_faultedAddr = 0;
	
	// writing data to DATASTREAM
	data = atable[address];
	readCount++;
	if (VERBOSEDEBUG || DEBUG) {
	  cout << "   SEG RAMSRC: firing - data is " << data << endl;
	}
	STREAM_WRITE_ARRAY(DATASTREAM, data);
	fire++;
      }
    } else if (STREAM_DATA(ADDRSTREAM)) {
      if (STREAM_EOS_ARRAY(ADDRSTREAM)) {
	if (VERBOSEDEBUG || DEBUG) {
	  cout << "   SEG RAMSRC: firing EOS input" << endl;
	}
	fire++;
	
	STREAM_FREE(ADDRSTREAM);
	STREAM_CLOSE(DATASTREAM);
	
	this_segment_is_done = 1;
	
	return(0);
      } else {
	address = STREAM_READ_ARRAY(ADDRSTREAM);
	if (VERBOSEDEBUG || DEBUG) {
	  cout << "   SEG RAMSRC: firing - address is " << address << endl;
	}
	
	// check if the address is within bounds of the mapped address block.
	if (checkIfAddrFault(address)) {
	  sim_isFaulted = 1;
	  sim_faultedAddr = address;
	  
	  if (VERBOSEDEBUG || DEBUG) {
	    cout << "   SEG RAMSRC: faulting on address " <<
	      sim_faultedAddr << endl;
	  }
	  
	  stall++;
	  
	  return(1);
	}
	
	// writing data to DATASTREAM
	data = atable[address];
	readCount++;
	if (VERBOSEDEBUG || DEBUG) {
	  cout << "   SEG RAMSRC: firing - data is " << data << endl;
	}
	STREAM_WRITE_ARRAY(DATASTREAM, data);
	fire++;
      }
    } else {
      if (VERBOSEDEBUG) {
	cout << "   SEG RAMSRC: not firing" << endl;
      }
      stall++;
    }
  } else {  
    if (VERBOSEDEBUG) {
      cout << "   SEG RAMSRC: not firing" << endl;
    }
    stall++;
  }
  return(1);
}
