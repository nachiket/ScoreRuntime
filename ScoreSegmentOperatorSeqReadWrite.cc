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
// SCORE Segment Operator (Sequential Read-write)
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
#include "ScoreSegmentOperatorSeqReadWrite.h"
#include "ScoreSegmentSeqReadWrite.h"
#include "ScoreConfig.h"


char *ScoreSegmentOperatorSeqReadWrite_instancename=
  "ScoreSegmentOperatorSeqReadWrite_instance";


void *ScoreSegmentOperatorSeqReadWrite_proc_run(void *obj) {
  return(((ScoreSegmentOperatorSeqReadWrite *)obj)->proc_run());
}


ScoreSegmentOperatorSeqReadWrite::ScoreSegmentOperatorSeqReadWrite(
  unsigned int dwidth, unsigned int awidth, size_t nelems,
  UNSIGNED_SCORE_SEGMENT segPtr,
  UNSIGNED_SCORE_STREAM dataR, UNSIGNED_SCORE_STREAM dataW) {
  constructorHelper(dwidth, awidth, nelems, segPtr, dataR, dataW);
}


ScoreSegmentOperatorSeqReadWrite::ScoreSegmentOperatorSeqReadWrite(
  unsigned int dwidth, unsigned int awidth, size_t nelems,
  SIGNED_SCORE_SEGMENT segPtr,
  SIGNED_SCORE_STREAM dataR, SIGNED_SCORE_STREAM dataW) {
  constructorHelper(dwidth, awidth, nelems, segPtr, dataR, dataW);
}


ScoreSegmentOperatorSeqReadWrite::ScoreSegmentOperatorSeqReadWrite(
  unsigned int dwidth, unsigned int awidth, size_t nelems,
  BOOLEAN_SCORE_SEGMENT segPtr,
  BOOLEAN_SCORE_STREAM dataR, BOOLEAN_SCORE_STREAM dataW) {
  constructorHelper(dwidth, awidth, nelems, segPtr, dataR, dataW);
}


ScoreSegmentOperatorSeqReadWrite::ScoreSegmentOperatorSeqReadWrite(
  unsigned int dwidth, unsigned int awidth, size_t nelems,
  SIGNED_FIXED_SCORE_SEGMENT segPtr,
  SIGNED_FIXED_STREAM dataR, SIGNED_FIXED_STREAM dataW) {
  constructorHelper(dwidth, awidth, nelems, segPtr, dataR, dataW);
}


ScoreSegmentOperatorSeqReadWrite::ScoreSegmentOperatorSeqReadWrite(
  unsigned int dwidth, unsigned int awidth, size_t nelems,
  UNSIGNED_FIXED_SCORE_SEGMENT segPtr, 
  UNSIGNED_FIXED_STREAM dataR, UNSIGNED_FIXED_STREAM dataW) {
  constructorHelper(dwidth, awidth, nelems, segPtr, dataR, dataW);
}

ScoreSegmentOperatorSeqReadWrite::ScoreSegmentOperatorSeqReadWrite(
  unsigned int dwidth, unsigned int awidth, size_t nelems,
  DOUBLE_SCORE_SEGMENT segPtr, 
  DOUBLE_SCORE_STREAM dataR, DOUBLE_SCORE_STREAM dataW) {
  constructorHelper(dwidth, awidth, nelems, segPtr, dataR, dataW);
}


