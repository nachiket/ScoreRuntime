/////////////////////////////////////////////////////////////////////////////
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
// $Revision: 1.5 $
//
//////////////////////////////////////////////////////////////////////////////


#ifndef _ScoreSchedulerRandomDefines_h__
#define _ScoreSchedulerRandomDefines_h__

#ifdef MAIN_SCHED_FILE
#define _EXTERN_ 
#else
#define _EXTERN_ extern
#endif

#if GET_FEEDBACK
#include "ScoreFeedbackGraph.h"
extern ScoreFeedbackMode gFeedbackMode;
#endif

#define PROFILE_PERFORMPLACEMENT 0
#define PROFILE_PERFORMCLEANUP 0
#if PRINTSTATE
#define KEEPRECONFIGSTATISTICS 1
#else
#define KEEPRECONFIGSTATISTICS 0
#endif

// defining this to 1 forces an exhaustive search for all of the deadlock
// and bufferlock cycles. this can be exponential to find all of the 
// dependency cycles!
// setting this to 0 means we will run a simple linear DFS. for a graph
// with cycles, this should catch at least 1 cycle, but may not catch all of
// them.
#define EXHAUSTIVEDEADLOCKSEARCH 0

// if this is set to 1, then, when a node's IO consumption/production is
// reset, it is assumed that it will consume and produce from all IO.
// if this is set to 0, then, the IO consumption/production will be
// set according to the current state.
#define RESETNODETOALLIO 1


#if SCHEDULECLUSTERS_FOR_SCHEDTIME
#include <asm/msr.h>

_EXTERN_
unsigned long long current_scheduleClusters;

_EXTERN_
unsigned long long startClock, endClock, diffClock;
#endif

#if DOSCHEDULE_FOR_SCHEDTIME
#include <asm/msr.h>

_EXTERN_
unsigned long long current_gatherStatusInfo, current_findDonePagesSegments,
  current_findFaultedMemSeg, current_findFreeableClusters,
  current_dealWithDeadLock, current_scheduleClusters,
  current_performPlacement;

_EXTERN_
unsigned long long startClock, endClock, diffClock;
#endif

#if DOPROFILING || DOPROFILING_SCHEDULECLUSTERS
_EXTERN_
unsigned int total_usedCPs, total_usedCMBs,
  min_usedCPs, min_usedCMBs,
  max_usedCPs, max_usedCMBs;
#endif

#if DOPROFILING
#include <asm/msr.h>

_EXTERN_
unsigned long long current_gatherStatusInfo, current_findDonePagesSegments,
  current_findFaultedMemSeg, current_findFreeableClusters,
  current_dealWithDeadLock, current_scheduleClusters,
  current_performPlacement;

_EXTERN_
unsigned long long total_getCurrentStatus, total_gatherStatusInfo,
  total_findDonePagesSegments, total_findFaultedMemSeg,
  total_findFreeableClusters, total_dealWithDeadLock,
  total_scheduleClusters, total_performPlacement,
  total_issueReconfigCommands, total_performCleanup,
  total_doSchedule;
_EXTERN_
unsigned long long min_getCurrentStatus, min_gatherStatusInfo,
  min_findDonePagesSegments, min_findFaultedMemSeg,
  min_findFreeableClusters, min_dealWithDeadLock,
  min_scheduleClusters, min_performPlacement,
  min_issueReconfigCommands, min_performCleanup,
  min_doSchedule;
_EXTERN_
unsigned long long max_getCurrentStatus, max_gatherStatusInfo,
  max_findDonePagesSegments, max_findFaultedMemSeg,
  max_findFreeableClusters, max_dealWithDeadLock,
  max_scheduleClusters, max_performPlacement,
  max_issueReconfigCommands, max_performCleanup,
  max_doSchedule;

_EXTERN_ unsigned long long startClock, endClock, diffClock;
#endif

#if DOPROFILING_SCHEDULECLUSTERS
#include <asm/msr.h>

#define PROF_CAT_EXECCYCLES   0
#define PROF_CAT_MISSCYCLES   1
#define PROF_CAT_MEMREFS      2

#define PROF_CAT_COUNT        3

_EXTERN_
unsigned long long total_scheduleClusters[PROF_CAT_COUNT],
  total_scheduleClusters_handleDoneNodes[PROF_CAT_COUNT],
  total_scheduleClusters_copyResidentToWaiting[PROF_CAT_COUNT],
  total_scheduleClusters_schedule[PROF_CAT_COUNT],
  total_scheduleClusters_requiredResources[PROF_CAT_COUNT],
  total_scheduleClusters_moveScheduledNodes[PROF_CAT_COUNT],
  total_scheduleClusters_placeStitches[PROF_CAT_COUNT],
  total_scheduleClusters_removeUnscheduledNodes[PROF_CAT_COUNT];

