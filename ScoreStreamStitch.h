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
// $Revision: 1.8 $
//
//////////////////////////////////////////////////////////////////////////////

#ifndef _ScoreStreamStitch_H

#define _ScoreStreamStitch_H

#include <semaphore.h>
#include "ScoreStream.h"
#include "ScoreConfig.h"


class ScoreStreamStitch : public ScoreStream {

 public:
  void *operator new(size_t);
  void operator delete(void *rawMem, size_t size);
  ScoreStreamStitch(int,int,int,ScoreType); 
  //width, fixed, length, type
  ~ScoreStreamStitch();

  void reset() {
    head = tail = 0;
    token_written = token_read = 0;
    src = sink = NULL;
    sched_src = sched_sink = NULL;

    if (!USE_POLLING_STREAMS) {
      
      // reinitialize the semaphores
      if (sem_init(&sem_TO_CONSUME, 0, 0) == 1) {
	perror("sem_init -- creation");
	exit(errno);
      }
      if (sem_init(&sem_DONE_MUTEX, 0, 1) == 1) {
	perror("sem_init -- creation");
	exit(errno);
      }
    }
      
    interProcess = 0;
    producerClosed = 0;
    consumerFreed = 0;
    producerClosed_hw = 0;
    consumerFreed_hw = 0;

    srcIsDone = 0;
    sinkIsDone = 0;
    sched_srcIsDone = 0;
    sched_sinkIsDone = 0;

    sched_srcFunc = STREAM_OPERATOR_TYPE;
    sched_snkFunc = STREAM_OPERATOR_TYPE;

    isCrossCluster = 0;
    sched_isCrossCluster = 0;

    sched_isStitch = 1;
    sched_isPotentiallyFull = 0;
    sched_isPotentiallyEmpty = 0;

    sched_isProcessorArrayStream = 0;

    readThreadCounter = NULL;
    writeThreadCounter = NULL;
  
    threadCounterPtr = NULL;

    sim_sinkOnStallQueue = 0;
    sim_haveCheckedSinkUnstallTime = 0;
  }

  void recycle(int width_t, int fixed_t, int length_t,
	       ScoreType type_t) {
    // FIX ME! REALLY SHOULD PAY ATTENTION TO WIDTH IN CASE IT
    //         CHANGED!

    // constructor for ScoreStreamStitch class
    width = width_t;
    fixed = fixed_t;

    if (!USE_POLLING_STREAMS) { 
      // reinitialize the semaphores
      if (sem_init(&sem_AVAIL_SLOTS, 0, length_t+1) == 1) {
	perror("sem_init -- creation");
	exit(errno);
      }
    }
      
    length = length_t;

    type = type_t;
  }

 private:
};


// needed by LEDA for use with lists/etc.
int compare(ScoreStreamStitch * const & left, 
	    ScoreStreamStitch * const & right);

#endif
