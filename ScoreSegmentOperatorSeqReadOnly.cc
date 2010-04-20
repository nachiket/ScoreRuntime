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
// SCORE Segment Operator (Sequential Read-only)
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
#include "ScoreSegmentOperatorSeqReadOnly.h"
#include "ScoreSegmentSeqReadOnly.h"
#include "ScoreConfig.h"

#define ADDRSTREAM addrStream
#define DATASTREAM dataStream

static char *ScoreSegmentOperatorSeqReadOnly_instancename=
  "ScoreSegmentOperatorSeqReadOnly_instance";

void *ScoreSegmentOperatorSeqReadOnly_proc_run(void *obj) {
  return(((ScoreSegmentOperatorSeqReadOnly *)obj)->proc_run());
}


ScoreSegmentOperatorSeqReadOnly::ScoreSegmentOperatorSeqReadOnly(
  unsigned int dwidth, unsigned int awidth, size_t nelems,
  UNSIGNED_SCORE_SEGMENT segPtr,
  UNSIGNED_SCORE_STREAM data) {

  cout << "Debug: segPtr=" << segPtr << endl;
  cout << "Debug: data()=" << ((long long *)segPtr->data()) << endl;
  cout << "Debug: dataPtr=" << ((long long *)segPtr->dataPtr) << endl;
//  cout << "Debug: data[1]=" << ((long long *)segPtr->dataPtr)[1] << endl;
//  cout << "Debug: data[1]=" << ((long long *)segPtr->data())[1] << endl;

  constructorHelper(dwidth, awidth, nelems, segPtr, data);
}


ScoreSegmentOperatorSeqReadOnly::ScoreSegmentOperatorSeqReadOnly(
  unsigned int dwidth, unsigned int awidth, size_t nelems,
  SIGNED_SCORE_SEGMENT segPtr,
  SIGNED_SCORE_STREAM data) {
  constructorHelper(dwidth, awidth, nelems, segPtr, data);
}


ScoreSegmentOperatorSeqReadOnly::ScoreSegmentOperatorSeqReadOnly(
  unsigned int dwidth, unsigned int awidth, size_t nelems,
  BOOLEAN_SCORE_SEGMENT segPtr,
  BOOLEAN_SCORE_STREAM data) {
  constructorHelper(dwidth, awidth, nelems, segPtr, data);
}


ScoreSegmentOperatorSeqReadOnly::ScoreSegmentOperatorSeqReadOnly(
  unsigned int dwidth, unsigned int awidth, size_t nelems,
  SIGNED_FIXED_SCORE_SEGMENT segPtr,
  SIGNED_FIXED_STREAM data) {
  constructorHelper(dwidth, awidth, nelems, segPtr, data);
}


ScoreSegmentOperatorSeqReadOnly::ScoreSegmentOperatorSeqReadOnly(
  unsigned int dwidth, unsigned int awidth, size_t nelems,
  UNSIGNED_FIXED_SCORE_SEGMENT segPtr, 
  UNSIGNED_FIXED_STREAM data) {
  constructorHelper(dwidth, awidth, nelems, segPtr, data);
}


// Decides whether or not the ScoreSegmentOperator should be sent to the
// scheduler. If not, then it spawns the operator as a thread.
void ScoreSegmentOperatorSeqReadOnly::constructorHelper(
  unsigned int dwidth, unsigned int awidth, size_t nelems,
  ScoreSegment *segPtr,
  ScoreStream *data) {
  //char *instance_fn=(char*)NULL; //resolve(ScoreSegmentOperatorSeqReadOnly_instancename);
  char *instance_fn=resolve(ScoreSegmentOperatorSeqReadOnly_instancename);

  // do sanity checking!
  if (segPtr->segLength != nelems) {
    cerr << "SCORESEGMENTOPERATORSEQREADONLYERR: segPtr->segLength != " <<
      "nelems" << endl;
    exit(1);
  }
  if (segPtr->segWidth != dwidth) {
    cerr << "SCORESEGMENTOPERATORSEQREADONLYERR: segPtr->segWidth != dwidth" <<
      endl;
    exit(1);
  }

  if (instance_fn!=(char *) NULL) {
    long slen;
    long alen;
    long blen;
    ScoreSegmentOperatorSeqReadOnly_arg *arg_data;
    struct msgbuf *msgp;
    arg_data=(ScoreSegmentOperatorSeqReadOnly_arg *)
      malloc(sizeof(ScoreSegmentOperatorSeqReadOnly_arg));
    arg_data->segPtrID=SEGMENT_OBJ_TO_ID(segPtr);
    arg_data->dataID=STREAM_OBJ_TO_ID(data);
    alen=sizeof(ScoreSegmentOperatorSeqReadOnly_arg);
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
      cerr << "ScoreSegmentOperatorSeqReadOnly msgsnd call failed with " <<
	"errno=" << errno << endl;
       exit(2);
    }
  } else {

//	  segment = new ScoreSegmentSeqReadOnly(segPtr, data);

	  pthread_attr_t *a_thread_attribute=(pthread_attr_t *)malloc(sizeof(pthread_attr_t));
	  pthread_attr_init(a_thread_attribute);
	  pthread_attr_setdetachstate(a_thread_attribute,PTHREAD_CREATE_DETACHED);
	  pthread_create(&rpt,a_thread_attribute,&ScoreSegmentOperatorSeqReadOnly_proc_run, this);

    segment = segPtr;
    dataStream = data;

    // FIX ME!
    //cerr << "NEED TO ADD SCORESEGMENTOPERATORSEQREADONLY SPAWNING CODE!" << 
    //  endl;
    //exit(2);
  }
}