_EXTERN_
unsigned long long min_scheduleClusters[PROF_CAT_COUNT],
  min_scheduleClusters_handleDoneNodes[PROF_CAT_COUNT],
  min_scheduleClusters_copyResidentToWaiting[PROF_CAT_COUNT],
  min_scheduleClusters_schedule[PROF_CAT_COUNT],
  min_scheduleClusters_requiredResources[PROF_CAT_COUNT],
  min_scheduleClusters_moveScheduledNodes[PROF_CAT_COUNT],
  min_scheduleClusters_placeStitches[PROF_CAT_COUNT],
  min_scheduleClusters_removeUnscheduledNodes[PROF_CAT_COUNT];

_EXTERN_
unsigned long long max_scheduleClusters[PROF_CAT_COUNT],
  max_scheduleClusters_handleDoneNodes[PROF_CAT_COUNT],
  max_scheduleClusters_copyResidentToWaiting[PROF_CAT_COUNT],
  max_scheduleClusters_schedule[PROF_CAT_COUNT],
  max_scheduleClusters_requiredResources[PROF_CAT_COUNT],
  max_scheduleClusters_moveScheduledNodes[PROF_CAT_COUNT],
  max_scheduleClusters_placeStitches[PROF_CAT_COUNT],
  max_scheduleClusters_removeUnscheduledNodes[PROF_CAT_COUNT];

_EXTERN_
unsigned long long scheduleClusters_handleDoneNodes_perItemCount,
  scheduleClusters_copyResidentToWaiting_perItemCount,
  scheduleClusters_schedule_perItemCount,
  scheduleClusters_requiredResources_perItemCount,
  scheduleClusters_moveScheduledNodes_perItemCount,
  scheduleClusters_placeStitches_perItemCount,
  scheduleClusters_removeUnscheduledNodes_perItemCount;

_EXTERN_
unsigned long long total_scheduleClusters_handleDoneNodes_perItem[PROF_CAT_COUNT],
  total_scheduleClusters_copyResidentToWaiting_perItem[PROF_CAT_COUNT],
  total_scheduleClusters_schedule_perItem[PROF_CAT_COUNT],
  total_scheduleClusters_requiredResources_perItem[PROF_CAT_COUNT],
  total_scheduleClusters_moveScheduledNodes_perItem[PROF_CAT_COUNT],
  total_scheduleClusters_placeStitches_perItem[PROF_CAT_COUNT],
  total_scheduleClusters_removeUnscheduledNodes_perItem[PROF_CAT_COUNT];

_EXTERN_
unsigned long long min_scheduleClusters_handleDoneNodes_perItem[PROF_CAT_COUNT],
  min_scheduleClusters_copyResidentToWaiting_perItem[PROF_CAT_COUNT],
  min_scheduleClusters_schedule_perItem[PROF_CAT_COUNT],
  min_scheduleClusters_requiredResources_perItem[PROF_CAT_COUNT],
  min_scheduleClusters_moveScheduledNodes_perItem[PROF_CAT_COUNT],
  min_scheduleClusters_placeStitches_perItem[PROF_CAT_COUNT],
  min_scheduleClusters_removeUnscheduledNodes_perItem[PROF_CAT_COUNT];

_EXTERN_
unsigned long long max_scheduleClusters_handleDoneNodes_perItem[PROF_CAT_COUNT],
  max_scheduleClusters_copyResidentToWaiting_perItem[PROF_CAT_COUNT],
  max_scheduleClusters_schedule_perItem[PROF_CAT_COUNT],
  max_scheduleClusters_requiredResources_perItem[PROF_CAT_COUNT],
  max_scheduleClusters_moveScheduledNodes_perItem[PROF_CAT_COUNT],
  max_scheduleClusters_placeStitches_perItem[PROF_CAT_COUNT],
  max_scheduleClusters_removeUnscheduledNodes_perItem[PROF_CAT_COUNT];

#endif

#if KEEPRECONFIGSTATISTICS
_EXTERN_
unsigned long long total_stopPage, total_startPage,
  total_stopSegment, total_startSegment,
  total_dumpPageState, total_dumpPageFIFO,
  total_loadPageConfig, total_loadPageState, total_loadPageFIFO,
  total_getSegmentPointers, total_dumpSegmentFIFO, 
  total_setSegmentConfigPointers, total_changeSegmentMode,
  total_changeSegmentTRAandPBOandMAX, total_resetSegmentDoneFlag, 
  total_loadSegmentFIFO,
  total_memXferPrimaryToCMB, total_memXferCMBToPrimary, total_memXferCMBToCMB,
  total_connectStream;
#endif

#endif // ifndef


