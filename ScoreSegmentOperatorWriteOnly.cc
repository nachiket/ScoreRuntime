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
// SCORE Segment Operator (Write-only)
// $Revision: 1.7 $
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
#include "ScoreSegmentOperatorWriteOnly.h"
#include "ScoreConfig.h"


char *ScoreSegmentOperatorWriteOnly_instancename=
  "ScoreSegmentOperatorWriteOnly_instance";


void *ScoreSegmentOperatorWriteOnly_proc_run(void *obj) {
  return(((ScoreSegmentOperatorWriteOnly *)obj)->proc_run());
}


ScoreSegmentOperatorWriteOnly::ScoreSegmentOperatorWriteOnly(
  unsigned int dwidth, unsigned int awidth, size_t nelems,
  UNSIGNED_SCORE_SEGMENT segPtr,
  UNSIGNED_SCORE_STREAM addr, UNSIGNED_SCORE_STREAM data) {
  constructorHelper(dwidth, awidth, nelems, segPtr, addr, data);
}


ScoreSegmentOperatorWriteOnly::ScoreSegmentOperatorWriteOnly(
  unsigned int dwidth, unsigned int awidth, size_t nelems,
  SIGNED_SCORE_SEGMENT segPtr,
  UNSIGNED_SCORE_STREAM addr, SIGNED_SCORE_STREAM data) {
  constructorHelper(dwidth, awidth, nelems, segPtr, addr, data);
}


ScoreSegmentOperatorWriteOnly::ScoreSegmentOperatorWriteOnly(
  unsigned int dwidth, unsigned int awidth, size_t nelems,
  BOOLEAN_SCORE_SEGMENT segPtr,
  UNSIGNED_SCORE_STREAM addr, BOOLEAN_SCORE_STREAM data) {
  constructorHelper(dwidth, awidth, nelems, segPtr, addr, data);
}


ScoreSegmentOperatorWriteOnly::ScoreSegmentOperatorWriteOnly(
  unsigned int dwidth, unsigned int awidth, size_t nelems,
  SIGNED_FIXED_SCORE_SEGMENT segPtr,
  UNSIGNED_SCORE_STREAM addr, SIGNED_FIXED_STREAM data) {
  constructorHelper(dwidth, awidth, nelems, segPtr, addr, data);
}


ScoreSegmentOperatorWriteOnly::ScoreSegmentOperatorWriteOnly(
  unsigned int dwidth, unsigned int awidth, size_t nelems,
  UNSIGNED_FIXED_SCORE_SEGMENT segPtr, 
  UNSIGNED_SCORE_STREAM addr, UNSIGNED_FIXED_STREAM data) {
  constructorHelper(dwidth, awidth, nelems, segPtr, addr, data);
}

ScoreSegmentOperatorWriteOnly::ScoreSegmentOperatorWriteOnly(
  unsigned int dwidth, unsigned int awidth, size_t nelems,
  DOUBLE_SCORE_SEGMENT segPtr, 
  UNSIGNED_SCORE_STREAM addr, 
  DOUBLE_SCORE_STREAM data) {
  constructorHelper(dwidth, awidth, nelems, segPtr, addr, data);
}


// Decides whether or not the ScoreSegmentOperator should be sent to the
// scheduler. If not, then it spawns the operator as a thread.
void ScoreSegmentOperatorWriteOnly::constructorHelper(
  unsigned int dwidth, unsigned int awidth, size_t nelems,
  ScoreSegment *segPtr,
  ScoreStream *addr, ScoreStream *data) {
  char *instance_fn=resolve(ScoreSegmentOperatorWriteOnly_instancename);

  // do sanity checking!
  if (segPtr->segLength != nelems) {
    cerr << "SCORESEGMENTOPERATORWRITEONLYERR: segPtr->segLength != nelems" <<
      endl;
    exit(1);
  }
  if (segPtr->segWidth != dwidth) {
    cerr << "SCORESEGMENTOPERATORWRITEONLYERR: segPtr->segWidth != dwidth" <<
      endl;
    exit(1);
  }

  if (instance_fn!=(char *) NULL) {
    long slen;
    long alen;
    long blen;
    ScoreSegmentOperatorWriteOnly_arg *arg_data;
    struct msgbuf *msgp;
    arg_data=(ScoreSegmentOperatorWriteOnly_arg *)
      malloc(sizeof(ScoreSegmentOperatorWriteOnly_arg));
    arg_data->segPtrID=SEGMENT_OBJ_TO_ID(segPtr);
    arg_data->addrID=STREAM_OBJ_TO_ID(addr);
    arg_data->dataID=STREAM_OBJ_TO_ID(data);
    alen=sizeof(ScoreSegmentOperatorWriteOnly_arg);
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
       cerr << "ScoreSegmentOperatorWriteOnly msgsnd call failed with errno=" 
	    << errno << endl;
       exit(2);
    }
  } else {
    segment = segPtr;
    addrStream = addr;
    dataStream = data;

    // FIX ME!
    cerr << "NEED TO ADD SCORESEGMENTOPERATORWRITEONLY SPAWNING CODE!" << endl;
    exit(2);
  }
}


#if 0
void ScoreSegmentOperatorWriteOnly::proc_run() {
  
  int address, data, *atable=(int *)dataPtr;

  while (1) {
    if (sim_isFaulted) {
      if (checkIfAddrFault(sim_faultedAddr)) {
	if (VERBOSEDEBUG || DEBUG) {
	  cout << "   SEG RAMSNK: faulting on address " << sim_faultedAddr << 
	    endl;
	}
      } else { 
	address = sim_faultedAddr;

	sim_isFaulted = 0;
	sim_faultedAddr = 0;

	// writing data to DATASTREAM
	data = atable[address];
	if (VERBOSEDEBUG || DEBUG) {
	  cout << "   SEG RAMSNK: firing - data is " << data << endl;
	}
	DATASTREAM->stream_write(data);
      }
    } else if (ADDRSTREAM->stream_eos()) {
	if (VERBOSEDEBUG || DEBUG) {
	  cout << "   SEG RAMSNK: firing EOS input" << endl;
	}
	break;
    } else { // address stream not faulted and not eos
      address = ADDRSTREAM->stream_read();
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
      }

      // writing data to DATASTREAM
      data = atable[address];
      if (VERBOSEDEBUG || DEBUG) {
	cout << "   SEG RAMSNK: firing - data is " << data << endl;
      }
      DATASTREAM->stream_write(data);
    }
  }

  stream_free(ADDRSTREAM);
  stream_close(DATASTREAM);

}
#else
void *ScoreSegmentOperatorWriteOnly::proc_run() {
  cerr << "FIX ME!!!!!!" << endl;
  exit(1);
}
#endif

#undef NEW_ScoreSegmentOperatorWriteOnly
extern "C" void *NEW_ScoreSegmentOperatorWriteOnly(
  unsigned int dwidth, unsigned int awidth, size_t nelems,
  void *segPtr,
  void *addr, void *data) {
  return((void *) (new ScoreSegmentOperatorWriteOnly(
     dwidth, awidth, nelems, 
     (UNSIGNED_SCORE_SEGMENT) segPtr, 
     (UNSIGNED_SCORE_STREAM) addr, 
     (UNSIGNED_SCORE_STREAM) data)));
}
