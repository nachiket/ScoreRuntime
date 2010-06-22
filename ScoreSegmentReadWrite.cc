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
// $Revision: 1.19 $
//
//////////////////////////////////////////////////////////////////////////////

#include <stdlib.h>
#include "ScoreSegmentReadWrite.h"


#define ADDRSTREAM in[SCORE_CMB_RAMSRCSINK_ADDR_INNUM]
#define DATAWSTREAM in[SCORE_CMB_RAMSRCSINK_DATAW_INNUM]
#define WRITESTREAM in[SCORE_CMB_RAMSRCSINK_WRITE_INNUM]
#define DATARSTREAM out[SCORE_CMB_RAMSRCSINK_DATAR_OUTNUM]


void *ScoreSegmentReadWrite::operator new(size_t size) {
  return((void *) malloc(size));
}


void ScoreSegmentReadWrite::constructorHelper(unsigned int dwidth, 
					      unsigned int awidth, 
					      size_t nelem,
					      ScoreSegment* segPtr, 
					      ScoreStream* addr_t, 
					      ScoreStream* dataR_t,
					      ScoreStream* dataW_t,
					      ScoreStream* write_t) {

//  printf("Setup a score segment rw\n");
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

/* // April 16th 2010 - something bizarre is happening with outputs..
  cout << "Setting up streams.. addr_t=" << addr_t << " id=" << addr_t->streamID << endl;
  cout << "Setting up streams.. dataW_t=" << dataW_t << " id=" << dataW_t->streamID << endl;
  cout << "Setting up streams.. write_t=" << write_t << " id=" << write_t->streamID<< endl;
  cout << "Setting up streams.. dataR_t=" << dataR_t << " id=" << dataR_t->streamID << endl;
*/
  

  // Fourth, set the page inputs and outputs 
  declareIO(3,1);
  
  bindInput(SCORE_CMB_RAMSRCSINK_ADDR_INNUM,
	    addr_t,new ScoreStreamType(0,awidth));
  bindInput(SCORE_CMB_RAMSRCSINK_DATAW_INNUM,
	    dataW_t,new ScoreStreamType(0,dwidth));
  bindInput(SCORE_CMB_RAMSRCSINK_WRITE_INNUM,
	    write_t,new ScoreStreamType(0,1));
  bindOutput(SCORE_CMB_RAMSRCSINK_DATAR_OUTNUM,
	     dataR_t,new ScoreStreamType(0,dwidth));

  // Initiate variables
  writeToken = NOT_READ;

  // Finally, initialize variables
  mode = SCORE_CMB_RAMSRCSINK;
  sched_mode = SCORE_CMB_RAMSRCSINK;
  sched_old_mode = SCORE_CMB_RAMSRCSINK;

  long long int *atable = (long long int *)dataPtr;

  if (VERBOSEDEBUG) {
    for (unsigned int index = 0; index < segLength; index++) {
      printf("   SEG RAMSRCSINK: data in %d is %Ld\n", index, atable[index]);
    }
  }

  // set the input mask.
  sim_segmentInputMask = 0;

  shouldUseUSECOUNT = segPtr->shouldUseUSECOUNT;
}


ScoreSegmentReadWrite::~ScoreSegmentReadWrite() {
  // FIX ME! NEED TO DETACH FROM SHARED SEGMENT!
  cerr << "ScoreSegmentReadWriteERR: NEED TO DETACH FROM SHARED SEGMENT!" << 
    endl;
}

int ScoreSegmentReadWrite::step() {

	long long int data, *atable=(long long int *) dataPtr;

	// Get the write signal
	long long int write;
	bool eofr_detected=false;
	if(!WRITESTREAM->stream_empty() && !ADDRSTREAM->stream_empty()) {

		if(STREAM_EOFR(WRITESTREAM)) {
			STREAM_READ_NOACC(WRITESTREAM);
			write = 0;
			eofr_detected = true;
			// propagate EOFR to the output stream?
			DATARSTREAM->stream_write(EOFR);
		} else {
			write = WRITESTREAM->stream_read();
		}

//		cout << "write=" << write << endl; fflush(stdout);

		// Get the address
		int address;
		if(STREAM_EOFR(ADDRSTREAM)) {
			STREAM_READ_NOACC(ADDRSTREAM);
			if(!eofr_detected) {
				cerr << "eofr on write but not on addr" << endl;
				exit(1);
			}
		} else {
			address = STREAM_READ_NOACC(ADDRSTREAM);
		}
	
		if(eofr_detected) {
			cout << "RW EOFR" << endl;
			return(0);
		}

		if(write==0) {
			while(DATARSTREAM->stream_full()) {sched_yield();}
			data=atable[address];
			DATARSTREAM->stream_write(data);
//			cout << "Address=" << address << "Read=" << (double)data << endl; fflush(stdout);

		} else if(write==1) {
			while(DATAWSTREAM->stream_empty()) {sched_yield();}
			data=STREAM_READ_NOACC(DATAWSTREAM);
			atable[address]=data;
//			cout << "Address=" << address << "Write=" << (double)data << endl; fflush(stdout);
		}
	}
}

