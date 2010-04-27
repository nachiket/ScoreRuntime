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
// SCORE Segment Operator (Read-write)
// $Revision: 1.8 $
//
//////////////////////////////////////////////////////////////////////////////


#include <iostream>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include "ScoreStream.h"
#include "ScoreSegment.h"
#include "ScoreOperator.h"
#include "ScoreOperatorElement.h"
#include "ScoreSegmentOperatorReadWrite.h"
#include "ScoreSegmentReadWrite.h"
#include "ScoreConfig.h"


char *ScoreSegmentOperatorReadWrite_instancename=
  "ScoreSegmentOperatorReadWrite_instance";


void *ScoreSegmentOperatorReadWrite_proc_run(void *obj) {
  return(((ScoreSegmentOperatorReadWrite *)obj)->proc_run());
}


ScoreSegmentOperatorReadWrite::ScoreSegmentOperatorReadWrite(
  unsigned int dwidth, unsigned int awidth, size_t nelems,
  UNSIGNED_SCORE_SEGMENT segPtr,
  UNSIGNED_SCORE_STREAM addr, 
  UNSIGNED_SCORE_STREAM dataR, UNSIGNED_SCORE_STREAM dataW,
  BOOLEAN_SCORE_STREAM write) {
  constructorHelper(dwidth, awidth, nelems, segPtr, addr, dataR, dataW, write);
}


ScoreSegmentOperatorReadWrite::ScoreSegmentOperatorReadWrite(
  unsigned int dwidth, unsigned int awidth, size_t nelems,
  SIGNED_SCORE_SEGMENT segPtr,
  UNSIGNED_SCORE_STREAM addr,
  SIGNED_SCORE_STREAM dataR, SIGNED_SCORE_STREAM dataW,
  BOOLEAN_SCORE_STREAM write) {
  constructorHelper(dwidth, awidth, nelems, segPtr, addr, dataR, dataW, write);
}


ScoreSegmentOperatorReadWrite::ScoreSegmentOperatorReadWrite(
  unsigned int dwidth, unsigned int awidth, size_t nelems,
  BOOLEAN_SCORE_SEGMENT segPtr,
  UNSIGNED_SCORE_STREAM addr,
  BOOLEAN_SCORE_STREAM dataR, BOOLEAN_SCORE_STREAM dataW,
  BOOLEAN_SCORE_STREAM write) {
  constructorHelper(dwidth, awidth, nelems, segPtr, addr, dataR, dataW, write);
}


ScoreSegmentOperatorReadWrite::ScoreSegmentOperatorReadWrite(
  unsigned int dwidth, unsigned int awidth, size_t nelems,
  SIGNED_FIXED_SCORE_SEGMENT segPtr,
  UNSIGNED_SCORE_STREAM addr,
  SIGNED_FIXED_STREAM dataR, SIGNED_FIXED_STREAM dataW,
  BOOLEAN_SCORE_STREAM write) {
  constructorHelper(dwidth, awidth, nelems, segPtr, addr, dataR, dataW, write);
}


ScoreSegmentOperatorReadWrite::ScoreSegmentOperatorReadWrite(
  unsigned int dwidth, unsigned int awidth, size_t nelems,
  UNSIGNED_FIXED_SCORE_SEGMENT segPtr, 
  UNSIGNED_SCORE_STREAM addr,
  UNSIGNED_FIXED_STREAM dataR, UNSIGNED_FIXED_STREAM dataW,
  BOOLEAN_SCORE_STREAM write) {
  constructorHelper(dwidth, awidth, nelems, segPtr, addr, dataR, dataW, write);
}

ScoreSegmentOperatorReadWrite::ScoreSegmentOperatorReadWrite(
  unsigned int dwidth, unsigned int awidth, size_t nelems,
  DOUBLE_SCORE_SEGMENT segPtr, 
  UNSIGNED_SCORE_STREAM addr,
  DOUBLE_SCORE_STREAM dataR, DOUBLE_SCORE_STREAM dataW,
  BOOLEAN_SCORE_STREAM write) {
  constructorHelper(dwidth, awidth, nelems, segPtr, addr, dataR, dataW, write);
}


