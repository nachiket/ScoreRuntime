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
// SCORE User Support
// $Revision: 2.4 $
//
//////////////////////////////////////////////////////////////////////////////

#include <iostream>
#include <string.h>

#include "ScoreGlobalCounter.h"
#include "ScoreOperator.h"
#include "ScorePlock.h"

extern ScoreGlobalCounter *globalCounter;
extern unsigned long long *totalStreamOverheadPtr;
/* PAPI not work
extern volatile const struct perfctr_cpu_state *usr_cpu_state;
*/

extern "C" void score_init() {

  set_proc(PLOCK_USER_CPU, "score_init");

  globalCounter = new ScoreGlobalCounter();
  /* PAPI not work
  usr_cpu_state = globalCounter->threadCounter->get_cpu_state();
  */
  totalStreamOverheadPtr =
    &(globalCounter->threadCounter->record->totalStreamOverhead);

#if TIMEACC || DOPROFILING
  globalCounter->threadCounter->ScoreThreadCounterEnable(USER_PROG);
  globalCounter->startTime = (unsigned long long) rdtsc64();
#endif
  globalCounter->threadTimeStart = globalCounter->threadCounter->read_tsc();
}

extern "C" void score_exit() {
  //  unsigned long long currentTime = rdtsc64();
  unsigned long long threadTime = globalCounter->threadCounter->read_tsc() - 
    globalCounter->threadTimeStart;

  unsigned long long runningTime = globalCounter->ScoreGlobalCounterRead();
  int simCycle = globalCounter->threadCounter->record->simCycle -
    globalCounter->simStartCycle;      
  
  printf("********************\n");
  printf("*Program Statistics*\n");
  printf("********************\n");
  printf("Simulator start cycle:         %d\n",globalCounter->simStartCycle);
  printf("Simulator current cycle time:  %d\n",globalCounter->threadCounter
	 ->record->simCycle);
  printf("Raw program running time is:   %llu\n",runningTime);
  printf("Number of stream reads:        %d\n",globalCounter->threadCounter->
	 record->numOfStreamReadOPs);
  printf("Number of stream writes:       %d\n",globalCounter->threadCounter->
	 record->numOfStreamWriteOPs);
  printf("Number of stream eos:          %d\n",globalCounter->threadCounter->
	 record->numOfStreamEosOPs);
  printf("Number of user stream ops:     %d\n",globalCounter->threadCounter->
	 record->numOfUserStreamOPs);
  printf("Stream overhead:               %llu\n",globalCounter->threadCounter->
	 record->totalStreamOverhead);
  printf("User program thread time:      %llu\n",threadTime);
  printf("Simulator cycle count is:      %d\n", simCycle);
  printf("Un-accelerated user time:      %llu\n",threadTime -
	 globalCounter->threadCounter->record->totalStreamOverhead);
  printf("Max program running time is:   %llu\n",threadTime + simCycle -
	 globalCounter->threadCounter->record->totalStreamOverhead);

  ScoreOperator::forAllOperators();
}

/* PAPI not work
extern "C" const volatile struct perfctr_cpu_state *getUserCpuState() {
  return globalCounter->threadCounter->get_cpu_state();
}
*/






