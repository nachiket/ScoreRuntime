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
// $Revision: 1.9 $
//
//////////////////////////////////////////////////////////////////////////////

#include <iostream.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <values.h>
#include <dlfcn.h>
#include "LEDA/core/list.h"
#include "LEDA/graph/graph.h"
#include "LEDA/basic_graph_alg.h"
#include "LEDA/graph_alg.h"
#include "ScoreOperatorInstance.h"
#include "ScoreStream.h"
#include "ScoreStreamStitch.h"
#include "ScorePage.h"
#include "ScoreSegment.h"
#include "ScoreSegmentStitch.h"
#include "ScoreCluster.h"
#include "ScoreProcess.h"
#include "ScoreProcessorNode.h"
#include "ScoreArray.h"
#include "ScoreType.h"
#include "ScoreHardwareAPI.h"
#include "ScoreSchedulerRandom.h"
#include "ScoreRuntime.h"
#include "ScoreSimulator.h"
#include "ScoreConfig.h"
#include "ScoreStateGraph.h"
#include "ScoreDummyDonePage.h"
#include "ScoreCustomStack.h"
#include "ScoreCustomList.h"
#include "ScoreCustomQueue.h"
#include "ScoreCustomLinkedList.h"

#include "ScoreSchedulerRandomDefines.h"

extern bool PROFILE_VERBOSE;

#if DOPROFILING_SCHEDULECLUSTERS

static char *profile_cats[] = { "exec cycles", "miss cycles", "mem refs" };

#define PROFILE_SCHEDULECLUSTERS(__categoryVar__,__categoryString__,__itemCount__) \
  endTime[PROF_CAT_EXECCYCLES] = threadCounter->ScoreThreadSchCounterRead(); \
  threadCounter->DCUmissRead(SCHEDULER, endTime[PROF_CAT_MEMREFS], endTime[PROF_CAT_MISSCYCLES]); \
 { if ((__itemCount__) != 0)\
     scheduleClusters_ ## __categoryVar__ ## _perItemCount ++; \
   \
 for (int xyz = 0; xyz < PROF_CAT_COUNT; xyz ++) { \
  diffTime = endTime[xyz] - startTime[xyz]; \
 \
  total_scheduleClusters_ ## __categoryVar__ [xyz] += diffTime; \
  if (diffTime < min_scheduleClusters_ ## __categoryVar__ [xyz]) { \
    min_scheduleClusters_ ## __categoryVar__ [xyz] = diffTime; \
  } \
  if (diffTime > max_scheduleClusters_ ## __categoryVar__ [xyz]) { \
    max_scheduleClusters_ ## __categoryVar__ [xyz]= diffTime; \
  } \
 \
  current_scheduleClusters_ ## __categoryVar__ [xyz] = diffTime; \
 \
  if ((__itemCount__) != 0) { \
       diffTime_perItem = diffTime / (__itemCount__); \
    \
    total_scheduleClusters_ ## __categoryVar__ ## _perItem [xyz] += diffTime_perItem; \
    if (diffTime_perItem < min_scheduleClusters_ ## __categoryVar__ ## _perItem [xyz]) { \
       min_scheduleClusters_ ## __categoryVar__ ## _perItem [xyz] = diffTime_perItem; \
    } \
    if (diffTime_perItem > max_scheduleClusters_ ## __categoryVar__ ## _perItem [xyz]) { \
       max_scheduleClusters_ ## __categoryVar__ ## _perItem [xyz] = diffTime_perItem; \
    } \
  } \
 if (PROFILE_VERBOSE) { \
 if (xyz == 0) cerr << "****** SCHEDULECLUSTERS - " << __categoryString__ << ":\n"; \
 cerr << " ** " << profile_cats[xyz] <<": " << diffTime; \
 if ((__itemCount__) != 0) { \
   cerr << " [" << diffTime_perItem << "/item]"; \
 } cerr << endl; } } \
  } \
 \
  threadCounter->DCUmissRead(SCHEDULER, startTime[PROF_CAT_MEMREFS], startTime[PROF_CAT_MISSCYCLES]); \
  startTime[PROF_CAT_EXECCYCLES] = threadCounter->ScoreThreadSchCounterRead(); 

#else
#define PROFILE_SCHEDULECLUSTERS(__categoryVar__,__categoryString__,__itemCount__)
#endif