#if 1
void* ScoreSegmentOperatorSeqReadOnly::proc_run() {

  int address;
  long long int data;
  long long int *atable=(long long int*)segment->dataPtr; // this shouldn't point to dataPtr.. jesus

  while(1) {
    if(!DATASTREAM->stream_full()) {
      // get address
      address=segment->readAddr;
      segment->readAddr++;
      data=atable[address];
      // recycle to start and resume operation
      if(segment->readAddr==segment->segLength) {
        segment->readAddr=0;
        stream_close(DATASTREAM);
        DATASTREAM->stream_write(atable[address]);
      } else {
        cout << "Internally addr=" << segment->readAddr << 
			   " dataPtr=" << segment->dataPtr << 
			   " data=" << data << endl;
        // write data
        DATASTREAM->stream_write(atable[address]);
      }
    }
    sched_yield();
  }

/*
  int data, *atable=(int *)dataPtr;

  while (1) {
    if (sim_isFaulted) {
      if (checkIfAddrFault(sim_faultedAddr)) {
	if (VERBOSEDEBUG || DEBUG) {
	  cout << "   SEG SEQSRC: faulting on address " << sim_faultedAddr << 
	    endl;
	}
      } else { 

	sim_isFaulted = 0;
	sim_faultedAddr = 0;

	// move data to DATASTREAM
	data = atable[address];
	address++;
	if (VERBOSEDEBUG || DEBUG) {
	  cout << "   SEG SEQSRC: firing - data is " << data << endl;
	}
	DATASTREAM->stream_write(data);
      }
    } else if (address == segLength) {
      if (VERBOSEDEBUG || DEBUG) {
	cout << "   SEG SEQSRC: firing EOS input" << endl;
      }
      break;
    } else { 
      if (VERBOSEDEBUG || DEBUG) {
	cout << "   SEG SEQSRC: firing - address is " << address << endl;
      }

      // check if the address is within bounds of the mapped address block.
      if (checkIfAddrFault(address)) {
	sim_isFaulted = 1;
	sim_faultedAddr = address;

	if (VERBOSEDEBUG || DEBUG) {
	  cout << "   SEG SEQSRC: faulting on address " << sim_faultedAddr << 
	    endl;
	}
      }

      // writing data to DATASTREAM
      data = atable[address];
      address++;
      if (VERBOSEDEBUG || DEBUG) {
	cout << "   SEG SEQSRC: firing - data is " << data << endl;
      }
      DATASTREAM->stream_write(data);
    }
  }

  stream_close(DATASTREAM);
*/  

}
#else
void *ScoreSegmentOperatorSeqReadOnly::proc_run() {
  cerr << "FIX ME!!!!!!" << endl;
  exit(1);
}
#endif

#undef NEW_ScoreSegmentOperatorSeqReadOnly
extern "C" void *NEW_ScoreSegmentOperatorSeqReadOnly(
  unsigned int dwidth, unsigned int awidth, size_t nelems,
  void *segPtr,
  void *data) {
  return((void *) (new ScoreSegmentOperatorSeqReadOnly(
     dwidth, awidth, nelems, 
     (UNSIGNED_SCORE_SEGMENT) segPtr, 
     (UNSIGNED_SCORE_STREAM) data)));
}
