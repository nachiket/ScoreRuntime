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
// SCORE Counter
// $Revision: 1.14 $
//
//////////////////////////////////////////////////////////////////////////////

#ifndef _ScoreThreadCounter_H
#define _ScoreThreadCounter_H

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sched.h>
#include <pthread.h>

/* PAPI not work
#include "libperfctr.h"
*/

#define IPC_KEY 0xfeedbabe
#define MAX_NUMBER_OF_STREAMS 126


typedef enum { SIMULATOR, SCHEDULER, STREAM,
	       USER_PROG, RUNTIME } RT_component;

const size_t RTC_count = 5;

#ifdef THREADCOUNTERMAIN
const char* RTC_str[] = 
{ "SIMULATOR", "SCHEDULER", "STREAM", "USER_PROG", "RUNTIME" };
#else
extern const char* RTC_str[];
#endif

/* The record class is allocated in the shared segment and will
 * contain the PIDs for each components, and any statistics that
 * should be shared
 */
typedef struct {

  /* interesting statistics */
  int numOfStreamCreated;
  int numOfStreamReadOPs;
  int numOfStreamWriteOPs;
  int numOfStreamEosOPs;
  int numOfUserStreamOPs;

  /* time accounting statistics */
  unsigned long long totalStreamOverhead;
  unsigned long long nonOverlapTime;
  unsigned int simStartCycle;
  unsigned int simCycle;

  /* misc */
  unsigned long long streamThreadTime;
} ScoreThreadCounterRecord;


// NOTE: we need to add mutex to protect creation and destruction of
// these counters

class ScoreThreadCounter {
 public:
  ScoreThreadCounter(RT_component rtc);
  ~ScoreThreadCounter();

  void ScoreThreadCounterEnable(int);

  void init();
  void print(); // for debugging

  unsigned long long read_tsc(void) {
    return 0;
    /* PAPI not work
    return vperfctr_read_tsc(ctr);
    */
  }

  /* PAPI not work
  volatile const struct perfctr_cpu_state *get_cpu_state() const {
    //return ctr->cpu_state;
    return &(ctr->kstate->cpu_state);
  }
  */

  ScoreThreadCounterRecord *record;

 private:
  void* alloc_shared(size_t size);
  void free_shared(void*);

  // look at the ids[] array and instantiate a vperfctr for every
  // process that is running
  void openPerfCtrs();

  /* PAPI not work
  struct vperfctr *ctr;
  */

  RT_component mytype;
  int segID;
};

#endif
