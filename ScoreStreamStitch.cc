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
#include <errno.h>
#include <semaphore.h>
#include <asm/msr.h>
#include "ScoreGraphNode.h"
#include "ScoreStreamStitch.h"
#include "ScoreConfig.h"


void *ScoreStreamStitch::operator new(size_t size) {
  return malloc(size);
}

void ScoreStreamStitch::operator delete(void* rawMem, size_t) {
  free(rawMem);
}


ScoreStreamStitch::ScoreStreamStitch(int width_t, int fixed_t, int length_t,
				     ScoreType type_t) {

  assert(length_t == ARRAY_FIFO_SIZE);

  // constructor for ScoreStreamStitch class
  width = width_t;
  fixed = fixed_t;

  head = tail = 0;
  token_written = token_read = 0;
  src = sink = NULL;
  sched_src = sched_sink = NULL;

  if (!USE_POLLING_STREAMS) { 
    // initialize the semaphores
    if (sem_init(&sem_AVAIL_SLOTS, 0, length_t+1) == 1) {
      perror("sem_init -- creation");
      exit(errno);
    }
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

  srcFunc = STREAM_OPERATOR_TYPE;
  snkFunc = STREAM_OPERATOR_TYPE;
  sched_srcFunc = STREAM_OPERATOR_TYPE;
  sched_snkFunc = STREAM_OPERATOR_TYPE;

  length = length_t;

  isCrossCluster = 0;
  sched_isCrossCluster = 0;

  type = type_t;

  sched_isStitch = 1;
  sched_isPotentiallyFull = 0;
  sched_isPotentiallyEmpty = 0;

  sched_spareStreamStitchList = NULL;

  readThreadCounter = NULL;
  writeThreadCounter = NULL;
  
  threadCounterPtr = NULL;

  realTimeRead[0] = threadTimeRead[0] = 0;
  realTimeWrite[0] = threadTimeWrite[0] = 0;

#if 0
  for (int i=0; i < DEFAULT_N_SLOTS+1+1; i++) {
    buffer[i].token = 0;
    buffer[i].timeStamp = 0;
  }
#endif

  sim_sinkOnStallQueue = 0;
  sim_haveCheckedSinkUnstallTime = 0;

  acquire.sem_num = 0;
  acquire.sem_op = -1;
  acquire.sem_flg = 0; // SEM_UNDO;
  release.sem_num = 0;
  release.sem_op = 1;
  release.sem_flg = 0; // SEM_UNDO;
}

ScoreStreamStitch::~ScoreStreamStitch()
{
  if (VERBOSEDEBUG || DEBUG)
    {
      cerr << "[SID=" << streamID << "]  "
	   << "ScoreStreamStitch Destructor called on " << streamID << endl;
    }
}


// required by LEDA for use with lists/etc.

// provides comparison operation between ScoreStreamStitchs.
// NOTE: Right now, we only say 2 streams are equal if their pointers
//       are equal. Otherwise, less than/greater than is determined
//       simply by their pointer values.
int compare(ScoreStreamStitch * const & left, 
	    ScoreStreamStitch * const & right) {
  if (left == right) {
    return(0);
  } else if (left < right) {
    return(-1);
  } else {
    return(1);
  }
}

