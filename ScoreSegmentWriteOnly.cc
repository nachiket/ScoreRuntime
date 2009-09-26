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
// $Revision: 1.16 $
//
//////////////////////////////////////////////////////////////////////////////

#include <stdlib.h>
#include "ScoreSegmentWriteOnly.h"
#include "ScoreConfig.h"


#define ADDRSTREAM in[SCORE_CMB_RAMSINK_ADDR_INNUM]
#define DATASTREAM in[SCORE_CMB_RAMSINK_DATA_INNUM]


void *ScoreSegmentWriteOnly::operator new(size_t size) {
  return((void *) malloc(size));
}


void ScoreSegmentWriteOnly::constructorHelper(unsigned int dwidth, 
					     unsigned int awidth, 
					     unsigned int nelem,
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
  declareIO(2,0);
  bindInput(SCORE_CMB_RAMSINK_ADDR_INNUM,
	    addr_t,new ScoreStreamType(0,awidth));
  bindInput(SCORE_CMB_RAMSINK_DATA_INNUM,
	    data_t,new ScoreStreamType(0,dwidth));

  // Finally, initialize variables
  mode = SCORE_CMB_RAMSINK;
  sched_mode = SCORE_CMB_RAMSINK;
  sched_old_mode = SCORE_CMB_RAMSINK;

  sim_segmentInputMask = (1<<SCORE_CMB_RAMSINK_ADDR_INNUM) |
    (1<<SCORE_CMB_RAMSINK_DATA_INNUM);

  shouldUseUSECOUNT = segPtr->shouldUseUSECOUNT;
}


ScoreSegmentWriteOnly::~ScoreSegmentWriteOnly() {
  // FIX ME! NEED TO DETACH FROM SHARED SEGMENT!
  cerr << "ScoreSegmentWriteOnlyERR: NEED TO DETACH FROM SHARED SEGMENT!" << 
    endl;
}

int ScoreSegmentWriteOnly::step() {
  
  int address;
  long long int data, *atable=(long long int *)dataPtr;

  if (this_segment_is_done) {
    return(0);
  }

  if (sim_isFaulted) {
    if (checkIfAddrFault(sim_faultedAddr)) {
      if (VERBOSEDEBUG || DEBUG) {
	cout << "   SEG RAMSNK: faulting on address " << sim_faultedAddr << 
	  endl;
      }

      stall++;

      return(1);
    } else {
      if (STREAM_DATA(DATASTREAM)) {
	address = sim_faultedAddr;
	
	sim_isFaulted = 0;
	sim_faultedAddr = 0;
	
	// writing data to DATASTREAM
	data = STREAM_READ_ARRAY(DATASTREAM);
	atable[address] = data;
	writeCount++;
	if (VERBOSEDEBUG || DEBUG) {
	  cout << "   SEG RAMSNK: firing - data is " << data << endl;
	}
	fire++;
      } else {
	stall ++;
      }
    }
  } else if (STREAM_DATA(ADDRSTREAM) && STREAM_DATA(DATASTREAM)) {
    if (STREAM_EOS_ARRAY(ADDRSTREAM) || STREAM_EOS_ARRAY(DATASTREAM)) {
      if (VERBOSEDEBUG || DEBUG) {
	cout << "   SEG RAMSNK: firing EOS input" << endl;
      }
      fire++;

      STREAM_FREE(ADDRSTREAM);
      STREAM_FREE(DATASTREAM);
      
      this_segment_is_done = 1;

      return(0);
    } else {
      address = STREAM_READ_ARRAY(ADDRSTREAM);
      if (VERBOSEDEBUG || DEBUG) {
	cout << "   SEG RAMSNK: firing - address is " << address << endl;
      }

      // check if the address is within bounds of the mapped address block.
      if (checkIfAddrFault(address)) {
	sim_isFaulted = 1;
	sim_faultedAddr = address;

	if (VERBOSEDEBUG || DEBUG) {
	  cout << "   SEG RAMSNK: faulting on address " << sim_faultedAddr << 
	    endl;
	}

	stall++;

	return(1);
      }
      data = STREAM_READ_ARRAY(DATASTREAM);

      // writing data to atable
      atable[address] = data;
      writeCount++;
      if (VERBOSEDEBUG || DEBUG) {
	cout << "   SEG RAMSNK: firing - data is " << data << endl;
      }
      fire++;
    }
  } else {
    if (STREAM_DATA(ADDRSTREAM)) {
      if (STREAM_EOS_ARRAY(ADDRSTREAM)) {
	if (VERBOSEDEBUG || DEBUG) {
	  cout << "   SEG RAMSNK: firing EOS input" << endl;
	}
	fire++;

	STREAM_FREE(ADDRSTREAM);
	STREAM_FREE(DATASTREAM);
      
	this_segment_is_done = 1;
	
	return(0);
      }
    }
    if (STREAM_DATA(DATASTREAM)) {
      if (STREAM_EOS_ARRAY(DATASTREAM)) {
	if (VERBOSEDEBUG || DEBUG) {
	  cout << "   SEG RAMSNK: firing EOS input" << endl;
	}
	fire++;

	STREAM_FREE(ADDRSTREAM);
	STREAM_FREE(DATASTREAM);
      
	this_segment_is_done = 1;
	
	return(0);
      }
    }

    if (VERBOSEDEBUG) {
      cout << "   SEG RAMSNK: not firing" << endl;
    }
    stall++;
  }

  return(1);
}