// Decides whether or not the ScoreSegmentOperator should be sent to the
// scheduler. If not, then it spawns the operator as a thread.
void ScoreSegmentOperatorSeqReadWrite::constructorHelper(
  unsigned int dwidth, unsigned int awidth, size_t nelems,
  ScoreSegment *segPtr,
  ScoreStream *dataR, ScoreStream *dataW) {
  char *instance_fn=resolve(ScoreSegmentOperatorSeqReadWrite_instancename);

  // do sanity checking!
  if (segPtr->segLength != nelems) {
    cerr << "SCORESEGMENTOPERATORSEQREADWRITEERR: segPtr->segLength != " <<
      "nelems" << endl;
    exit(1);
  }
  if (segPtr->segWidth != dwidth) {
    cerr << "SCORESEGMENTOPERATORSEQREADWRITEERR: segPtr->segWidth != " <<
      "dwidth" << endl;
    exit(1);
  }

  if (instance_fn!=(char *) NULL) {
    long slen;
    long alen;
    long blen;
    ScoreSegmentOperatorSeqReadWrite_arg *arg_data;
    struct msgbuf *msgp;
    arg_data=(ScoreSegmentOperatorSeqReadWrite_arg *)
      malloc(sizeof(ScoreSegmentOperatorSeqReadWrite_arg));
    arg_data->segPtrID=SEGMENT_OBJ_TO_ID(segPtr);
    arg_data->dataRID=STREAM_OBJ_TO_ID(dataR);
    arg_data->dataWID=STREAM_OBJ_TO_ID(dataW);
    alen=sizeof(ScoreSegmentOperatorSeqReadWrite_arg);
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
      cerr << "ScoreSegmentOperatorSeqReadWrite msgsnd call failed with " <<
	"errno=" << errno << endl;
       exit(2);
    }
  } else {
    //segment = segPtr;
    segment = new ScoreSegmentSeqReadWrite(segPtr, dataR, dataW);
    dataRStream = dataR;
    dataWStream = dataW;

    pthread_attr_t *a_thread_attribute=(pthread_attr_t *)malloc(sizeof(pthread_attr_t));
    pthread_attr_init(a_thread_attribute);
    pthread_attr_setdetachstate(a_thread_attribute,PTHREAD_CREATE_DETACHED);
    pthread_create(&rpt,a_thread_attribute,&ScoreSegmentOperatorSeqReadWrite_proc_run, this);


    // FIX ME!
//    cerr << "NEED TO ADD SCORESEGMENTOPERATORSEQREADWRITE SPAWNING CODE!" << 
//      endl;
//    exit(2);
  }
}


#if 1
void* ScoreSegmentOperatorSeqReadWrite::proc_run() {

  while(1) {
    segment->step();
    sched_yield();
  }

/*
  int data, *atable=(int *)dataPtr;

  while (1) {
    if (sim_isFaulted) {
      if (checkIfAddrFault(sim_faultedAddr)) {
	if (VERBOSEDEBUG || DEBUG) {
	  cout << "   SEG SEQSRCSINK: faulting on address " 
	       << sim_faultedAddr << endl;
	}
      } else { 

	sim_isFaulted = 0;
	sim_faultedAddr = 0;

	// move data to DATASTREAM
	data = atable[address];
	address++;
	if (VERBOSEDEBUG || DEBUG) {
	  cout << "   SEG SEQSRCSINK: firing - data is " << data << endl;
	}
	DATASTREAM->stream_write(data);
      }
    } else if (address == segLength) {
      if (VERBOSEDEBUG || DEBUG) {
	cout << "   SEG SEQSRCSINK: firing EOS input" << endl;
      }
      break;
    } else { 
      if (VERBOSEDEBUG || DEBUG) {
	cout << "   SEG SEQSRCSINK: firing - address is " << address << endl;
      }

      // check if the address is within bounds of the mapped address block.
      if (checkIfAddrFault(address)) {
	sim_isFaulted = 1;
	sim_faultedAddr = address;

	if (VERBOSEDEBUG || DEBUG) {
	  cout << "   SEG SEQSRCSINK: faulting on address " 
	       << sim_faultedAddr << endl;
	}
      }

      // writing data to DATASTREAM
      data = atable[address];
      address++;
      if (VERBOSEDEBUG || DEBUG) {
	cout << "   SEG SEQSRCSINK: firing - data is " << data << endl;
      }
      DATASTREAM->stream_write(data);
    }
  }

  stream_close(DATASTREAM);
*/
}
#else
void *ScoreSegmentOperatorSeqReadWrite::proc_run() {
  cerr << "FIX ME!!!!!!" << endl;
  exit(1);
}
#endif

#undef NEW_ScoreSegmentOperatorSeqReadWrite
extern "C" void *NEW_ScoreSegmentOperatorSeqReadWrite(
  unsigned int dwidth, unsigned int awidth, size_t nelems,
  void *segPtr,
  void *dataR, void *dataW) {
  return((void *) (new ScoreSegmentOperatorSeqReadWrite(
     dwidth, awidth, nelems, 
     (UNSIGNED_SCORE_SEGMENT) segPtr, 
     (UNSIGNED_SCORE_STREAM) dataR, 
     (UNSIGNED_SCORE_STREAM) dataW)));
}
