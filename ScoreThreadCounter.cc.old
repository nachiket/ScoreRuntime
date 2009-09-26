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

#define THREADCOUNTERMAIN

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <iostream>
#include "ScoreThreadCounter.h"

// allocate a region of memory in the shared segment identified by
// IPC_KEY
void *ScoreThreadCounter::alloc_shared(size_t size) 
{
  void *shmptr;
  // Want to create a shared memory segment containing the values
  // of the thread counters.
  // The shared memory segent ID is a constant value (IPC_ID)
  // If the segment exists already, we want to attach to it;
  // otherwise, we will create the segment using IPC_ID

  if ((segID=shmget((int)IPC_KEY, size, IPC_CREAT | 0666)) != -1) {
    if ((shmptr=shmat(segID, 0, 0))==(void *) -1) {
      perror("shmptr -- thread counter new operator -- attach ");
      exit(errno);
    }
  } else {
    perror("segID -- thread counter new operator -- creation ");
    exit(errno);
  }
  return shmptr;
}

// release the memory allocated by alloc_shared
void ScoreThreadCounter::free_shared(void* ptr)
{
  // detach the pointer from a segment
  if (shmdt(ptr) == -1) {
    perror("free_shared: shmdt: ");
    exit(errno);
  }
  
  // determine if this process is the creator of the segment (the one
  // that invoked shmget in alloc_shared. The creator can instruct for the
  // segment to be removed. The actual removal will take place once the
  // number of attached processes drops back to 0

  struct shmid_ds shminfo;
  if(!shmctl(segID, IPC_STAT, &shminfo)) {
    if (shminfo.shm_cpid == getpid()) {
      shmctl(segID, IPC_RMID, &shminfo);
    }
  }
}

ScoreThreadCounter::ScoreThreadCounter(RT_component rtc) : mytype(rtc)
{
  record = (ScoreThreadCounterRecord *) 
    alloc_shared(sizeof(ScoreThreadCounterRecord));

  if (!(ctr = vperfctr_open())) {
    perror("ScoreThreadCounter -- vperfctr_open");
    exit (1);
  }
}



ScoreThreadCounter::~ScoreThreadCounter() {

  free_shared(record);
  record = 0;

  vperfctr_close(ctr);
}

void ScoreThreadCounter::ScoreThreadCounterEnable(int counterType)
{
  struct vperfctr_control control;
  struct perfctr_info info;
  
  unsigned int tsc_on = 1;
  unsigned int nractrs = 1;
  unsigned int pmc_map0 = 0;
  unsigned int evntsel0 = 0;
  unsigned int evntsel_aux0 = 0;

  if( vperfctr_info(ctr, &info) < 0 ) {
    perror("ScoreThreadCounterEnable -- vperfctr_info");
    exit(1);
  }

  /* Attempt to set up control to count clocks via the TSC
     and retired instructions via PMC0. */
  switch( info.cpu_type ) {
  case PERFCTR_X86_GENERIC:
    nractrs = 0;		/* no PMCs available */
    break;
  case PERFCTR_X86_INTEL_P5:
  case PERFCTR_X86_INTEL_P5MMX:
  case PERFCTR_X86_CYRIX_MII:
    /* event 0x16 (INSTRUCTIONS_EXECUTED), count at CPL 3 */
    evntsel0 = 0x16 | (2 << 6);
    break;
  case PERFCTR_X86_INTEL_P6:
  case PERFCTR_X86_INTEL_PII:
  case PERFCTR_X86_INTEL_PIII:
  case PERFCTR_X86_AMD_K7:
    /* event 0xC0 (INST_RETIRED), count at CPL > 0, Enable */
    evntsel0 = 0xC0 | (1 << 16) | (1 << 22);
    break;
  case PERFCTR_X86_WINCHIP_C6:
    tsc_on = 0;		/* no working TSC available */
    evntsel0 = 0x02;	/* X86_INSTRUCTIONS */
    break;
  case PERFCTR_X86_WINCHIP_2:
    tsc_on = 0;		/* no working TSC available */
    evntsel0 = 0x16;	/* INSTRUCTIONS_EXECUTED */
    break;
  case PERFCTR_X86_VIA_C3:
    pmc_map0 = 1;		/* redirect PMC0 to PERFCTR1 */
    evntsel0 = 0xC0;	/* INSTRUCTIONS_EXECUTED */
    break;
  case PERFCTR_X86_INTEL_P4:
    /* PMC0: IQ_COUNTER0 with fast RDPMC */
    pmc_map0 = 0x0C | (1 << 31);
    /* IQ_CCCR0: required flags, ESCR 4 (CRU_ESCR0), Enable */
    evntsel0 = (0x3 << 16) | (4 << 13) | (1 << 12);
    /* CRU_ESCR0: event 2 (instr_retired), NBOGUSNTAG, CPL>0 */
    evntsel_aux0 = (2 << 25) | (1 << 9) | (1 << 2);
    break;
  default:
    fprintf(stderr, "cpu type %u (%s) not supported\n",
	    info.cpu_type, perfctr_cpu_name(&info));
    exit(1);
  }
  memset(&control, 0, sizeof control);
  control.cpu_control.tsc_on = tsc_on;
  control.cpu_control.nractrs = nractrs;
  control.cpu_control.pmc_map[0] = pmc_map0;
  control.cpu_control.evntsel[0] = evntsel0;
  control.cpu_control.evntsel_aux[0] = evntsel_aux0;

#if 0
  printf("\nControl used:\n");
  printf("tsc_on\t\t\t%u\n", tsc_on);
  printf("nractrs\t\t\t%u\n", nractrs);
  if( nractrs ) {
    if( pmc_map0 >= 18 )
      printf("pmc_map[0]\t\t0x%08X\n", pmc_map0);
    else
      printf("pmc_map[0]\t\t%u\n", pmc_map0);
    printf("evntsel[0]\t\t0x%08X\n", evntsel0);
    if( evntsel_aux0 )
      printf("evntsel_aux[0]\t\t0x%08X\n", evntsel_aux0);
  }
#endif

  if( vperfctr_control(ctr, &control) < 0 ) {
    perror("ScoreThreadCounterEnable -- vperfctr_control");
    exit(1);
  } 
}


void ScoreThreadCounter::init()
{
  record->numOfStreamCreated = 0;
  record->numOfStreamReadOPs = 0;
  record->numOfStreamWriteOPs = 0;
  record->numOfStreamEosOPs = 0;
  record->numOfUserStreamOPs = 0;

  record->totalStreamOverhead = 0ull;
  record->nonOverlapTime = 0ull;
  record->simStartCycle = 0u;
  record->simCycle = 0u;

  record->streamThreadTime = 0ull;
}