// Decides whether or not the ScoreSegmentOperator should be sent to the
// scheduler. If not, then it spawns the operator as a thread.
void ScoreSegmentOperatorReadWrite::constructorHelper(
  unsigned int dwidth, unsigned int awidth, size_t nelems,
  ScoreSegment *segPtr,
  ScoreStream *addr,
  ScoreStream *dataR, ScoreStream *dataW,
  ScoreStream *write) {
  char *instance_fn=resolve(ScoreSegmentOperatorReadWrite_instancename);

//  cout << "Inside segPtr=" << segPtr << " length=" << segPtr->segLength << endl;
  // do sanity checking!
  if (segPtr->segLength != nelems) {
    cerr << "SCORESEGMENTOPERATORREADWRITEERR: segPtr->segLength != nelems" <<
      endl;
    exit(1);
  }
  if (segPtr->segWidth != dwidth) {
    cerr << "SCORESEGMENTOPERATORREADWRITEERR: segPtr->segWidth != dwidth" <<
      endl;
    exit(1);
  }

  if (instance_fn!=(char *) NULL) {
    long slen;
    long alen;
    long blen;
    ScoreSegmentOperatorReadWrite_arg *arg_data;
    struct msgbuf *msgp;
    arg_data=(ScoreSegmentOperatorReadWrite_arg *)
      malloc(sizeof(ScoreSegmentOperatorReadWrite_arg));
    arg_data->segPtrID=SEGMENT_OBJ_TO_ID(segPtr);
    arg_data->addrID=STREAM_OBJ_TO_ID(addr);
    arg_data->dataRID=STREAM_OBJ_TO_ID(dataR);
    arg_data->dataWID=STREAM_OBJ_TO_ID(dataW);
    arg_data->writeID=STREAM_OBJ_TO_ID(write);
    alen=sizeof(ScoreSegmentOperatorReadWrite_arg);
    slen=strlen(instance_fn);
    blen=sizeof(long)+sizeof(long)+slen+alen;
    msgp=(struct msgbuf *)malloc(sizeof(msgbuf)+sizeof(char)*(blen-1));
    int sid=schedulerId();
    memcpy(msgp->mtext,&alen,sizeof(long));
    memcpy(msgp->mtext+sizeof(long),&slen,sizeof(long));
    memcpy(msgp->mtext+sizeof(long)*2,instance_fn,slen);
    memcpy(msgp->mtext+sizeof(long)*2+slen,arg_data,alen);
    msgp->mtype=SCORE_INSTANTIATE_MESSAGE_TYPE;
    int res=msgsnd(sid, msgp, blen, 0);
    if (res==-1) {
       cerr << "ScoreSegmentOperatorReadWrite msgsnd call failed with errno=" 
	    << errno << endl;
       exit(2);
    }
  } else {

//    segment = segPtr;
//    addrStream = addr;
 //   dataRStream = dataR;
  //  dataWStream = dataW;
    //writeStream = write;

    segment = new ScoreSegmentReadWrite(segPtr, addr,
		    dataR, dataW,
		    write);
    
    pthread_attr_t *a_thread_attribute=(pthread_attr_t *)malloc(sizeof(pthread_attr_t));
    pthread_attr_init(a_thread_attribute);
    pthread_attr_setdetachstate(a_thread_attribute,PTHREAD_CREATE_DETACHED);

    pthread_create(&rpt,a_thread_attribute,&ScoreSegmentOperatorReadWrite_proc_run, this);
				

    // FIX ME!
    // cerr << "NEED TO ADD SCORESEGMENTOPERATORREADWRITE SPAWNING CODE!" << endl;
    // exit(2);
  }
}


#if 1
void *ScoreSegmentOperatorReadWrite::proc_run() {
  
  //printf("Inside proc_run()\n"); fflush(stdout);
  //int address, data, *atable=(int *)segment->dataPtr;
  while (1) {
    segment->step();
    sched_yield();
  }

/*  
    if (segment->sim_isFaulted) {
      if (segment->checkIfAddrFault(segment->sim_faultedAddr)) {
	if (VERBOSEDEBUG || DEBUG) {
	  cout << "   SEG RAMSRCSINK: faulting on address " << segment->sim_faultedAddr << 
	    endl;
	}
      } else { 
	address = segment->sim_faultedAddr;

	segment->sim_isFaulted = 0;
	segment->sim_faultedAddr = 0;

	// writing data to dataOutStream
	data = atable[address];
	if (VERBOSEDEBUG || DEBUG) {
	  cout << "   SEG RAMSRCSINK: firing - data is " << data << endl;
	}
	segment->dataOutStream->stream_write(data);
      }
    } else if (addrStream->stream_eos()) {
	if (VERBOSEDEBUG || DEBUG) {
	  cout << "   SEG RAMSRCSINK: firing EOS input" << endl;
	}
	break;
    } else { // address stream not faulted and not eos    
      address = addrStream->stream_read();
      if (VERBOSEDEBUG || DEBUG) {
	cout << "   SEG RAMSRCSINK: firing - address is " << address << endl;
      }

      // check if the address is within bounds of the mapped address block.
      if (segment->checkIfAddrFault(address)) {
	segment->sim_isFaulted = 1;
	segment->sim_faultedAddr = address;
	segment->sim_faultedMode = rwToken;

	if (VERBOSEDEBUG || DEBUG) {
	  cout << "   SEG RAMSRCSINK: faulting on address " << segment->sim_faultedAddr << 
	    endl;
	}
      }

      // writing data to dataOutStream
      data = atable[address];
      if (VERBOSEDEBUG || DEBUG) {
	cout << "   SEG RAMSRCSINK: firing - data is " << data << endl;
      }
      segment->dataOutStream->stream_write(data);
    }
  }

  stream_free(addrStream);
  stream_close(segment->dataOutStream);
*/  

}
// ELIMINATED ON April 16th 2010
#else
void *ScoreSegmentOperatorReadWrite::proc_run() {
  cerr << "FIX ME!!!!!!" << endl;
  exit(1);
}
#endif

#undef NEW_ScoreSegmentOperatorReadWrite
extern "C" void *NEW_ScoreSegmentOperatorReadWrite(
  unsigned int dwidth, unsigned int awidth, size_t nelems,
  void *segPtr,
  void *addr, 
  void *dataR, void *dataW,
  void *write) {
  return((void *) (new ScoreSegmentOperatorReadWrite(
     dwidth, awidth, nelems, 
     (UNSIGNED_SCORE_SEGMENT) segPtr, 
     (UNSIGNED_SCORE_STREAM) addr, 
     (UNSIGNED_SCORE_STREAM) dataR, 
     (UNSIGNED_SCORE_STREAM) dataW,
     (BOOLEAN_SCORE_STREAM) write)));
}