#define PRINT_SCHEDULECLUSTERS_PROFILE(__category__) \
    { for (int xyz = 0; xyz < PROF_CAT_COUNT; xyz ++ ) { \
      if (xyz == 0) cerr << "SCHED: Total scheduleClusters_" << #__category__ << ":\n" ; \
      cerr << " ** " << profile_cats[xyz] << ": " << \
      total_scheduleClusters_ ## __category__ [xyz] << " cycle(s) " <<  \
      "[" << min_scheduleClusters_ ## __category__  [xyz] << " : " << \
      total_scheduleClusters_ ## __category__  [xyz] /currentTimeslice << " : " << \
      max_scheduleClusters_ ## __category__  [xyz] << "] " << \
      "[" << (((double) total_scheduleClusters_ ## __category__ [xyz] )/ \
	      totalCycles)*100 <<  \
      "]% -- {" << min_scheduleClusters_ ## __category__ ## _perItem  [xyz] << " : " << \
      (scheduleClusters_ ## __category__ ## _perItemCount ? total_scheduleClusters_ ## __category__ ## _perItem  [xyz] /   \
            scheduleClusters_ ## __category__ ## _perItemCount : 0) << " : " \
	   << max_scheduleClusters_ ## __category__ ## _perItem  [xyz] << "}/item" << endl; } }



//-------------------------------------------------------------------------
// ScoreSchedulerRandom::recomputeDeltaCMB
//    given the func and the node on one end of the stream, this routine
//    compute the change in available cmbs that is required to schedule
//    this node. It is assumed that this node is not scheduled currently,
//    and only one node is scheduled at a time.
//    assumptions: node is NOT done
//    retval: true, if 'node' should be marked as scheduled, if page fits
//            false, otherwise
//------------------------------------------------------------------------
inline bool ScoreSchedulerRandom::recomputeDeltaCMB(int func,
						    ScoreGraphNode *mynode, 
						    int *deltaCMB,
						    ScoreCluster *currentCluster)
{
  switch(func) {
  case STREAM_OPERATOR_TYPE:
    assert(mynode == 0);
#if 0
    (*deltaCMB) --;
#endif
    break;
  case STREAM_SEGMENT_TYPE:
    assert(mynode->isSegment());
    if(((ScoreSegment*)mynode)->sched_isStitch) {
      ScoreSegmentStitch *stitch = (ScoreSegmentStitch*) mynode;
      
      // if it is a member of the cluster, do not account for it
      if (stitch->sched_parentCluster == currentCluster)
	return true;

      // only account for it, if it was not already scheduled
      // the following check relies on the fact that stitches in 
      // stitchListToMarkScheduled will be mark scheduled at this point
      // and we wish to prevent double-counting.
      if (stitch->getLastTimesliceScheduled() != currentTimeslice) {
	(*deltaCMB) --;
	return true;
      }
      break;
    }
    // fall through to page for "regular segments"
  case STREAM_PAGE_TYPE:
    {
#ifndef NDEBUG
      if (func == STREAM_PAGE_TYPE) 
	assert(mynode->isPage());
      else 
	assert(mynode->isSegment());
#endif
      if (mynode->getLastTimesliceScheduled() != currentTimeslice) {
	(*deltaCMB) --;
      } else {
	// adjust for the fact that 'page' already accounted for
	// possible CMB, that will not be needed
	(*deltaCMB) ++; 
      }
    }
    break;
  default:
    assert(0);
  }
  return false;
}


//-------------------------------------------------------------------------
// ScoreSchedulerRandom::insertStitchBufferOnInput
//     given the following arguments:
//                              |------------|
// ---------attachedStream----->|  cluster   |
//                              |------------|
//     insert a stitch buffer in between and return its pointer:
//
//                               .------.                 |------------|
// ---------attachedStream----->< stitch >---newStream--->|  cluster   |
//                               `------'                 |------------|
//
//-------------------------------------------------------------------------
ScoreSegmentStitch*
ScoreSchedulerRandom::insertStitchBufferOnInput(SCORE_STREAM attachedStream, 
						ScoreCluster *cluster,
						unsigned int clusterInputNum)
{
  ScoreStreamStitch *newStream = NULL;
  ScoreGraphNode *currentNode = attachedStream->sched_sink;
  int oldAttachedStreamSinkNum = attachedStream->sched_snkNum;
  ScoreStreamType *oldAttachedStreamSinkType =
    currentNode->inputType(oldAttachedStreamSinkNum);
  ScoreStream *oldStream = 
    currentNode->getSchedInput(oldAttachedStreamSinkNum);
  
  // attempt to get a spare stream stitch!
  if (SCORECUSTOMSTACK_LENGTH(spareStreamStitchList) > 0) {
    ScoreStream *tempStream;
    
    SCORECUSTOMSTACK_POP(spareStreamStitchList, tempStream);
    newStream = (ScoreStreamStitch *) tempStream;
    
    newStream->recycle(attachedStream->get_width(),
		       attachedStream->get_fixed(),
		       attachedStream->get_length(),
		       attachedStream->get_type());
  } else {
    if (VERBOSEDEBUG || DEBUG) {
      cout << "SCHED: RAN OUT OF SPARE STITCH STREAMS! " <<
	"INSTANTIATING A NEW ONE!" << endl;
    }
    
    newStream = new ScoreStreamStitch(attachedStream->get_width(),
				      attachedStream->get_fixed(),
				      attachedStream->get_length(),
				      attachedStream->get_type());
    newStream->reset();
    newStream->sched_spareStreamStitchList = spareStreamStitchList;
  }
  
  newStream->consumerFreed = oldStream->consumerFreed;
  oldStream->consumerFreed = 0;
  newStream->consumerFreed_hw = oldStream->consumerFreed_hw;
  oldStream->consumerFreed_hw = 0;
  newStream->sched_isCrossCluster = oldStream->sched_isCrossCluster;
  newStream->sim_sinkOnStallQueue = oldStream->sim_sinkOnStallQueue;
  oldStream->sim_sinkOnStallQueue = 0;
  newStream->sim_haveCheckedSinkUnstallTime = 
    oldStream->sim_haveCheckedSinkUnstallTime;
  oldStream->sim_haveCheckedSinkUnstallTime = 0;
  
  currentNode->unbindSchedInput(oldAttachedStreamSinkNum);
  currentNode->bindSchedInput(oldAttachedStreamSinkNum,
			      newStream,
			      oldAttachedStreamSinkType);
  
  // FIX ME! IS THIS THE CORRECT WAY TO SPECIFY ABSOLUTE
  //         SEGMENT SIZE?
  ScoreSegmentStitch *newStitch = NULL;
  
  // attempt to get a spare segment stitch!
  if (SCORECUSTOMSTACK_LENGTH(spareSegmentStitchList) > 0) {
    SCORECUSTOMSTACK_POP(spareSegmentStitchList, newStitch);
  } else {
    if (VERBOSEDEBUG || DEBUG) {
      cout << "SCHED: RAN OUT OF SPARE STITCH SEGMENTS! " <<
	"INSTANTIATING A NEW ONE!" << endl;
    }
    
    newStitch = new ScoreSegmentStitch(SCORE_DATASEGMENTBLOCK_LOADSIZE, 8,
			     NULL, NULL);
    newStitch->reset();
  }
  
  newStitch->recycle(SCORE_DATASEGMENTBLOCK_LOADSIZE/(SCORE_ALIGN_BITWIDTH_TO_8(attachedStream->get_width())/8),
		     attachedStream->get_width(),
		     attachedStream, newStream);
  
  // FIX ME! I ASSUME THAT THE PROCESS TO ASSOCIATE THIS STITCH
  //         BUFFER TO IS THE SAME AS THE CURRENTNODE.
  newStitch->sched_parentProcess = currentNode->sched_parentProcess;
  
  newStitch->sched_residentStart = 0;
  newStitch->sched_maxAddr = newStitch->length();
  newStitch->sched_residentLength = newStitch->length();
  
  // fix the input list of the current cluster.
  // NOTE: cannot risk disturbing the linked list structure while
  //       traversing it!
  SCORECUSTOMLIST_ASSIGN(cluster->inputList, clusterInputNum, newStream);
  
  // add the new stitch buffer to the stitch buffer list.
  SCORECUSTOMLIST_APPEND(stitchBufferList, newStitch);
  SCORECUSTOMLIST_APPEND(newStitch->sched_parentProcess->stitchBufferList, newStitch);
  newStitch->sched_parentProcess->numSegments++;
  
  newStitch->sched_old_mode = newStitch->sched_mode =
    SCORE_CMB_SEQSRCSINK;
  
  return newStitch;
}
							     
//-------------------------------------------------------------------------
// ScoreSchedulerRandom::insertStitchBufferOnOutput
//     given the following arguments:
// |------------|
// |  cluster   |-----attachedStream--->
// |------------|
//     insert a stitch buffer in between and return its pointer:
// |------------|                    .------.
// |  cluster   |-----newStream---->< stitch >-----attachedStream--->
// |------------|                    `------'
//
//-------------------------------------------------------------------------
ScoreSegmentStitch*
ScoreSchedulerRandom::insertStitchBufferOnOutput(ScoreCluster *cluster,
						 unsigned int clusterOutputNum,
						 SCORE_STREAM attachedStream)
{
  ScoreStreamStitch *newStream = NULL;
  ScoreGraphNode *currentNode = attachedStream->sched_src;
  int oldAttachedStreamSrcNum = attachedStream->sched_srcNum;
  ScoreStreamType *oldAttachedStreamSrcType =
    currentNode->outputType(oldAttachedStreamSrcNum);
  ScoreStream *oldStream = 
    currentNode->getSchedOutput(oldAttachedStreamSrcNum);
  
  // attempt to get a spare stream stitch!
  if (SCORECUSTOMSTACK_LENGTH(spareStreamStitchList) > 0) {
    ScoreStream *tempStream;
    
    SCORECUSTOMSTACK_POP(spareStreamStitchList, tempStream);
    newStream = (ScoreStreamStitch *) tempStream;
    
    newStream->recycle(attachedStream->get_width(),
		       attachedStream->get_fixed(),
		       attachedStream->get_length(),
		       attachedStream->get_type());
  } else {
    if (VERBOSEDEBUG || DEBUG) {
      cout << "SCHED: RAN OUT OF SPARE STITCH STREAMS! " <<
	"INSTANTIATING A NEW ONE!" << endl;
    }
    
    newStream = new ScoreStreamStitch(attachedStream->get_width(),
				      attachedStream->get_fixed(),
				      attachedStream->get_length(),
				      attachedStream->get_type());
    
    newStream->reset();
    newStream->sched_spareStreamStitchList = spareStreamStitchList;
  }
  
  newStream->producerClosed = oldStream->producerClosed;
  oldStream->producerClosed = 0;
  newStream->producerClosed_hw = oldStream->producerClosed_hw;
  oldStream->producerClosed_hw = 0;
  newStream->sched_isCrossCluster = oldStream->sched_isCrossCluster;
  
  currentNode->unbindSchedOutput(oldAttachedStreamSrcNum);
  currentNode->bindSchedOutput(oldAttachedStreamSrcNum,
			       newStream,
			       oldAttachedStreamSrcType);
  
  // FIX ME! IS THIS THE CORRECT WAY TO SPECIFY ABSOLUTE
  //         SEGMENT SIZE?
  ScoreSegmentStitch *newStitch = NULL;
  
  // attempt to get a spare segment stitch!
  if (SCORECUSTOMSTACK_LENGTH(spareSegmentStitchList) > 0) {
    SCORECUSTOMSTACK_POP(spareSegmentStitchList, newStitch);
  } else {
    if (VERBOSEDEBUG || DEBUG) {
      cout << "SCHED: RAN OUT OF SPARE STITCH SEGMENTS! " <<
	"INSTANTIATING A NEW ONE!" << endl;
    }
    
    newStitch = 
      new ScoreSegmentStitch(SCORE_DATASEGMENTBLOCK_LOADSIZE, 8,
			     NULL, NULL);
    newStitch->reset();
  }
  
  newStitch->recycle(SCORE_DATASEGMENTBLOCK_LOADSIZE/(SCORE_ALIGN_BITWIDTH_TO_8(attachedStream->get_width())/8),
		     attachedStream->get_width(),
		     newStream, attachedStream);
  
  // FIX ME! I ASSUME THAT THE PROCESS TO ASSOCIATE THIS STITCH
  //         BUFFER TO IS THE SAME AS THE CURRENTNODE.
  newStitch->sched_parentProcess = currentNode->sched_parentProcess;
  
  newStitch->sched_residentStart = 0;
  newStitch->sched_maxAddr = newStitch->length();
  newStitch->sched_residentLength = newStitch->length();
  
  // fix the output list of the current cluster.
  // NOTE: cannot risk disturbing the linked list structure while
  //       traversing it!
  SCORECUSTOMLIST_ASSIGN(cluster->outputList, clusterOutputNum, newStream);
  
  // add the new stitch buffer to the stitch buffer list.
  SCORECUSTOMLIST_APPEND(stitchBufferList, newStitch);
  SCORECUSTOMLIST_APPEND(newStitch->sched_parentProcess->stitchBufferList, newStitch);
  newStitch->sched_parentProcess->numSegments++;
  
  newStitch->sched_old_mode = newStitch->sched_mode =
    SCORE_CMB_SEQSRCSINK;
  
  return newStitch;
}



///////////////////////////////////////////////////////////////////////////////
// ScoreSchedulerRandom::doSchedule:
//   Perform scheduling of the array.
//
// Parameters: none.
//
// Return value: none.
///////////////////////////////////////////////////////////////////////////////
void ScoreSchedulerRandom::doSchedule() {
  unsigned int i;
#if 0
#if DOPROFILING
  unsigned long long doScheduleStartClock, doScheduleEndClock, 
    doScheduleDiffClock;
#endif
#else
#if DOPROFILING
  unsigned long long doScheduleDiffClock;
#endif
#endif

#if DOSCHEDULE_FOR_SCHEDTIME
  unsigned long long doScheduleDiffClock;
#endif

  // get a lock on the scheduler data mutex.
  pthread_mutex_lock(&schedulerDataMutex);

  if (1|| VERBOSEDEBUG || DEBUG || DOPROFILING || PRINTSTATE) {
    if (!isPseudoIdeal) {
      cerr << "SCHED: Starting doSchedule()" << endl;
      cerr << "SCHED: SCHEDULER VIRTUAL TIME: " << schedulerVirtualTime << endl;
    } else {
      if ((schedulerVirtualTime % 100000) == 0) {
        cerr << "SCHED: SCHEDULER VIRTUAL TIME: " << schedulerVirtualTime << endl;
      }
    }
  }

#if 0
#if DOPROFILING
  doScheduleStartClock = threadCounter->ScoreThreadSchCounterRead();
#endif
#endif

  currentTimeslice++;
  if (VERBOSEDEBUG || DEBUG || DOPROFILING || PRINTSTATE) {
    cerr << "SCHED: Current timeslice: " << currentTimeslice << endl;
  }

  if (VISUALIZE_STATE)
    visualizeCurrentState();

  // check to see if the scheduler was reawakened (simulation only).
  if (isReawakening) {
    cerr << "SCHED: SCHEDULER REAWOKEN AT CYCLE = " << schedulerVirtualTime <<
      endl;
    lastReawakenTime = schedulerVirtualTime;

    isReawakening = 0;
  }
  
  // get the current status of the array.
  if (VERBOSEDEBUG || DEBUG) {
    cerr << "SCHED:    getCurrentStatus()" << endl;
  }
  getCurrentStatus();

  // gather status info for the array and store in scheduler data structures.
  if (VERBOSEDEBUG || DEBUG) {
    cerr << "SCHED:    gatherStatusInfo()" << endl;
  }
  gatherStatusInfo();

  // find the done pages/segments.
  if (VERBOSEDEBUG || DEBUG) {
    cerr << "SCHED:    findDonePagesSegments()" << endl;
  }
  findDonePagesSegments();

  if (VERBOSEDEBUG || DEBUG || PRINTSTATE) {
    cerr << "SCHED: NUMBER OF DONE NODES: " << 
      SCORECUSTOMLIST_LENGTH(doneNodeList) << endl;
    for (i = 0; i < SCORECUSTOMLIST_LENGTH(doneNodeList); i++) {
      ScoreGraphNode *currentNode;
      SCORECUSTOMLIST_ITEMAT(doneNodeList, i, currentNode);
      cerr << "SCHED:    DONE NODE: " << (unsigned int) currentNode << 
	"[cluster = " << ((unsigned int)(currentNode->sched_parentCluster)) <<
	endl;
    }
  }

  // find any memory segments that have faulted on their address.
  if (VERBOSEDEBUG || DEBUG) {
    cerr << "SCHED:    findFaultedMemSeg()" << endl;
  }
  findFaultedMemSeg();

  if (VERBOSEDEBUG || DEBUG || PRINTSTATE) {
    cerr << "SCHED: NUMBER OF FAULTED SEGMENTS: " << 
      SCORECUSTOMLIST_LENGTH(faultedMemSegList) << endl;
    for (i = 0; i < SCORECUSTOMLIST_LENGTH(faultedMemSegList); i++) {
      ScoreSegment *currentSegment;
      SCORECUSTOMLIST_ITEMAT(faultedMemSegList, i, currentSegment);
      cerr << "SCHED:    FAULTED MEMSEG: " << 
	(unsigned int) currentSegment << 
	"(TRA: " << currentSegment->traAddr << 
	" FAULTADDR: " << currentSegment->sched_faultedAddr << ")" << endl;
    }
  }

#if RANDOM_SCHEDULER_VERSION == 1
  // find the freeable clusters.
  if (VERBOSEDEBUG || DEBUG) {
    cerr << "SCHED:    findFreeableClusters()" << endl;
  }
  findFreeableClusters();

  if (VERBOSEDEBUG || DEBUG || PRINTSTATE) {
    cerr << "SCHED: NUMBER OF FREEABLE CLUSTERS: " << 
      SCORECUSTOMLIST_LENGTH(freeableClusterList) << endl;
    for (i = 0; i < SCORECUSTOMLIST_LENGTH(freeableClusterList); i++) {
      ScoreCluster *currentCluster;

      SCORECUSTOMLIST_ITEMAT(freeableClusterList, i, currentCluster);

      cerr << "SCHED:    FREEABLE CLUSTER: " << 
	(unsigned int) currentCluster << endl;
    }
  }
#endif // if RANDOM_SCHEDULER_VERSION == 1

  // deal with any potential bufferlock.
  if (VERBOSEDEBUG || DEBUG) {
    cerr << "SCHED:    dealWithDeadLock()" << endl;
  }
  if (!doNoDeadlockDetection) {
    dealWithDeadLock();
  }

  if (VERBOSEDEBUG || DEBUG || PRINTSTATE) {
    cerr << "SCHED: ===> BEFORE SCHEDULECLUSTERS()" << endl;
    printSchedState();
  }

  // perform cluster scheduling.
  if (VERBOSEDEBUG || DEBUG) {
    cerr << "SCHED:    scheduleClusters()" << endl;
  }
  scheduleClusters();

  if (VERBOSEDEBUG || DEBUG || PRINTSTATE) {
    cerr << "SCHED: ===> AFTER SCHEDULECLUSTERS()" << endl;
    printSchedState();
    printPlacementState();
  }
  // place the scheduled pages/memory segments.
  if (VERBOSEDEBUG || DEBUG) {
    cerr << "SCHED:    performPlacement()" << endl;
  }
  performPlacement();

#if SCHEDULECLUSTERS_FOR_SCHEDTIME
  advanceSimulatorTimeOffset(current_scheduleClusters);
#else
#if DOSCHEDULE_FOR_SCHEDTIME
  doScheduleDiffClock = current_gatherStatusInfo + 
    current_findDonePagesSegments +
    current_findFaultedMemSeg + current_findFreeableClusters + 
    current_dealWithDeadLock + current_scheduleClusters + 
    current_performPlacement;
  cerr << "*** DOSCHEDULE() ==> " << 
    doScheduleDiffClock <<
    " cycle(s)" << endl;

  advanceSimulatorTimeOffset(doScheduleDiffClock);
#else
  // FIX ME! THIS IS A TEMPORARY WORKAROUND FOR SUBSTITUTING IN A SCHEDULER
  //         DECISION TIME! THIS IS NECESSARILY INCORRECT!
  advanceSimulatorTimeOffset(SCORE_FAKE_SCHEDULER_TIME);
#endif
#endif

  // issue the commands necessary to reconfigure the array.
  if (VERBOSEDEBUG || DEBUG) {
    cerr << "SCHED:    issueReconfigCommands()" << endl;
  }
  issueReconfigCommands();

  if (VERBOSEDEBUG || DEBUG || PRINTSTATE) {
    cerr << "SCHED: NUMBER OF EMPTY STITCH SEGMENTS: " << 
      SCORECUSTOMLIST_LENGTH(emptyStitchList) << endl;
    for (i = 0; i < SCORECUSTOMLIST_LENGTH(emptyStitchList); i++) {
      ScoreSegmentStitch *currentStitch;

      SCORECUSTOMLIST_ITEMAT(emptyStitchList, i, currentStitch);

      cerr << "SCHED:    EMPTY STITCH: " << 
	(unsigned int) currentStitch << endl;
    }
  }

  // performs any cleanup necessary (i.e. synchronize memory segments with
  // the user process and deleting any necessary processes or clusters).
  if (VERBOSEDEBUG || DEBUG) {
    cerr << "SCHED:    performCleanup()" << endl;
  }
  performCleanup();

  // this is used for detailed testing... prints out the view of the world
  // so far and the status of the pages/segments/streams.
  if (VISUALIZE_STATE)
    visualizeCurrentState();

  if (VERBOSEDEBUG || PRINTSTATE) {
    printCurrentState();

    for (i = 0; i < numPhysicalCP; i++) {
      cerr << "SCHED: arrayCP[" << i << "]: "
	"active " << (unsigned int) arrayCP[i].active <<
	" actual " << (unsigned int) arrayCP[i].actual << endl;
    }
    for (i = 0; i < numPhysicalCMB; i++) {
      cerr << "SCHED: arrayCMB[" << i << "]: "
	"active " << (unsigned int) arrayCMB[i].active <<
	" actual " << (unsigned int) arrayCMB[i].actual << endl;
    }
  }

  // if there is no more work to do, then do not send a timeslice request.
  // otherwise, inform simulator that it can execute up to next timeslice.
  if ((SCORECUSTOMLIST_LENGTH(processList) == 0) &&
      (SCORECUSTOMLIST_LENGTH(stitchBufferList) == 0)) {
    isIdle = 1;

    cerr << endl;
    cerr << "********************************************************" << endl;
    cerr << "********************************************************" << endl;
    cerr << endl;
    cerr << "SCHED: NO MORE WORK TO DO AT CYCLE = " << schedulerVirtualTime <<
      endl;
    cerr << "SCHED: CYCLES ELAPSED FROM LAST IDLE PERIOD = " <<
      (schedulerVirtualTime-lastReawakenTime) << endl;
    cerr << endl;
    cerr << "********************************************************" << endl;
    cerr << "********************************************************" << endl;
    cerr << endl;

    if (visualization != NULL) {
      // fake in an idle state for all of the CPs and CMBs.
      for (i = 0; i < numPhysicalCP; i++) {
	visualization->addEventCP(i, VISUALIZATION_EVENT_IDLE,
				  schedulerVirtualTime);
      }
      for (i = 0; i < numPhysicalCMB; i++) {
	visualization->addEventCMB(i, VISUALIZATION_EVENT_IDLE,
				   schedulerVirtualTime);
      }

      visualization->syncVisualizationToFile();
    }
  } else {
#if VERBOSEDEBUG || DEBUG
    cerr << "SCHED: REQUESTING NEXT TIMESLICE AT: " << 
      schedulerVirtualTime << endl;
#endif
    requestNextTimeslice();
  }

#if 0
#if DOPROFILING
  doScheduleEndClock = threadCounter->ScoreThreadSchCounterRead();
  doScheduleDiffClock = doScheduleEndClock - doScheduleStartClock;
  if (doScheduleDiffClock < min_doSchedule) {
    min_doSchedule = doScheduleDiffClock;
  }
  if (doScheduleDiffClock > max_doSchedule) {
    max_doSchedule = doScheduleDiffClock;
  }
  total_doSchedule = total_doSchedule + doScheduleDiffClock;
  cerr << "*** DOSCHEDULE() ==> " << 
    doScheduleDiffClock <<
    " cycle(s)" << endl;
#endif
#else
#if DOPROFILING
  doScheduleDiffClock = current_gatherStatusInfo + 
    current_findDonePagesSegments +
    current_findFaultedMemSeg + current_findFreeableClusters + 
    current_dealWithDeadLock + current_scheduleClusters + 
    current_performPlacement;
  if (doScheduleDiffClock < min_doSchedule) {
    min_doSchedule = doScheduleDiffClock;
  }
  if (doScheduleDiffClock > max_doSchedule) {
    max_doSchedule = doScheduleDiffClock;
  }
  total_doSchedule = total_doSchedule + doScheduleDiffClock;
  cerr << "*** DOSCHEDULE() ==> " << 
    doScheduleDiffClock <<
    " cycle(s)" << endl;
#endif
#endif

  if (1|| VERBOSEDEBUG || DEBUG || DOPROFILING || PRINTSTATE) {
    if (!isPseudoIdeal) {
      cerr << "SCHED: Ending doSchedule()" << endl;
      cerr << "SCHED: Num free CPs: " << currentNumFreeCPs <<
              " CMBs: " << currentNumFreeCMBs << endl;
#if DOPROFILING || DOPROFILING_SCHEDULECLUSTERS
      {
	unsigned int usedCPs = numPhysicalCP - currentNumFreeCPs;
	unsigned int usedCMBs = numPhysicalCMB - currentNumFreeCMBs;
	total_usedCPs += usedCPs; // used to compute averages
	total_usedCMBs += usedCMBs;
	// prevent the last timeslice from affecting max/min results
	if ((usedCPs != 0) || (usedCMBs != 0)) {
	  if (usedCPs < min_usedCPs) min_usedCPs = usedCPs;
	  if (usedCMBs < min_usedCMBs) min_usedCMBs = usedCMBs;
	  if (usedCPs > max_usedCPs) max_usedCPs = usedCPs;
	  if (usedCMBs > max_usedCMBs) max_usedCMBs = usedCMBs;
	}
      }
#endif
    }
  }

#if DOPROFILING
  if (isIdle) {
#if 0
    unsigned long long totalCycles =
      total_getCurrentStatus + total_gatherStatusInfo +
      total_findDonePagesSegments + total_findFaultedMemSeg +
      total_findFreeableClusters + total_dealWithDeadLock +
      total_scheduleClusters + total_performPlacement +
      total_issueReconfigCommands + total_performCleanup;
#else
    unsigned long long totalCycles = total_doSchedule;
#endif
    
    cerr << "SCHED: =================================================" << endl;
#if 0
    cerr << "SCHED: Total getCurrentStatus(): " <<
      total_getCurrentStatus << " cycle(s) " << 
      "[" << min_getCurrentStatus << " : " <<
      total_getCurrentStatus/currentTimeslice << " : " <<
      max_getCurrentStatus << "] " <<
      "[" << (((double) total_getCurrentStatus)/totalCycles)*100 << 
      "]%" << endl;
#endif
    cerr << "SCHED: Total gatherStatusInfo(): " <<
      total_gatherStatusInfo << " cycle(s) " << 
      "[" << min_gatherStatusInfo << " : " <<
      total_gatherStatusInfo/currentTimeslice << " : " <<
      max_gatherStatusInfo << "] " <<
      "[" << (((double) total_gatherStatusInfo)/totalCycles)*100 << 
      "%]" << endl;
    cerr << "SCHED: Total findDonePagesSegments(): " <<
      total_findDonePagesSegments << " cycle(s) " << 
      "[" << min_findDonePagesSegments << " : " <<
      total_findDonePagesSegments/currentTimeslice << " : " <<
      max_findDonePagesSegments << "] " <<
      "[" << (((double) total_findDonePagesSegments)/totalCycles)*100 << 
      "%]" << endl;
    cerr << "SCHED: Total findFaultedMemSeg(): " <<
      total_findFaultedMemSeg << " cycle(s) " << 
      "[" << min_findFaultedMemSeg << " : " <<
      total_findFaultedMemSeg/currentTimeslice << " : " <<
      max_findFaultedMemSeg << "] " <<
      "[" << (((double) total_findFaultedMemSeg)/totalCycles)*100 << 
      "%]" << endl;
    cerr << "SCHED: Total findFreeableClusters(): " <<
      total_findFreeableClusters << " cycle(s) " << 
      "[" << min_findFreeableClusters << " : " <<
      total_findFreeableClusters/currentTimeslice << " : " <<
      max_findFreeableClusters << "] " <<
      "[" << (((double) total_findFreeableClusters)/totalCycles)*100 << 
      "%]" << endl;
    cerr << "SCHED: Total dealWithDeadLock(): " <<
      total_dealWithDeadLock << " cycle(s) " << 
      "[" << min_dealWithDeadLock << " : " <<
      total_dealWithDeadLock/currentTimeslice << " : " <<
      max_dealWithDeadLock << "] " <<
      "[" << (((double) total_dealWithDeadLock)/totalCycles)*100 << 
      "%]" << endl;
    cerr << "SCHED: Total scheduleClusters(): " <<
      total_scheduleClusters << " cycle(s) " << 
      "[" << min_scheduleClusters << " : " <<
      total_scheduleClusters/currentTimeslice << " : " <<
      max_scheduleClusters << "] " <<
      "[" << (((double) total_scheduleClusters)/totalCycles)*100 << 
      "%]" << endl;
    cerr << "SCHED: Total performPlacement(): " <<
      total_performPlacement << " cycle(s) " << 
      "[" << min_performPlacement << " : " <<
      total_performPlacement/currentTimeslice << " : " <<
      max_performPlacement << "] " <<
      "[" << (((double) total_performPlacement)/totalCycles)*100 << 
      "%]" << endl;
#if 0
    cerr << "SCHED: Total issueReconfigCommands(): " <<
      total_issueReconfigCommands << " cycle(s) " << 
      "[" << min_issueReconfigCommands << " : " <<
      total_issueReconfigCommands/currentTimeslice << " : " <<
      max_issueReconfigCommands << "] " <<
      "[" << (((double) total_issueReconfigCommands)/totalCycles)*100 << 
      "%]" << endl;
    cerr << "SCHED: Total performCleanup(): " <<
      total_performCleanup << " cycle(s) " << 
      "[" << min_performCleanup << " : " <<
      total_performCleanup/currentTimeslice << " : " <<
      max_performCleanup << "] " <<
      "[" << (((double) total_performCleanup)/totalCycles)*100 << 
      "%]" << endl;
#endif
    cerr << "SCHED: *** TOTAL DOSCHEDULE(): " <<
      total_doSchedule << " cycle(s) " <<
      "[" << min_doSchedule << " : " <<
      total_doSchedule/currentTimeslice << " : " <<
      max_doSchedule << "] " <<
      "[" << (((double) total_doSchedule)/totalCycles)*100 << 
      "%]" << endl;
    cerr << "SCHED: =================================================" << endl;
  }
#endif
#if DOPROFILING_SCHEDULECLUSTERS
  if (isIdle) {
    unsigned long long totalCycles =
      total_scheduleClusters_handleDoneNodes[PROF_CAT_EXECCYCLES] +
      total_scheduleClusters_copyResidentToWaiting[PROF_CAT_EXECCYCLES] +
      total_scheduleClusters_schedule[PROF_CAT_EXECCYCLES] +
      total_scheduleClusters_placeStitches[PROF_CAT_EXECCYCLES]+
      total_scheduleClusters_removeUnscheduledNodes[PROF_CAT_EXECCYCLES];

    cerr << "SCHED: =================================================" << endl;
    PRINT_SCHEDULECLUSTERS_PROFILE(handleDoneNodes);
    PRINT_SCHEDULECLUSTERS_PROFILE(copyResidentToWaiting);
    PRINT_SCHEDULECLUSTERS_PROFILE(schedule);
    scheduleClusters_requiredResources_perItemCount = scheduleClusters_schedule_perItemCount;
    PRINT_SCHEDULECLUSTERS_PROFILE(requiredResources);
    scheduleClusters_moveScheduledNodes_perItemCount = scheduleClusters_schedule_perItemCount;
    PRINT_SCHEDULECLUSTERS_PROFILE(moveScheduledNodes);
    PRINT_SCHEDULECLUSTERS_PROFILE(placeStitches);
    PRINT_SCHEDULECLUSTERS_PROFILE(removeUnscheduledNodes);
    cerr << "SCHED: =================================================" << endl;
  }
#endif

#if DOPROFILING || DOPROFILING_SCHEDULECLUSTERS
  if (isIdle) {
    cerr << "usedCPs: [ " << min_usedCPs << " : " << 
      total_usedCPs / (currentTimeslice - 1) << " : " << max_usedCPs << " ]\n";
    cerr << "usedCMBs: [ " << min_usedCMBs << " : " << 
      total_usedCMBs / (currentTimeslice - 1) << " : " << max_usedCMBs << " ]\n";
    cerr << "SCHED: =================================================" << endl;
  }
#endif

#if KEEPRECONFIGSTATISTICS
  if (isIdle) {
    cerr << "SCHED: =================================================" << endl;
    cerr << "Number of stopPage() calls: " << 
      total_stopPage << endl;
    cerr << "Number of startPage() calls: " << 
      total_startPage << endl;
    cerr << "Number of stopSegment() calls: " << 
      total_stopSegment << endl;
    cerr << "Number of startSegment() calls: " << 
      total_startSegment << endl;
    cerr << "Number of dumpPageState() calls: " << 
      total_dumpPageState << endl;
    cerr << "Number of dumpPageFIFO() calls: " << 
      total_dumpPageFIFO << endl;
    cerr << "Number of loadPageConfig() calls: " << 
      total_loadPageConfig << endl;
    cerr << "Number of loadPageState() calls: " << 
      total_loadPageState << endl;
    cerr << "Number of loadPageFIFO() calls: " << 
      total_loadPageFIFO << endl;
    cerr << "Number of getSegmentPointers() calls: " << 
      total_getSegmentPointers << endl;
    cerr << "Number of dumpSegmentFIFO() calls: " << 
      total_dumpSegmentFIFO << endl;
    cerr << "Number of setSegmentConfigPointers() calls: " << 
      total_setSegmentConfigPointers << endl;
    cerr << "Number of changeSegmentMode() calls: " << 
      total_changeSegmentMode << endl;
    cerr << "Number of changeSegmentTRAandPBOandMAX() calls: " << 
      total_changeSegmentTRAandPBOandMAX << endl;
    cerr << "Number of resetSegmentDoneFlag() calls: " << 
      total_resetSegmentDoneFlag << endl;
    cerr << "Number of loadSegmentFIFO() calls: " << 
      total_loadSegmentFIFO << endl;
    cerr << "Number of memXferPrimaryToCMB() calls: " << 
      total_memXferPrimaryToCMB << endl;
    cerr << "Number of memXferCMBToPrimary() calls: " << 
      total_memXferCMBToPrimary << endl;
    cerr << "Number of memXferCMBToCMB() calls: " << 
      total_memXferCMBToCMB << endl;
    cerr << "Number of connectStream() calls: " << 
      total_connectStream << endl;
    cerr << "SCHED: =================================================" << endl;
  }
#endif

  // if we are idle and are to exit when idle, then do so!
  if (isIdle && doExitOnIdle) {
    cerr << "SCHED: EXITING ON IDLE AS REQUESTED!" << endl;
    simulator->printStats();
    exit(0);
  }

  // release the lock on the scheduler data mutex.
  pthread_mutex_unlock(&schedulerDataMutex);
}


///////////////////////////////////////////////////////////////////////////////
// ScoreSchedulerRandom::scheduleClusters:
//   Given the list of done pages/segments, freeable clusters, 
//     ready clusters, and waiting clusters, decide which clusters should 
//     remain on the array and which clusters should be removed from the 
//     array. It will fill in the following lists with the result: 
//     scheduledPageList, scheduledMemSegList, removedPageList, 
//     removedMemSegList. The appropriate clusters will be moved to/from 
//     waitingClusterList, scheduledClusterList, removedClusterList,
//     residentClusterList.
//
//     Basic plan:
//     (0) sanity checks
//     (1) remove done nodes from waitingClusterList, residentClusterList,
//         and residentStitchList
//     (2) to make picking a random cluster easier, copy the contents of
//         residentClusterList to waitingClusterList
//     (3) compute available resources
//     (4) try to schedule as many clusters as resources permit, keeping
//         track of available resources
//         (a) any scheduled cluster is placed on the scheduledClusterList
//             and its pages/segments are added to the scheduledPagesList/
//             scheduledMemSegList
//         (b) existing stitchBuffers are accounted for
//     (5) go though all scheduled Clusters and place stitch buffers where
//         needed. 
//         (a) newly added stitch buffers are placed into scheduledMemSegList
//         (b) stitch buffers that are already resident may need to have
//             their mode changed (they are placed into
//             configChangedStitchSegList).
//     (6) go through all nodes on the residentClusterList and
//         residentStitchList
//         (a) if a node that used to be resident was scheduled this timeslice
//             place it on the remove list.
//     (7) clear the residentLists (others should be cleared after placement
//         and reconfiguration [look at the sanity checks]).
//     (8) swap list, by interchanging pointers, i.e. the scheduledList becomes
//         residentlist, and vice versa.
//
// Parameters: none.
//
// Return value: none.
///////////////////////////////////////////////////////////////////////////////
void ScoreSchedulerRandom::scheduleClusters() {
  int numFreePage, numFreeMemSeg;
  SCORECUSTOMLINKEDLISTITEM it;
  unsigned int i;
  unsigned int prevTimeslice = currentTimeslice - 1;
#if 0
  unsigned int iterNum = 0;
#endif

#if DOPROFILING_SCHEDULECLUSTERS
  if (PROFILE_VERBOSE)
    cerr << "==========> SCHEDULECLUSTERS BEGIN:" << endl;
  unsigned long long startTime[PROF_CAT_COUNT], endTime[PROF_CAT_COUNT], diffTime, diffTime_perItem = 0;
  unsigned long long current_scheduleClusters[PROF_CAT_COUNT],
    current_scheduleClusters_handleDoneNodes[PROF_CAT_COUNT],
    current_scheduleClusters_copyResidentToWaiting[PROF_CAT_COUNT],
    current_scheduleClusters_schedule[PROF_CAT_COUNT],
    current_scheduleClusters_placeStitches[PROF_CAT_COUNT],
    current_scheduleClusters_removeUnscheduledNodes[PROF_CAT_COUNT]; 

  unsigned long long current_scheduleClusters_requiredResources[PROF_CAT_COUNT] = {0, 0, 0};
  unsigned long long extra_startTime[PROF_CAT_COUNT] = {0, 0, 0}, extra_endTime[PROF_CAT_COUNT] = {0, 0, 0};
  unsigned long long current_scheduleClusters_moveScheduledNodes[PROF_CAT_COUNT] = {0, 0, 0};
  unsigned long long extra1_startTime[PROF_CAT_COUNT] = {0, 0, 0}, extra1_endTime[PROF_CAT_COUNT] = {0, 0, 0};
  unsigned long long profilerItemCount;
#endif

#if DOPROFILING
  startClock = threadCounter->ScoreThreadSchCounterRead();
#endif

#if SCHEDULECLUSTERS_FOR_SCHEDTIME
  startClock = threadCounter->ScoreThreadSchCounterRead();
#endif

#if DOSCHEDULE_FOR_SCHEDTIME
  startClock = threadCounter->ScoreThreadSchCounterRead();
#endif

#if DOPROFILING_SCHEDULECLUSTERS
  threadCounter->DCUmissRead(SCHEDULER,
			     startTime[PROF_CAT_MEMREFS],
			     startTime[PROF_CAT_MISSCYCLES]);
  startTime[PROF_CAT_EXECCYCLES] = threadCounter->ScoreThreadSchCounterRead();
#endif

  // (0) sanity checks

  assert(SCORECUSTOMLINKEDLIST_LENGTH(scheduledClusterList) == 0);
  assert(SCORECUSTOMLINKEDLIST_LENGTH(scheduledStitchList) == 0);

  assert(SCORECUSTOMLIST_LENGTH(scheduledPageList) == 0);
  assert(SCORECUSTOMLIST_LENGTH(scheduledMemSegList) == 0);
  assert(SCORECUSTOMLIST_LENGTH(removedPageList) == 0);
  assert(SCORECUSTOMLIST_LENGTH(removedMemSegList) == 0);
  assert(SCORECUSTOMLIST_LENGTH(doneNotRemovedPageList) == 0);
  assert(SCORECUSTOMLIST_LENGTH(doneNotRemovedMemSegList) == 0);
  assert(SCORECUSTOMLIST_LENGTH(configChangedStitchSegList) == 0);


  // (1) remove done nodes from waitingClusterList, residentClusterList.
  //     Do not bother with the residentStitchList, since any items that
  //     have not been rescheduled (i.e. are not on the scheduledStitchList,
  //     but are on the residentStitchList) will be placed in the
  //     removedMemSegList.

  // first go through all done nodes, and adjust the numbers in the
  // clusters and processes they belong to, therefore, that allows
  // to determine which clusters are done

  // the following is a small optimization to prevent unecessary list traversals
  unsigned int numDoneNodes = SCORECUSTOMLIST_LENGTH(doneNodeList);

#if DOPROFILING_SCHEDULECLUSTERS
  profilerItemCount = numDoneNodes;
#endif

  for (i = 0; i < SCORECUSTOMLIST_LENGTH(doneNodeList); i ++) {
    ScoreGraphNode *currentNode;

    SCORECUSTOMLIST_ITEMAT(doneNodeList, i, currentNode);

    ScoreCluster *parentCluster;
    if ((parentCluster = currentNode->sched_parentCluster)) {
      parentCluster->removeNode(currentNode);
      if ((parentCluster->numPages == 0) && (parentCluster->numSegments == 0)) {
	SCORECUSTOMLIST_APPEND(doneClusterList, parentCluster);
	// if the cluster is resident, remove it
	if (parentCluster->getLastTimesliceScheduled() == prevTimeslice) {
	  assert(parentCluster->sched_residentClusterListItem != 
		 SCORECUSTOMLINKEDLIST_NULL);
#ifndef NDEBUG
	  ScoreCluster *tmpCluster;
	  SCORECUSTOMLINKEDLIST_ITEMAT(residentClusterList,
				       parentCluster->sched_residentClusterListItem,
				       tmpCluster);
	  assert(tmpCluster == parentCluster);
#endif
	  SCORECUSTOMLINKEDLIST_DELITEM(residentClusterList,
					parentCluster->sched_residentClusterListItem);
	  // at least one node got eliminated from possibly being in waiting list
	} else {
	  // this will be taken care of after (removal of done clusters
	  // from the waitingClusterList)
	}
	parentCluster->sched_isDone = 1;
      }
    }

    // if node is currently resident (we haven't reconfigured yet)
    if (currentNode->getLastTimesliceScheduled() == prevTimeslice) {
      if (currentNode->isPage()) {
	SCORECUSTOMLIST_APPEND(removedPageList, (ScorePage*) currentNode);
      } else {
	SCORECUSTOMLIST_APPEND(removedMemSegList, (ScoreSegment*) currentNode);
	if (((ScoreSegment*) currentNode)->sched_isStitch) {
	  ScoreSegmentStitch *stitch = (ScoreSegmentStitch*) currentNode;
	  if (!stitch->sched_parentCluster) {
	    SCORECUSTOMLINKEDLIST_DELITEM(residentStitchList,
					  stitch->sched_residentStitchListItem);
	  }
	}
      }
    } else {
      if (currentNode->isPage()) {
	SCORECUSTOMLIST_APPEND(doneNotRemovedPageList,
			       (ScorePage*) currentNode);
      } else {
	SCORECUSTOMLIST_APPEND(doneNotRemovedMemSegList,
			       (ScoreSegment*) currentNode);
      }
    }
  }

  if (EXTRA_DEBUG) {
    cerr << "AFTER DONE NODE LIST WAS PROCESSED:\n";
    cerr << "removedPageList size = " << SCORECUSTOMLIST_LENGTH(removedPageList) << endl;
    cerr << "removedMemSegList size = " << SCORECUSTOMLIST_LENGTH(removedMemSegList) << endl;
    cerr << "doneNotRemovedPageList size = " << SCORECUSTOMLIST_LENGTH(doneNotRemovedPageList) << endl;
    cerr << "doneNotRemovedMemSegList size = " << SCORECUSTOMLIST_LENGTH(doneNotRemovedPageList) << endl;
  }
    
  // now go through waiting _cluster_ lists and prune any done clusters out
  for (unsigned int listIndex = 0;
       numDoneNodes && listIndex < SCORECUSTOMLIST_LENGTH(waitingClusterList); ) {
    ScoreCluster *currentCluster;
    SCORECUSTOMLIST_ITEMAT(waitingClusterList, listIndex, currentCluster);
    if (currentCluster->sched_isDone) {
      SCORECUSTOMLIST_REMOVEITEMAT(waitingClusterList, listIndex);
      numDoneNodes --;
    } else {
      listIndex ++; // advance the index iff the list was NOT modified
    }
  }

  PROFILE_SCHEDULECLUSTERS(handleDoneNodes,"HANDLE DONE NODES",profilerItemCount);

#if DOPROFILING_SCHEDULECLUSTERS
  profilerItemCount = SCORECUSTOMLINKEDLIST_LENGTH(residentClusterList);
#endif

  // (2) to make picking a random cluster easier, copy the contents of
  //     residentClusterList to waitingClusterList
  
  SCORECUSTOMLINKEDLIST_HEAD(residentClusterList, it);
  while (it != SCORECUSTOMLINKEDLIST_NULL) {
    ScoreCluster *currentCluster;
    SCORECUSTOMLINKEDLIST_ITEMAT(residentClusterList, it, currentCluster);
    assert(!(currentCluster->sched_isDone));
    SCORECUSTOMLIST_APPEND(waitingClusterList, currentCluster);
    SCORECUSTOMLINKEDLIST_GOTONEXT(residentClusterList, it);
  }
 
  // (3) compute available resources

  // assume entire array is available
  numFreePage = numPhysicalCP;
  numFreeMemSeg = numPhysicalCMB;

  PROFILE_SCHEDULECLUSTERS(copyResidentToWaiting,
			   "COPY RES CLUSTERS TO WAIT LIST",
			   profilerItemCount);

#if DOPROFILING_SCHEDULECLUSTERS
  profilerItemCount = 0;
#endif

  // (4) try to schedule as many clusters as resources permit, keeping
  //     track of available resources
  //     (a) any scheduled cluster is placed on the scheduledClusterList
  //         and its pages/segments are added to the scheduledPagesList/
  //         scheduledMemSegList
  //     (b) existing stitchBuffers are accounted for

  int deltaCPs = 0, deltaCMBs = 0;
  unsigned int random_index = 0u;
  ScoreCluster *currentCluster = 0;
  SCORECUSTOMLIST_CLEAR(stitchListToMarkScheduled);

  while ((numFreePage + deltaCPs >= 0) && (numFreeMemSeg + deltaCMBs >= 0)) {
    if (currentCluster) {
#if DOPROFILING_SCHEDULECLUSTERS
      profilerItemCount ++;
      extra1_startTime[PROF_CAT_EXECCYCLES] = threadCounter->ScoreThreadSchCounterRead();
#endif
      numFreePage += deltaCPs;
      numFreeMemSeg += deltaCMBs;
      SCORECUSTOMLIST_REMOVEITEMAT(waitingClusterList, random_index);

      // if this cluster is resident, then move it to the scheduled list,
      // however, no reconfiguration is required.
      // used the getLastTimesliceScheduled since we have not set the new
      // timeslice scheduled for clusters yet (it will be done right after).
      if (currentCluster->getLastTimesliceScheduled() == prevTimeslice) {
	assert(currentCluster->sched_residentClusterListItem !=
	       SCORECUSTOMLINKEDLIST_NULL);
#ifndef NDEBUG
	ScoreCluster *tmpCluster;
	SCORECUSTOMLINKEDLIST_ITEMAT(residentClusterList,
				     currentCluster->sched_residentClusterListItem,
				     tmpCluster);
	assert(tmpCluster == currentCluster);
#endif
	SCORECUSTOMLINKEDLIST_DELITEM(residentClusterList,
				      currentCluster->sched_residentClusterListItem);
      } else { // otherwise, schedule each node in the cluster
	unsigned int numNodes = 
	  currentCluster->numPages + currentCluster->numSegments;
	
	for (unsigned int i = 0; i < numNodes; i ++) {
	  ScoreGraphNode *mynode;
	  SCORECUSTOMLIST_ITEMAT(currentCluster->nodeList, i, mynode);
	  if (mynode->isPage()) {
	    SCORECUSTOMLIST_APPEND(scheduledPageList, (ScorePage*) mynode);
	  } else {
	    SCORECUSTOMLIST_APPEND(scheduledMemSegList, (ScoreSegment*) mynode);
	  }
	}
      }
      
      currentCluster->setLastTimesliceScheduled(currentTimeslice);
      SCORECUSTOMLINKEDLIST_APPEND(scheduledClusterList, currentCluster, 
				   currentCluster->sched_residentClusterListItem);
      
      for (unsigned int listIndex = 0;
	   listIndex < SCORECUSTOMLIST_LENGTH(stitchListToMarkScheduled);
	   listIndex ++) {
	ScoreSegmentStitch *currentStitch;
	SCORECUSTOMLIST_ITEMAT(stitchListToMarkScheduled, listIndex,
			       currentStitch);
	currentStitch->setLastTimesliceScheduled(currentTimeslice);
      }
      SCORECUSTOMLIST_CLEAR(stitchListToMarkScheduled);
#if 0
      cerr << "Initial scheduling iteration " << iterNum << " (" << currentTimeslice << ") --> freeCPs = " << numFreePage << " freeCMBs = " << numFreeMemSeg <<"\n";
      visualizeCurrentState();
      iterNum ++;
#endif

#if DOPROFILING_SCHEDULECLUSTERS
      extra1_endTime[PROF_CAT_EXECCYCLES] = threadCounter->ScoreThreadSchCounterRead();
      current_scheduleClusters_moveScheduledNodes[PROF_CAT_EXECCYCLES] += 
	extra1_endTime[PROF_CAT_EXECCYCLES] - extra1_startTime[PROF_CAT_EXECCYCLES];
#endif
    }

    if (SCORECUSTOMLIST_ISEMPTY(waitingClusterList))
      break;

    random_index = (unsigned int)(((double)random()) / ((double)RAND_MAX) *
				  SCORECUSTOMLIST_LENGTH(waitingClusterList));

#if DOPROFILING_SCHEDULECLUSTERS
    extra_startTime[PROF_CAT_EXECCYCLES] = threadCounter->ScoreThreadSchCounterRead();
#endif

    // determine resource requirements for this cluster
    SCORECUSTOMLIST_ITEMAT(waitingClusterList, random_index, currentCluster);
    
    deltaCPs = - currentCluster->numPages;    // set initial values
    deltaCMBs = - currentCluster->numSegments;

    for (i = 0; i < SCORECUSTOMLIST_LENGTH(currentCluster->inputList); i++) {
      SCORE_STREAM currentStream;
      SCORECUSTOMLIST_ITEMAT(currentCluster->inputList, i, currentStream);
      
      if (!(currentStream->sched_srcIsDone)) {
	if (recomputeDeltaCMB(currentStream->sched_srcFunc,
			      currentStream->sched_src,
			      &deltaCMBs, currentCluster)) {
	  SCORECUSTOMLIST_APPEND(stitchListToMarkScheduled,
				 (ScoreSegmentStitch*) currentStream->sched_src);
	}
      }
    }
    
    for (i = 0; i < SCORECUSTOMLIST_LENGTH(currentCluster->outputList); i++) {
      SCORE_STREAM currentStream;
      SCORECUSTOMLIST_ITEMAT(currentCluster->outputList, i, currentStream);
      
      if (!(currentStream->sched_sinkIsDone)) {
	if (recomputeDeltaCMB(currentStream->sched_snkFunc,
			      currentStream->sched_sink,
			      &deltaCMBs, currentCluster)) {
	  SCORECUSTOMLIST_APPEND(stitchListToMarkScheduled,
				 (ScoreSegmentStitch*) currentStream->sched_sink);
	}
      }
    }
#if DOPROFILING_SCHEDULECLUSTERS
    extra_endTime[PROF_CAT_EXECCYCLES] = threadCounter->ScoreThreadSchCounterRead();
    current_scheduleClusters_requiredResources[PROF_CAT_EXECCYCLES] += 
      extra_endTime[PROF_CAT_EXECCYCLES] - extra_startTime[PROF_CAT_EXECCYCLES];
#endif
  }
  if (EXTRA_DEBUG) {
    cerr << "After initial scheduling:\n";
    cerr << "numFreePage = " << numFreePage << endl;
    cerr << "numFreeMemSeg = " << numFreeMemSeg << endl;
    cerr << "scheduledPageList size = " << SCORECUSTOMLIST_LENGTH(scheduledPageList) << endl;
    cerr << "scheduledMemSegList size = " << SCORECUSTOMLIST_LENGTH (scheduledMemSegList) << endl;
  }

  PROFILE_SCHEDULECLUSTERS(schedule,"SCHEDULE",profilerItemCount);

#if DOPROFILING_SCHEDULECLUSTERS
  {
    unsigned long long t1 = current_scheduleClusters_moveScheduledNodes[PROF_CAT_EXECCYCLES];
    total_scheduleClusters_moveScheduledNodes [PROF_CAT_EXECCYCLES] += t1;
    if (t1 < min_scheduleClusters_moveScheduledNodes[PROF_CAT_EXECCYCLES])
      min_scheduleClusters_moveScheduledNodes[PROF_CAT_EXECCYCLES] = t1;
    if (t1 > max_scheduleClusters_moveScheduledNodes[PROF_CAT_EXECCYCLES])
      max_scheduleClusters_moveScheduledNodes[PROF_CAT_EXECCYCLES] = t1;
    
    if (PROFILE_VERBOSE) 
      cerr << "++++ MoveScheduledNodes -- " << t1;

    if (profilerItemCount) {
      diffTime_perItem = t1 / profilerItemCount;
      if (PROFILE_VERBOSE)
	cerr << " -- [" << diffTime_perItem << "/item]";
      total_scheduleClusters_moveScheduledNodes_perItem[PROF_CAT_EXECCYCLES] +=diffTime_perItem;
      if (diffTime_perItem < min_scheduleClusters_moveScheduledNodes_perItem[PROF_CAT_EXECCYCLES])
	min_scheduleClusters_moveScheduledNodes_perItem[PROF_CAT_EXECCYCLES] = diffTime_perItem;
      if (diffTime_perItem > max_scheduleClusters_moveScheduledNodes_perItem[PROF_CAT_EXECCYCLES])
	max_scheduleClusters_moveScheduledNodes_perItem[PROF_CAT_EXECCYCLES] = diffTime_perItem;
    }
    cerr << endl;

    t1 = current_scheduleClusters_requiredResources[PROF_CAT_EXECCYCLES];
    total_scheduleClusters_requiredResources [PROF_CAT_EXECCYCLES] += t1;
    if (t1 < min_scheduleClusters_requiredResources[PROF_CAT_EXECCYCLES])
      min_scheduleClusters_requiredResources[PROF_CAT_EXECCYCLES] = t1;
    if (t1 > max_scheduleClusters_requiredResources[PROF_CAT_EXECCYCLES])
      max_scheduleClusters_requiredResources[PROF_CAT_EXECCYCLES] = t1;
    
    if (PROFILE_VERBOSE)
      cerr << "++++ RequiredResorces -- " << t1;

    if (profilerItemCount) {
      diffTime_perItem = t1 / profilerItemCount;
      if (PROFILE_VERBOSE)
	cerr << " -- [" << diffTime_perItem << "/item]";
      total_scheduleClusters_requiredResources_perItem[PROF_CAT_EXECCYCLES] +=diffTime_perItem;
      if (diffTime_perItem < min_scheduleClusters_requiredResources_perItem[PROF_CAT_EXECCYCLES])
	min_scheduleClusters_requiredResources_perItem[PROF_CAT_EXECCYCLES] = diffTime_perItem;
      if (diffTime_perItem > max_scheduleClusters_requiredResources_perItem[PROF_CAT_EXECCYCLES])
	max_scheduleClusters_requiredResources_perItem[PROF_CAT_EXECCYCLES] = diffTime_perItem;
    }
    cerr << endl;

    // call these again to override the values from PROFILE_SCHEDULERCLUSTERS above
    threadCounter->DCUmissRead(SCHEDULER,
			       startTime[PROF_CAT_MEMREFS],
			       startTime[PROF_CAT_MISSCYCLES]);
    startTime[PROF_CAT_EXECCYCLES] = threadCounter->ScoreThreadSchCounterRead(); 
  } 
#endif
  // (5) go though all scheduled Clusters and place stitch buffers where
  //     needed. 
  //     (a) newly added stitch buffers are placed into scheduledMemSegList
  //     (b) stitch buffers that are already resident may need to have
  //         their mode changed (they are placed into
  //         configChangedStitchSegList).

  SCORECUSTOMLINKEDLIST_HEAD(scheduledClusterList, it);
  while (it != SCORECUSTOMLINKEDLIST_NULL) {
    ScoreCluster *currentCluster;
    SCORECUSTOMLINKEDLIST_ITEMAT(scheduledClusterList, it, currentCluster);
  
    assert(currentCluster->getLastTimesliceScheduled() == currentTimeslice);

    for (i = 0; i < SCORECUSTOMLIST_LENGTH(currentCluster->inputList); i ++) {
      SCORE_STREAM currentStream;
      SCORECUSTOMLIST_ITEMAT(currentCluster->inputList, i, currentStream);
      if (!(currentStream->sched_srcIsDone)) {
	switch(currentStream->sched_srcFunc) {
	case STREAM_OPERATOR_TYPE:
	  {
	    assert(currentStream->sched_src == 0);
#if 0
	    ScoreSegmentStitch *stitch =
	      insertStitchBufferOnInput(currentStream, currentCluster, i);
	    
	    // make sure the stitch buffer does not get cleaned up when empty!
	    stitch->sched_mustBeInDataFlow = 1;
	    
	    // schedule the stitch
	    stitch->setLastTimesliceScheduled(currentTimeslice);
	    SCORECUSTOMLINKEDLIST_APPEND(scheduledStitchList, stitch,
					 stitch->sched_residentStitchListItem);
	    
	    SCORECUSTOMLIST_APPEND(scheduledMemSegList, stitch);
	    stitch->sched_lastTimesliceConfigured = currentTimeslice;
#endif
	  }
	  break;
	case STREAM_SEGMENT_TYPE:
	  if (((ScoreSegment*)currentStream->sched_src)->sched_isStitch) {
	    ScoreSegmentStitch *stitch = (ScoreSegmentStitch*)(currentStream->sched_src);
	    // the stitch should have been mark scheduled during (4)
	    assert(stitch->getLastTimesliceScheduled() == currentTimeslice);
	    
	    // continue iff this stitch was not yet configured
	    if (stitch->sched_lastTimesliceConfigured != currentTimeslice) {
	      // compute segment mode
	      stitch->sched_old_mode = stitch->sched_mode;
	      
#ifndef NDEBUG
	      {
		// verify that there are no consequitive stitches (not supported now)
		SCORE_STREAM funkyStream = stitch->getSchedInStream();
		
		if (funkyStream->sched_srcFunc == STREAM_SEGMENT_TYPE) {
		  if (!(funkyStream->sched_srcIsDone)) {
		    ScoreSegment *seg = (ScoreSegment*)(funkyStream->sched_src);
		    if (seg->sched_isStitch) {
		      assert(0);
		    }
		  }
		}
	      }
#endif
	      // it is always srcsink in this case, since the stitch may have
	      // some tokens present in its fifo even if src is not scheduled
	      stitch->sched_mode = SCORE_CMB_SEQSRCSINK;
	      
	      if(stitch->sched_residentStitchListItem != SCORECUSTOMLINKEDLIST_NULL) {
#ifndef NDEBUG
		{
		  ScoreSegmentStitch *tmpStitch;
		  SCORECUSTOMLINKEDLIST_ITEMAT(residentStitchList,
					       stitch->sched_residentStitchListItem,
					       tmpStitch);
		  assert(tmpStitch == stitch);
		}
#endif
		// stitch is currently resident, possible reconfiguration required
#ifndef NDEBUG
		if (EXTRA_DEBUG) {
		  cerr << "DELETING " << ((unsigned int)(stitch)) << " [item = " <<
		    stitch->sched_residentStitchListItem <<
		    "] from residentstitchlist " <<
		    ((unsigned int)(residentStitchList)) << "\n";
		}
#endif
		SCORECUSTOMLINKEDLIST_DELITEM(residentStitchList,
					      stitch->sched_residentStitchListItem);
		if (stitch->sched_old_mode != stitch->sched_mode)
		  SCORECUSTOMLIST_APPEND(configChangedStitchSegList, stitch);
	      } else {
		// stitch is not currently resident
		SCORECUSTOMLIST_APPEND(scheduledMemSegList, stitch);
	      }
	      SCORECUSTOMLINKEDLIST_APPEND(scheduledStitchList, stitch,
					   stitch->sched_residentStitchListItem);

#ifndef NDEBUG
	      if (EXTRA_DEBUG) {
	      cerr << "APPENDING " << ((unsigned int)(stitch)) <<
		" [item = " << stitch->sched_residentStitchListItem <<
		"] to scheduledstitchlist " << ((unsigned int)(scheduledStitchList)) <<
		"\n";
	      }
#endif	      
	      stitch->sched_lastTimesliceConfigured = currentTimeslice;
	    }
	    break;
	  }
	  // if it is a "regular segment" fall through
	  // handle the same way as a page
	case STREAM_PAGE_TYPE:
	  {
	    ScoreGraphNode *node = currentStream->sched_src;
	    assert(node->isPage() || node->isSegment());
	    if (node->getLastTimesliceScheduled() != currentTimeslice) {
	      // the src node is not scheduled
	      
	      // check that page belongs to another cluster
	      assert(node->sched_parentCluster != currentCluster);
	      
	      ScoreCluster *srcCluster = node->sched_parentCluster;
	      unsigned int srcClusterOutputNumber;
	      for (srcClusterOutputNumber = 0; 
		   srcClusterOutputNumber <
		     SCORECUSTOMLIST_LENGTH(srcCluster->outputList);
		   srcClusterOutputNumber ++) {
		SCORE_STREAM tmpStream;
		SCORECUSTOMLIST_ITEMAT(srcCluster->outputList,
				       srcClusterOutputNumber, tmpStream);
		if (tmpStream == currentStream) 
		  break;
	      }

	      // make sure that the stream was found in the cluster's
	      // output list, otherwise there is a corruption
	      assert(srcClusterOutputNumber < SCORECUSTOMLIST_LENGTH(srcCluster->outputList));

	      ScoreSegmentStitch *stitch =
		insertStitchBufferOnOutput(node->sched_parentCluster, 
					   srcClusterOutputNumber,
					   currentStream);
	      
	      stitch->setLastTimesliceScheduled(currentTimeslice);
	      SCORECUSTOMLINKEDLIST_APPEND(scheduledStitchList, stitch,
					   stitch->sched_residentStitchListItem);
	      
	      SCORECUSTOMLIST_APPEND(scheduledMemSegList, stitch);
	      stitch->sched_lastTimesliceConfigured = currentTimeslice;
	    }
	  }
	  break;
	default:
	  assert(0);
	}
      }
    }


    for (i = 0; i < SCORECUSTOMLIST_LENGTH(currentCluster->outputList); i ++) {
      SCORE_STREAM currentStream;
      SCORECUSTOMLIST_ITEMAT(currentCluster->outputList, i, currentStream);
      if (!(currentStream->sched_sinkIsDone)) {
	switch(currentStream->sched_snkFunc) {
	case STREAM_OPERATOR_TYPE:
	  {
	    assert(currentStream->sched_sink == 0);
#if 0
	    ScoreSegmentStitch *stitch =
	      insertStitchBufferOnOutput(currentCluster, i, currentStream);
	    
	    // make it part of the cluster since it must be schedulable by itself
	    currentCluster->addNode_noAddIO(stitch);
	    
	    // make sure the stitch buffer does not get cleaned up when empty!
	    stitch->sched_mustBeInDataFlow = 1;
	    
	    // schedule the stitch
	    stitch->setLastTimesliceScheduled(currentTimeslice);
	    SCORECUSTOMLINKEDLIST_APPEND(scheduledStitchList, stitch,
					 stitch->sched_residentStitchListItem);
	    
	    SCORECUSTOMLIST_APPEND(scheduledMemSegList, stitch);
	    stitch->sched_lastTimesliceConfigured = currentTimeslice;
#endif
	  }
	  break;
	case STREAM_SEGMENT_TYPE:
	  if (((ScoreSegment*)currentStream->sched_sink)->sched_isStitch) {
	    ScoreSegmentStitch *stitch = (ScoreSegmentStitch*)(currentStream->sched_sink);
	    // the stitch should have been mark scheduled during (4)
	    assert(stitch->getLastTimesliceScheduled() == currentTimeslice);
	    
	    // continue iff this stitch was not yet configured
	    if (stitch->sched_lastTimesliceConfigured != currentTimeslice) {
	      // compute segment mode
	      stitch->sched_old_mode = stitch->sched_mode;
	      SCORE_STREAM outStream = stitch->getSchedOutStream();
	      
	      switch(outStream->sched_snkFunc) {
	      case STREAM_OPERATOR_TYPE:
		stitch->sched_mode = SCORE_CMB_SEQSRCSINK;
		break;
	      case STREAM_SEGMENT_TYPE:
		stitch->sched_mode = SCORE_CMB_SEQSRCSINK;
		break;
	      case STREAM_PAGE_TYPE:
		{
		  ScorePage *page = (ScorePage*)(outStream->sched_sink);
		  if (page->getLastTimesliceScheduled() == currentTimeslice) {
		    stitch->sched_mode = SCORE_CMB_SEQSRCSINK;
		  } else {
		    stitch->sched_mode = SCORE_CMB_SEQSINK;
		  }
		}
		break;
	      default:
		assert(0);
	      }
	      

	      if(stitch->sched_residentStitchListItem != SCORECUSTOMLINKEDLIST_NULL) {
#ifndef NDEBUG
		{
		  ScoreSegmentStitch *tmpStitch;
		  SCORECUSTOMLINKEDLIST_ITEMAT(residentStitchList,
					       stitch->sched_residentStitchListItem,
					       tmpStitch);
		  assert(tmpStitch == stitch);
		}
#endif
		// stitch is currently resident, possible reconfiguation required
#ifndef NDEBUG
		if (EXTRA_DEBUG) {
		  cerr << "DELETING " << ((unsigned int)(stitch)) << " [item = " <<
		    stitch->sched_residentStitchListItem <<
		    "] from residentstitchlist " <<
		    ((unsigned int)(residentStitchList)) << "\n";
		}
#endif
		SCORECUSTOMLINKEDLIST_DELITEM(residentStitchList,
					      stitch->sched_residentStitchListItem);
		if (stitch->sched_old_mode != stitch->sched_mode)
		  SCORECUSTOMLIST_APPEND(configChangedStitchSegList, stitch);
	      } else {
		// stitch is not currently resident
		SCORECUSTOMLIST_APPEND(scheduledMemSegList, stitch);
	      }
	      SCORECUSTOMLINKEDLIST_APPEND(scheduledStitchList, stitch,
					   stitch->sched_residentStitchListItem);
#ifndef NDEBUG	      
	      if (EXTRA_DEBUG) {
		cerr << "APPENDING " << ((unsigned int)(stitch)) << " [item = " <<
		  stitch->sched_residentStitchListItem <<
		  "] to scheduledstitchlist " << ((unsigned int)(scheduledStitchList))
		     <<"\n";
	      }
#endif
	      stitch->sched_lastTimesliceConfigured = currentTimeslice;
	    }
	    break;
	  }
	  // if it is a "regular segment" fall through
	  // handle the same way as a page
	case STREAM_PAGE_TYPE:
	  {
	    ScoreGraphNode *node = currentStream->sched_sink;
	    assert(node->isPage() || node->isSegment());
	    if (node->getLastTimesliceScheduled() != currentTimeslice) {
	      // the sink page is not scheduled
	      
	      // check that page does not belong to another cluster
	      // since all nodes within a cluster must be sheduled
	      assert(node->sched_parentCluster != currentCluster);
	      
	      ScoreSegmentStitch *stitch =
		insertStitchBufferOnOutput(currentCluster, i, currentStream);
	      
	      // make sure that newly inserted stitch has correct mode
	      stitch->sched_mode = stitch->sched_old_mode = SCORE_CMB_SEQSINK;

	      stitch->setLastTimesliceScheduled(currentTimeslice);
	      SCORECUSTOMLINKEDLIST_APPEND(scheduledStitchList, stitch,
					   stitch->sched_residentStitchListItem);
	      
	      SCORECUSTOMLIST_APPEND(scheduledMemSegList, stitch);
	      stitch->sched_lastTimesliceConfigured = currentTimeslice;
	    }
	  }
	  break;
	default:
	  assert(0);
	}
      }
    }
    
    SCORECUSTOMLINKEDLIST_GOTONEXT(scheduledClusterList, it);

#if 0
    cerr << "Placing stitches iteration " << iterNum << " (" << currentTimeslice << ")\n";
    visualizeCurrentState();
    iterNum ++;
#endif
  }

  if (EXTRA_DEBUG) {
    cerr << "After stitch placement:\n";
    cerr << "numFreePage = " << numFreePage << endl;
    cerr << "numFreeMemSeg = " << numFreeMemSeg << endl;
    cerr << "scheduledPageList size = " << SCORECUSTOMLIST_LENGTH(scheduledPageList) << endl;
    cerr << "scheduledMemSegList size = " << SCORECUSTOMLIST_LENGTH (scheduledMemSegList) << endl;
  }
  
  PROFILE_SCHEDULECLUSTERS(placeStitches,"PLACE STITCHES",profilerItemCount);
  
#if DOPROFILING_SCHEDULECLUSTERS
  profilerItemCount =
    SCORECUSTOMLINKEDLIST_LENGTH(residentClusterList) +
    SCORECUSTOMLINKEDLIST_LENGTH(residentStitchList);
#endif 

  // (6) go through all nodes on the residentClusterList and
  //     residentStitchList
  //     (a) if a node that used to be resident was NOT scheduled this
  //         timeslice, place it on the remove list.

  // first part is already done in (4) and (5)
  // here, place all unscheduled items on appropriate remove lists
  
  SCORECUSTOMLINKEDLIST_HEAD(residentClusterList, it);
  while (it != SCORECUSTOMLINKEDLIST_NULL) {
    ScoreCluster *currentCluster;
    SCORECUSTOMLINKEDLIST_ITEMAT(residentClusterList, it, currentCluster);
    
    assert(currentCluster->getLastTimesliceScheduled() == prevTimeslice);

    unsigned int numNodes = SCORECUSTOMLIST_LENGTH(currentCluster->nodeList);

    for(unsigned listIndex = 0; listIndex < numNodes; listIndex ++) {
      ScoreGraphNode *mynode;
      SCORECUSTOMLIST_ITEMAT(currentCluster->nodeList, listIndex, mynode);
      if (mynode->isPage()) {
	SCORECUSTOMLIST_APPEND(removedPageList, (ScorePage*) mynode);
      } else {
	ScoreSegment *segment = (ScoreSegment*) mynode;
	if (!(segment->sched_isFaulted)) // otherwise, it is on faultedMemSegList
	  SCORECUSTOMLIST_APPEND(removedMemSegList, segment);
      }
    }
    SCORECUSTOMLINKEDLIST_GOTONEXT(residentClusterList, it);
  }

  SCORECUSTOMLINKEDLIST_HEAD(residentStitchList, it);
  while (it != SCORECUSTOMLINKEDLIST_NULL) {
    ScoreSegmentStitch *currentStitch;
    SCORECUSTOMLINKEDLIST_ITEMAT(residentStitchList, it, currentStitch);

    // this must be done, since this is an indicator that the stitch is not
    // on the residentStitchList (clearing the list does not do this)
    currentStitch->sched_residentStitchListItem = SCORECUSTOMLINKEDLIST_NULL;

    assert(currentStitch->getLastTimesliceScheduled() == prevTimeslice);
    
    if (!(currentStitch->sched_isFaulted)) // otherwise, it is on faultedMemSegList
      SCORECUSTOMLIST_APPEND(removedMemSegList, (ScoreSegment*) currentStitch);

    SCORECUSTOMLINKEDLIST_GOTONEXT(residentStitchList, it);
  }

  
  if (EXTRA_DEBUG) {
    cerr << "AFTER RESIDENT LISTS CLEANING:\n";
    cerr << "removedPageList size = " << SCORECUSTOMLIST_LENGTH(removedPageList) << endl;
    cerr << "removedMemSegList size = " << SCORECUSTOMLIST_LENGTH(removedMemSegList) << endl;
    cerr << "doneNotRemovedPageList size = " << SCORECUSTOMLIST_LENGTH(doneNotRemovedPageList) << endl;
    cerr << "doneNotRemovedMemSegList size = " << SCORECUSTOMLIST_LENGTH(doneNotRemovedPageList) << endl;
  }

  
  // (7) clear the residentLists (others should be cleared after placement
  //     and reconfiguration [look at the sanity checks]).
  
  SCORECUSTOMLINKEDLIST_CLEAR(residentClusterList);
  SCORECUSTOMLINKEDLIST_CLEAR(residentStitchList);
  

  // (8) swap list, by interchanging pointers, i.e. the scheduledList becomes
  //     residentlist, and vice versa.

  ScoreCustomLinkedList<ScoreCluster*> *tmpClusterList = residentClusterList;
  ScoreCustomLinkedList<ScoreSegmentStitch*> *tmpStitchList = residentStitchList;
  residentClusterList = scheduledClusterList;
  residentStitchList = scheduledStitchList;

  scheduledClusterList = tmpClusterList;
  scheduledStitchList = tmpStitchList;

  // (9) Finish
  currentNumFreeCPs = numFreePage;
  currentNumFreeCMBs = numFreeMemSeg;

  PROFILE_SCHEDULECLUSTERS(removeUnscheduledNodes,"REMOVE UNSCHEDULED NODES",
			   profilerItemCount);

#if DOSCHEDULE_FOR_SCHEDTIME
  endClock = threadCounter->ScoreThreadSchCounterRead();
  diffClock = endClock - startClock;
  current_scheduleClusters = diffClock;
#endif

#if SCHEDULECLUSTERS_FOR_SCHEDTIME
  endClock = threadCounter->ScoreThreadSchCounterRead();
  diffClock = endClock - startClock;
  current_scheduleClusters = diffClock;
  cerr << "   scheduleClusters() ==> " << 
    diffClock <<
    " cycle(s)" << endl;
#endif

#if DOPROFILING
  endClock = threadCounter->ScoreThreadSchCounterRead();
  diffClock = endClock - startClock;
  if (diffClock < min_scheduleClusters) {
    min_scheduleClusters = diffClock;
  }
  if (diffClock > max_scheduleClusters) {
    max_scheduleClusters = diffClock;
  }
  total_scheduleClusters = total_scheduleClusters + diffClock;
  current_scheduleClusters = diffClock;
  cerr << "   scheduleClusters() ==> " << 
    diffClock <<
    " cycle(s)" << endl;
#endif

#if DOPROFILING_SCHEDULECLUSTERS
  for (int xyz = 0; xyz < PROF_CAT_COUNT; xyz ++) {
    current_scheduleClusters[xyz] = 
      current_scheduleClusters_handleDoneNodes[xyz] +
      current_scheduleClusters_copyResidentToWaiting[xyz] + 
      current_scheduleClusters_schedule[xyz] + 
      current_scheduleClusters_placeStitches[xyz] +
      current_scheduleClusters_removeUnscheduledNodes[xyz];
    
    total_scheduleClusters[xyz] += current_scheduleClusters[xyz];
    if (current_scheduleClusters[xyz] < min_scheduleClusters[xyz]) 
      min_scheduleClusters[xyz] = current_scheduleClusters[xyz];
    
    if (current_scheduleClusters[xyz] > max_scheduleClusters[xyz])
      max_scheduleClusters[xyz] = current_scheduleClusters[xyz];
    
    if (PROFILE_VERBOSE) {
      if (xyz == 0) cerr << "****** SCHEDULE CLUSTERS -- TOTAL:\n";
      cerr << " ** " << profile_cats[xyz] << ": " <<
	current_scheduleClusters[xyz] << endl;
    }
  }
    if (PROFILE_VERBOSE)
      cerr << "==========> SCHEDULECLUSTERS END" << endl;
#endif
  
}


