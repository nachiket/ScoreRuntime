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
// SCORE Global Counter
// $Revision: 1.24 $
//
//////////////////////////////////////////////////////////////////////////////

#include <iostream>
#include <string.h>

#include "ScoreGlobalCounter.h"

ScoreGlobalCounter *globalCounter = 0;
unsigned long long *totalStreamOverheadPtr = 0;
/* PAPI not work
volatile const struct perfctr_cpu_state *usr_cpu_state = 0;
*/

ScoreGlobalCounter::ScoreGlobalCounter() {

   // ADDED BY NACHIKET TO AVOID PERFCTR
  if(TIMEACC) {
    threadCounter = new ScoreThreadCounter(USER_PROG);

    // init stats in the shared segment
    threadCounter->init(); 

    simStartCycle = threadCounter->record->simCycle;
    threadCounter->record->simStartCycle = simStartCycle;
    setCpuSpeed();
  }
}

unsigned long long ScoreGlobalCounter::ScoreGlobalCounterRead() {
  
  currentTime = rdtsc64();

  printf("Global Counter start time:   %llu\n",startTime);
  printf("Global Counter current time: %llu\n",currentTime);

  return (currentTime - startTime);

}

void ScoreGlobalCounter::setCpuSpeed() {
  FILE *ifp;
  char buffer[80], second[20];
  float speed;
  char *ss = "MHz";

  ifp = fopen("/proc/cpuinfo","r");
  if (ifp == NULL) {
    printf("Unable to open file /proc/cpuinfo\n");
    exit(-1);
  }
  while (fgets(buffer,79,ifp) != NULL) {
    sscanf(buffer, "%*s%s%*s%f", second, &speed);
    if (strncmp(second,ss,3) == 0) {
      cpuSpeed = speed;
      break;
    }
  }
  cpuArraySpeedRatio = (int)floor(cpuSpeed/ARRAY_CLOCK_SPEED);
  if (cpuArraySpeedRatio < 1) {
    cpuArraySpeedRatio = 1;
  }
  if (DEBUG) {
    printf("cpu speed is: %f\n", cpuSpeed);
    printf("cpu to array speed raio is: %d\n", cpuArraySpeedRatio);
  }
  fclose(ifp);
}

