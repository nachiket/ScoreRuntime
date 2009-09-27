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
// $Revision: 1.16 $
//
//////////////////////////////////////////////////////////////////////////////

#ifndef _ScoreGlobalCounter_H

#define _ScoreGlobalCounter_H


#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <errno.h>
#include <asm/msr.h>

#include "ScoreThreadCounter.h"
#include "ScoreConfig.h"
#include "ScoreUserSupport.h"

#define SEM_OVERHEAD 27000
#define THD_OVERHEAD 3000

class ScoreGlobalCounter {

 public:
  ScoreGlobalCounter();
  unsigned long long threadTimeStart;
  unsigned long long ScoreGlobalCounterRead();
  unsigned long long ScoreSimCounterRead();
  unsigned long long ScoreSchCounterRead();
  unsigned long long simStartTime;
  unsigned long long schStartTime;
  void setCpuSpeed(void);
  int simStartCycle;
  ScoreThreadCounter *threadCounter;
  int cpuArraySpeedRatio;
  unsigned long long startTime;

 private:
  unsigned long long currentTime;
  int currentID;
  float cpuSpeed;
};

union u64_conversion {  
        unsigned int ui[2];
        unsigned long long ull;
};

inline unsigned long long rdtsc64(void) {
  return 0;
  /* PAPI not work
  union u64_conversion u;
  rdtsc(u.ui[0], u.ui[1]);
  return u.ull;
  */
}

struct perfctr_state *getUserKstate();


extern ScoreGlobalCounter *globalCounter;
/* PAPI not work - Nachiket edit 27th September
extern volatile const struct perfctr_cpu_state *usr_cpu_state;
*/
extern unsigned long long *totalStreamOverheadPtr;

#endif




