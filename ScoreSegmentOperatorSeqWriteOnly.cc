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
// SCORE Segment Operator (Sequential Write-only)
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
#include "ScoreSegmentOperatorSeqWriteOnly.h"
#include "ScoreConfig.h"


char *ScoreSegmentOperatorSeqWriteOnly_instancename=
  "ScoreSegmentOperatorSeqWriteOnly_instance";


void *ScoreSegmentOperatorSeqWriteOnly_proc_run(void *obj) {
  return(((ScoreSegmentOperatorSeqWriteOnly *)obj)->proc_run());
}


ScoreSegmentOperatorSeqWriteOnly::ScoreSegmentOperatorSeqWriteOnly(
  unsigned int dwidth, unsigned int awidth, size_t nelems,
  UNSIGNED_SCORE_SEGMENT segPtr,
  UNSIGNED_SCORE_STREAM data) {
  constructorHelper(dwidth, awidth, nelems, segPtr, data);
}


ScoreSegmentOperatorSeqWriteOnly::ScoreSegmentOperatorSeqWriteOnly(
  unsigned int dwidth, unsigned int awidth, size_t nelems,
  SIGNED_SCORE_SEGMENT segPtr,
  SIGNED_SCORE_STREAM data) {
  constructorHelper(dwidth, awidth, nelems, segPtr, data);
}


ScoreSegmentOperatorSeqWriteOnly::ScoreSegmentOperatorSeqWriteOnly(
  unsigned int dwidth, unsigned int awidth, size_t nelems,
  BOOLEAN_SCORE_SEGMENT segPtr,
  BOOLEAN_SCORE_STREAM data) {
  constructorHelper(dwidth, awidth, nelems, segPtr, data);
}


ScoreSegmentOperatorSeqWriteOnly::ScoreSegmentOperatorSeqWriteOnly(
  unsigned int dwidth, unsigned int awidth, size_t nelems,
  SIGNED_FIXED_SCORE_SEGMENT segPtr,
  SIGNED_FIXED_STREAM data) {
  constructorHelper(dwidth, awidth, nelems, segPtr, data);
}


ScoreSegmentOperatorSeqWriteOnly::ScoreSegmentOperatorSeqWriteOnly(
  unsigned int dwidth, unsigned int awidth, size_t nelems,
  UNSIGNED_FIXED_SCORE_SEGMENT segPtr, 
  UNSIGNED_FIXED_STREAM data) {
  constructorHelper(dwidth, awidth, nelems, segPtr, data);
}

ScoreSegmentOperatorSeqWriteOnly::ScoreSegmentOperatorSeqWriteOnly(
  unsigned int dwidth, unsigned int awidth, size_t nelems,
  DOUBLE_SCORE_SEGMENT segPtr, 
  DOUBLE_SCORE_STREAM data) {
  constructorHelper(dwidth, awidth, nelems, segPtr, data);
}


// Decides whether or not the ScoreSegmentOperator should be sent to the
// scheduler. If not, then it spawns the operator as a thread.
void ScoreSegmentOperatorSeqWriteOnly::constructorHelper(
  unsigned int dwidth, unsigned int awidth, size_t nelems,
  ScoreSegment *segPtr,
  ScoreStream *data) {
  char *instance_fn=resolve(ScoreSegmentOperatorSeqWriteOnly_instancename);

  // do sanity checking!
  if (segPtr->segLength != nelems) {
    cerr << "SCORESEGMENTOPERATORSEQWRITEONLYERR: segPtr->segLength != " <<
      "nelems" << endl;
    exit(1);
  }
  if (segPtr->segWidth != dwidth) {
    cerr << "SCORESEGMENTOPERATORSEQWRITEONLYERR: segPtr->segWidth != " <<
      "dwidth" << endl;
    exit(1);
  }

  if (instance_fn!=(char *) NULL) {
    long slen;
    long alen;
    long blen;
    ScoreSegmentOperatorSeqWriteOnly_arg *arg_data;
    struct msgbuf *msgp;
    arg_data=(ScoreSegmentOperatorSeqWriteOnly_arg *)
      malloc(sizeof(ScoreSegmentOperatorSeqWriteOnly_arg));
    arg_data->segPtrID=SEGMENT_OBJ_TO_ID(segPtr);
    arg_data->dataID=STREAM_OBJ_TO_ID(data);
    alen=sizeof(ScoreSegmentOperatorSeqWriteOnly_arg);
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
      cerr << "ScoreSegmentOperatorSeqWriteOnly msgsnd call failed with " <<
	"errno=" << errno << endl;
       exit(2);
    }
  } else {
    segment = segPtr;
    dataStream = data;

    // FIX ME!
    cerr << "NEED TO ADD SCORESEGMENTOPERATORSEQWRITEONLY SPAWNING CODE!" << 
      endl;
    exit(2);
  }
}


#if 0
void ScoreSegmentOperatorSeqWriteOnly::proc_run() {
  
  int data, *atable=(int *)dataPtr;

  while (1) {
    if (sim_isFaulted) {
      if (checkIfAddrFault(sim_faultedAddr)) {
	if (VERBOSEDEBUG || DEBUG) {
	  cout << "   SEG SEQSNK: faulting on address " << sim_faultedAddr << 
	    endl;
	}
      } else { 

	sim_isFaulted = 0;
	sim_faultedAddr = 0;

	// move data to DATASTREAM
	data = atable[address];
	address++;
	if (VERBOSEDEBUG || DEBUG) {
	  cout << "   SEG SEQSNK: firing - data is " << data << endl;
	}
	DATASTREAM->stream_write(data);
      }
    } else if (address == segLength) {
      if (VERBOSEDEBUG || DEBUG) {
	cout << "   SEG SEQSNK: firing EOS input" << endl;
      }
      break;
    } else { 
      if (VERBOSEDEBUG || DEBUG) {
	cout << "   SEG SEQSNK: firing - address is " << address << endl;
      }

      // check if the address is within bounds of the mapped address block.
      if (checkIfAddrFault(address)) {
	sim_isFaulted = 1;
	sim_faultedAddr = address;

	if (VERBOSEDEBUG || DEBUG) {
	  cout << "   SEG SEQSNK: faulting on address " << sim_faultedAddr << 
	    endl;
	}
      }

      // writing data to DATASTREAM
      data = atable[address];
      address++;
      if (VERBOSEDEBUG || DEBUG) {
	cout << "   SEG SEQSNK: firing - data is " << data << endl;
      }
      DATASTREAM->stream_write(data);
    }
  }

  stream_close(DATASTREAM);

}
#else
void *ScoreSegmentOperatorSeqWriteOnly::proc_run() {
  cerr << "FIX ME!!!!!!" << endl;
  exit(1);
}
#endif

#undef NEW_ScoreSegmentOperatorSeqWriteOnly
extern "C" void *NEW_ScoreSegmentOperatorSeqWriteOnly(
  unsigned int dwidth, unsigned int awidth, size_t nelems,
  void *segPtr,
  void *data) {
  return((void *) (new ScoreSegmentOperatorSeqWriteOnly(
     dwidth, awidth, nelems, 
     (UNSIGNED_SCORE_SEGMENT) segPtr, 
     (UNSIGNED_SCORE_STREAM) data)));
}