/* Commented on April 26th 2010
{

  int address;
  long long int data, *atable=(long long int *)dataPtr;

  if (this_segment_is_done) {
    return(0);
  }

  if (!STREAM_FULL(DATARSTREAM)) {
    if (sim_isFaulted) {
    printf("checking for address fault\n"); fflush(stdout);
      if (checkIfAddrFault(sim_faultedAddr)) {
	if (VERBOSEDEBUG || DEBUG) {
	  cout << "   SEG RAMSRCSINK: faulting on address " 
	       << sim_faultedAddr << endl;
	}
	
	stall++;
	
	sim_segmentInputMask = 0;
	
	return(1);
      } else { 
	address = sim_faultedAddr;
	writeToken = sim_faultedMode;
	
	if (writeToken == READTOKEN) {
	  // This is a read operation
	  data = atable[address];
	  readCount++;
	  if (VERBOSEDEBUG || DEBUG) {
	    cout << "   SEG RAMSRCSINK: AFTER FAULT firing - data is " <<
	      data << endl;
	  }
	  STREAM_WRITE_ARRAY(DATARSTREAM, data);
	  fire++;
	  sim_isFaulted = 0;
	  sim_faultedAddr = 0;
	  sim_faultedMode = 0;
	  writeToken = NOT_READ;
	  return(1);
	}
	if ((writeToken == WRITETOKEN) && STREAM_DATA(DATAWSTREAM)) {
	  // This is a write operation 
	  
	  data = STREAM_READ_ARRAY(DATAWSTREAM);
	  
	  // writing data to atable
	  atable[address] = data;
	  writeCount++;
	  if (VERBOSEDEBUG || DEBUG) {
	    cout << "   SEG RAMSRCSNK: firing - data is " << data << endl;
	  }
	  fire++;
	  sim_isFaulted = 0;
	  sim_faultedAddr = 0;
	  sim_faultedMode = 0;
	  writeToken = NOT_READ;
	  return(1);
	}
      }
    } else if ((STREAM_DATA(ADDRSTREAM) && (writeToken != NOT_READ)) ||
	       (STREAM_DATA(ADDRSTREAM) && STREAM_DATA(WRITESTREAM))) {
      if (STREAM_EOS_ARRAY(ADDRSTREAM) ||
	  ((writeToken == NOT_READ) && STREAM_EOS_ARRAY(WRITESTREAM))) {
	if (VERBOSEDEBUG || DEBUG) {
	  cout << "   SEG RAMSRCSINK: firing EOS input" << endl;
	}
	fire++;
	
	STREAM_FREE(ADDRSTREAM);
	STREAM_FREE(DATAWSTREAM);
	STREAM_FREE(WRITESTREAM);
	STREAM_CLOSE(DATARSTREAM);
	
	this_segment_is_done = 1;
	return(0);
      }
      
      // We need to read a token from writeStream to determine whether
      // it is a read or write operation. But we need to make sure
      // we read one token from writeStream for every one from addrStream
      // If  writeToken == NOT_READ, this means we have not read the
      // rwStream yet
      if (writeToken == NOT_READ)
	writeToken = STREAM_READ_ARRAY(WRITESTREAM);
      
      // check whether it is a read or write operation
      // if it is read operation, check if there is space on the dataOutStream
      // if it is write operation, check if there is token in dataInStream
      
      if (writeToken == READTOKEN) {	
	// This is a read operation and there is room in the output stream
	address = STREAM_READ_ARRAY(ADDRSTREAM);
	if (VERBOSEDEBUG || DEBUG) {
	  cout << "   SEG RAMSRCSINK: firing - address is " << address << endl;
	}
	
	// check if the address is within bounds of the mapped address block.
	if (checkIfAddrFault(address)) {
	  sim_isFaulted = 1;
	  sim_faultedAddr = address;
	  sim_faultedMode = writeToken;
	  
	  if (VERBOSEDEBUG || DEBUG) {
	    cout << "   SEG RAMSRCSINK: faulting on address " 
		 << sim_faultedAddr << endl;
	  }
	  
	  stall++;
	  sim_segmentInputMask = 0;
	  return(1);
	}
	
	// writing data to dataOutStream
	data = atable[address];
	readCount++;
	if (VERBOSEDEBUG || DEBUG) {
	  cout << "   SEG RAMSRCSINK: firing - data is " << data << endl;
	}
	STREAM_WRITE_ARRAY(DATARSTREAM, data);
	fire++;
	writeToken = NOT_READ;
	return (1);
      }
      
      if ((writeToken == WRITETOKEN) && STREAM_DATA(DATAWSTREAM)) {
	if (STREAM_EOS_ARRAY(DATAWSTREAM)) {
	  if (VERBOSEDEBUG || DEBUG) {
	    cout << "   SEG RAMSRCSINK: firing EOS input" << endl;
	  }
	  fire++;
	  
	  STREAM_FREE(ADDRSTREAM);
	  STREAM_FREE(DATAWSTREAM);
	  STREAM_FREE(WRITESTREAM);
	  STREAM_CLOSE(DATARSTREAM);
	  
	  this_segment_is_done = 1;
	  
	  return(0);
	}
	
	// This is a write operation
	
	address = STREAM_READ_ARRAY(ADDRSTREAM);
	if (VERBOSEDEBUG || DEBUG) {
	  cout << "   SEG RAMSRCSNK: firing - address is " << address << endl;
	}
	
	// check if the address is within bounds of the mapped address block.
	if (checkIfAddrFault(address)) {
	  sim_isFaulted = 1;
	  sim_faultedAddr = address;
	  sim_faultedMode = writeToken;
	  
	  if (VERBOSEDEBUG || DEBUG) {
	    cout << "   SEG RAMSRCSNK: faulting on address " 
		 << sim_faultedAddr << endl;
	  }
	  
	  stall++;
	  sim_segmentInputMask = 0;
	  return(1);
	}
	
	// Not checking EOS on dataInStream because the assumption
	// is that since addrStream and rwStream have valid tokens
	// dataStream should have valid token as well.
	data = STREAM_READ_ARRAY(DATAWSTREAM);
	
	// writing data to atable
	atable[address] = data;
	writeCount++;
	if (VERBOSEDEBUG || DEBUG) {
	  cout << "   SEG RAMSNK: firing - data is " << data << endl;
	}
	fire++;
	writeToken = NOT_READ;
	return(1);
	
      } 
      
      if (VERBOSEDEBUG) {
	cout << "   SEG RAMSRCSINK: not firing" << endl;
      }
      stall++;
      
      sim_segmentInputMask = (1<<SCORE_CMB_RAMSRCSINK_DATAW_INNUM);
      
    } else {
      if (STREAM_DATA(ADDRSTREAM)) {
	if (STREAM_EOS_ARRAY(ADDRSTREAM)) {
	  if (VERBOSEDEBUG || DEBUG) {
	    cout << "   SEG RAMSRCSINK: firing EOS input" << endl;
	  }
	  fire++;
	  
	  STREAM_FREE(ADDRSTREAM);
	  STREAM_FREE(DATAWSTREAM);
	  STREAM_FREE(WRITESTREAM);
	  STREAM_CLOSE(DATARSTREAM);
	  
	  this_segment_is_done = 1;
	  
	  return(0);
	}
      }
      
      if (((writeToken != NOT_READ) && STREAM_DATA(WRITESTREAM)) &&
	  STREAM_EOS_ARRAY(WRITESTREAM)) {
	if (VERBOSEDEBUG || DEBUG) {
	  cout << "   SEG RAMSRCSINK: firing EOS input" << endl;
	}
	fire++;
	
	STREAM_FREE(ADDRSTREAM);
	STREAM_FREE(DATAWSTREAM);
	STREAM_FREE(WRITESTREAM);
	STREAM_CLOSE(DATARSTREAM);
	
	this_segment_is_done = 1;
	
	return(0);
      }
      
      if (VERBOSEDEBUG) {
	cout << "   SEG RAMSRCSINK: not firing" << endl;
      }
      stall++;
      
      if (!STREAM_DATA(WRITESTREAM)) {
	sim_segmentInputMask = (1<<SCORE_CMB_RAMSRCSINK_WRITE_INNUM);
      } else if (!STREAM_DATA(ADDRSTREAM)) {
	sim_segmentInputMask = (1<<SCORE_CMB_RAMSRCSINK_ADDR_INNUM);
      }
    }
  } else {
    if (VERBOSEDEBUG) {
      cout << "   SEG RAMSRCSINK: not firing" << endl;
    }
    stall++;  
  }
  
  return(1);
} */











