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
// $Revision: 1.4 $
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

#define MAIN_SCHED_FILE 
#include "ScoreSchedulerRandomDefines.h"

///////////////////////////////////////////////////////////////////////////////
// ScoreSchedulerRandom::ScoreSchedulerRandom:
//   Constructor for the scheduler.
//   Initializes all internal structures.
//
// Parameters:
//   exitOnIdle: whether or not to exit when idle.
//   noDeadlockDetection: whether or not to perform deadlock detection.
//   noImplicitDoneNodes: whether or not to make nodes implicitly done.
//   stitchBufferDontCare: whether or not to consider absorbed stitch
//                         stitch buffers in cluster freeable determination.
//
// Return value: None.
///////////////////////////////////////////////////////////////////////////////
ScoreSchedulerRandom::ScoreSchedulerRandom(char exitOnIdle, char noDeadlockDetection,
                               char noImplicitDoneNodes, 
                               char stitchBufferDontCare) {
  if (TIMEACC || DOPROFILING) {
    threadCounter = new ScoreThreadCounter(SCHEDULER);
    threadCounter->ScoreThreadCounterEnable(SCHEDULER);
  } else {
    threadCounter = NULL;
  }

  unsigned int i;


  // initialize the mutex.
  if (pthread_mutex_init(&schedulerDataMutex, NULL) != 0) {
    cerr << "SCHEDERR: Could not initialize the scheduler data mutex!" << endl;
    exit(1);
  }

  // get the current array information.
  if (getArrayInfo(&numPhysicalCP, &numPhysicalCMB, &cmbSize) == -1) {
    cerr << "SCHEDERR: Could not get the current physical array information!" 
	 << endl;
    exit(1);
  }

  currentNumFreeCPs = numPhysicalCP;
  currentNumFreeCMBs = numPhysicalCMB;

  // set the current system parameters.
  if (SCORE_ALLOCSIZE_MEMSEG > cmbSize) {
    cerr << "SCHEDERR: The physical CMB size of " << cmbSize <<
      " is smaller than the minimum allocation size of " << 
      SCORE_ALLOCSIZE_MEMSEG << "!" << endl;

    exit(1);
  }
  if (SCORE_DATASEGMENTBLOCK_LOADSIZE > cmbSize) {
    cerr << "SCHED: SCORE_DATASEGMENTBLOCK_LOADSIZE (" <<
      SCORE_DATASEGMENTBLOCK_LOADSIZE << ") is larger than cmbSize (" <<
      cmbSize << ")!" << endl;

    exit(1);
  }
  
  // instantiate the physical array view arrays.
  // instantiate busy masks.
  // instantiate the physical array status arrays.
  // initialize the unused physical CPs/CMBs lists.
  arrayCP = new ScoreArrayCP[numPhysicalCP];
  arrayCPBusy = new char[numPhysicalCP];
  cpStatus = new ScoreArrayCPStatus[numPhysicalCP];
  unusedPhysicalCPs = 
    new ScoreCustomQueue<unsigned int>(SCORE_SCHEDULERUNUSEDPHYSICALCPS_BOUND);
  for (i = 0; i < numPhysicalCP; i++) {
    arrayCP[i].loc = i;
    arrayCP[i].active = NULL;
    arrayCP[i].actual = NULL;
    arrayCP[i].scheduled = NULL;
    arrayCPBusy[i] = 0;
    cpStatus[i].clearStatus();
    SCORECUSTOMQUEUE_QUEUE(unusedPhysicalCPs, i);
  }
  arrayCMB = new ScoreArrayCMB[numPhysicalCMB];
  arrayCMBBusy = new char[numPhysicalCMB];
  cmbStatus = new ScoreArrayCMBStatus[numPhysicalCMB];
  unusedPhysicalCMBs =
    new ScoreCustomLinkedList<unsigned int>(SCORE_SCHEDULERUNUSEDPHYSICALCMBS_BOUND);
  for (i = 0; i < numPhysicalCMB; i++) {
    arrayCMB[i].loc = i;
    arrayCMB[i].active = NULL;
    arrayCMB[i].actual = NULL;
    arrayCMB[i].scheduled = NULL;
    arrayCMB[i].segmentTable = 
      new ScoreSegmentTable(i, cmbSize);
    arrayCMBBusy[i] = 0;
    cmbStatus[i].clearStatus();
    SCORECUSTOMLINKEDLIST_APPEND(unusedPhysicalCMBs, 
				 i, arrayCMB[i].unusedPhysicalCMBsItem);
  }

  // fill up the spare segment stitch and stream stitch lists.
  // NOTE: SEGMENTS ARE INITIALIZED TO POTENTIALLY ACCOMODATE 8 BIT TOKENS!
  spareSegmentStitchList = 
    new ScoreCustomStack<ScoreSegmentStitch *>(SCORE_SPARESEGMENTSTITCHLIST_BOUND);
  for (i = 0; i < SCORE_INIT_SPARE_SEGMENTSTITCH; i++) {
    ScoreSegmentStitch *spareSegStitch = 
      new ScoreSegmentStitch(SCORE_DATASEGMENTBLOCK_LOADSIZE, 8,
			     NULL, NULL);
  
    spareSegStitch->reset();
    SCORECUSTOMSTACK_PUSH(spareSegmentStitchList, spareSegStitch);
  }
  spareStreamStitchList = 
    new ScoreCustomStack<ScoreStream *>(SCORE_SPARESTREAMSTITCHLIST_BOUND);
  for (i = 0; i < SCORE_INIT_SPARE_STREAMSTITCH; i++) {
    ScoreStreamStitch *spareStreamStitch = 
      new ScoreStreamStitch(64 /* FIX ME! */, 0, SCORE_INPUTFIFO_CAPACITY, 
			    SCORE_STREAM_UNTYPED);
    
    spareStreamStitch->reset();
    spareStreamStitch->sched_spareStreamStitchList = spareStreamStitchList;
    SCORECUSTOMSTACK_PUSH(spareStreamStitchList, spareStreamStitch);
  }

  doneNodeCheckList =
    new ScoreCustomQueue<ScoreGraphNode *>(SCORE_SCHEDULERDONENODECHECKLIST_BOUND);

  operatorStreamsFromProcessor =
    new ScoreCustomList<ScoreStream *>(SCORE_SCHEDULEROPERATORSTREAMSFROMPROCESSOR_BOUND);
  operatorStreamsToProcessor =
    new ScoreCustomList<ScoreStream *>(SCORE_SCHEDULEROPERATORSTREAMSTOPROCESSOR_BOUND);

  waitingClusterList =
    new ScoreCustomList<ScoreCluster *>(SCORE_SCHEDULERWAITINGCLUSTERLIST_BOUND);
  residentClusterList =
    new ScoreCustomLinkedList<ScoreCluster *>(SCORE_SCHEDULERRESIDENTCLUSTERLIST_BOUND);
  residentStitchList = 
    new ScoreCustomLinkedList<ScoreSegmentStitch *>(SCORE_SCHEDULERRESIDENTSTITCHLIST_BOUND);

  scheduledClusterList =
    new ScoreCustomLinkedList<ScoreCluster *>(SCORE_SCHEDULERRESIDENTCLUSTERLIST_BOUND);
  scheduledStitchList = 
    new ScoreCustomLinkedList<ScoreSegmentStitch *>(SCORE_SCHEDULERRESIDENTSTITCHLIST_BOUND);

  headClusterList =
    new ScoreCustomList<ScoreCluster *>(SCORE_SCHEDULERHEADCLUSTERLIST_BOUND);

  processorIStreamList =
    new ScoreCustomList<ScoreStream *>(SCORE_SCHEDULERPROCESSORISTREAMLIST_BOUND);
  processorOStreamList =
    new ScoreCustomList<ScoreStream *>(SCORE_SCHEDULERPROCESSOROSTREAMLIST_BOUND);

  doneNodeList = 
    new ScoreCustomList<ScoreGraphNode *>(SCORE_SCHEDULERDONENODELIST_BOUND);
#if RANDOM_SCHEDULER_VERSION == 1
  freeableClusterList =
    new ScoreCustomList<ScoreCluster *>(SCORE_SCHEDULERFREEABLECLUSTERLIST_BOUND);
#endif
  doneClusterList =
    new ScoreCustomList<ScoreCluster *>(SCORE_SCHEDULERDONECLUSTERLIST_BOUND);
  faultedMemSegList =
    new ScoreCustomList<ScoreSegment *>(SCORE_SCHEDULERFAULTEDMEMSEGLIST_BOUND);
  addedBufferLockStitchBufferList =
    new ScoreCustomList<ScoreSegmentStitch *>(SCORE_SCHEDULERADDEDBUFFERLOCKSTITCHBUFFERLIST_BOUND);
  scheduledPageList =
    new ScoreCustomList<ScorePage *>(SCORE_SCHEDULERSCHEDULEDPAGELIST_BOUND);
  scheduledMemSegList =
    new ScoreCustomList<ScoreSegment *>(SCORE_SCHEDULERSCHEDULEDMEMSEGLIST_BOUND);
  removedPageList =
    new ScoreCustomList<ScorePage *>(SCORE_SCHEDULERREMOVEDPAGELIST_BOUND);
  removedMemSegList =
    new ScoreCustomList<ScoreSegment *>(SCORE_SCHEDULERREMOVEDMEMSEGLIST_BOUND);
  doneNotRemovedPageList =
    new ScoreCustomList<ScorePage *>(SCORE_SCHEDULERDONENOTREMOVEDPAGELIST_BOUND);
  doneNotRemovedMemSegList =
    new ScoreCustomList<ScoreSegment *>(SCORE_SCHEDULERDONENOTREMOVEDMEMSEGLIST_BOUND);
  configChangedStitchSegList =
    new ScoreCustomList<ScoreSegment *>(SCORE_SCHEDULERCONFIGCHANGEDSTITCHSEGLIST_BOUND);
  emptyStitchList =
    new ScoreCustomList<ScoreSegmentStitch *>(SCORE_SCHEDULEREMPTYSTITCHLIST_BOUND);
  stitchBufferList =
    new ScoreCustomList<ScoreSegmentStitch *>(SCORE_SCHEDULERSTITCHBUFFERLIST_BOUND);
  processList =
    new ScoreCustomList<ScoreProcess *>(SCORE_SCHEDULERPROCESSLIST_BOUND);

  deadLockedProcesses =
    new ScoreCustomList<ScoreProcess *>(SCORE_SCHEDULERDEADLOCKEDPROCESSES_BOUND);

  stitchListToMarkScheduled =
    new ScoreCustomList<ScoreSegmentStitch*>(SCORE_SCHEDULERSTITCHLISTTOMARKSCHEDULED_BOUND);
  
  clusterListToAddProcessorStitch = 
    new ScoreCustomList<ScoreCluster*>(SCORE_SCHEDULERCLUSTERLISTTOADDSTITCH_BOUND);

  processorNode = 
    new ScoreProcessorNode(SCORE_SCHEDULERPROCESSORNODE_INPUT_BOUND, 
			   SCORE_SCHEDULERPROCESSORNODE_OUTPUT_BOUND);
  processorNode->setNumIO(0, 0);

  // initialize the virtual time.
  schedulerVirtualTime = 0;

  // initialize to idle.
  isIdle = 1;
  isReawakening = 0;
  lastReawakenTime = 0;

  currentTimeslice = 0;

  doExitOnIdle = exitOnIdle;
  doNoDeadlockDetection = noDeadlockDetection;
  doNotMakeNodesImplicitlyDone = noImplicitDoneNodes;
  noCareStitchBufferInClusters = stitchBufferDontCare;

#if SCHEDULECLUSTERS_FOR_SCHEDTIME
  current_scheduleClusters = 0;
#endif

#if DOPROFILING || DOPROFILING_SCHEDULECLUSTERS
  total_usedCPs = 0;
  total_usedCMBs = 0;
  min_usedCPs = MAXINT;
  min_usedCMBs = MAXINT;
  max_usedCPs = 0;
  max_usedCMBs = 0;
#endif

#if DOPROFILING
  total_getCurrentStatus = 0;
  total_gatherStatusInfo = 0;
  total_findDonePagesSegments = 0;
  total_findFaultedMemSeg = 0;
  total_findFreeableClusters = 0;
  total_dealWithDeadLock = 0;
  total_scheduleClusters = 0;
  total_performPlacement = 0;
  total_issueReconfigCommands = 0;
  total_performCleanup = 0;
  total_doSchedule = 0;

  min_getCurrentStatus = MAXLONG;
  min_gatherStatusInfo = MAXLONG;
  min_findDonePagesSegments = MAXLONG;
  min_findFaultedMemSeg = MAXLONG;
  min_findFreeableClusters = MAXLONG;
  min_dealWithDeadLock = MAXLONG;
  min_scheduleClusters = MAXLONG;
  min_performPlacement = MAXLONG;
  min_issueReconfigCommands = MAXLONG;
  min_performCleanup = MAXLONG;
  min_doSchedule = MAXLONG;

  max_getCurrentStatus = 0;
  max_gatherStatusInfo = 0;
  max_findDonePagesSegments = 0;
  max_findFaultedMemSeg = 0;
  max_findFreeableClusters = 0;
  max_dealWithDeadLock = 0;
  max_scheduleClusters = 0;
  max_performPlacement = 0;
  max_issueReconfigCommands = 0;
  max_performCleanup = 0;
  max_doSchedule = 0;
#endif
#if DOPROFILING_SCHEDULECLUSTERS

  scheduleClusters_handleDoneNodes_perItemCount = 0;
  scheduleClusters_copyResidentToWaiting_perItemCount = 0;
  scheduleClusters_schedule_perItemCount = 0;
  scheduleClusters_placeStitches_perItemCount = 0;
  scheduleClusters_removeUnscheduledNodes_perItemCount = 0;
  
  for (int i = 0; i < PROF_CAT_COUNT; i ++) {
    total_scheduleClusters[i] = 0;
    total_scheduleClusters_handleDoneNodes[i] = 0;
    total_scheduleClusters_copyResidentToWaiting[i] = 0;
    total_scheduleClusters_schedule[i] = 0;
    total_scheduleClusters_placeStitches[i] = 0;
    total_scheduleClusters_removeUnscheduledNodes[i] = 0;
    
    min_scheduleClusters[i] = MAXLONG;
    min_scheduleClusters_handleDoneNodes[i] = MAXLONG;
    min_scheduleClusters_copyResidentToWaiting[i] = MAXLONG;
    min_scheduleClusters_schedule[i] = MAXLONG;
    min_scheduleClusters_placeStitches[i] = MAXLONG;
    min_scheduleClusters_removeUnscheduledNodes[i] = MAXLONG;
    
    max_scheduleClusters[i] = 0;
    max_scheduleClusters_handleDoneNodes[i] = 0;
    max_scheduleClusters_copyResidentToWaiting[i] = 0;
    max_scheduleClusters_schedule[i] = 0;
    max_scheduleClusters_placeStitches[i] = 0;
    max_scheduleClusters_removeUnscheduledNodes[i] = 0;

    total_scheduleClusters_handleDoneNodes_perItem[i] = 0;
    total_scheduleClusters_copyResidentToWaiting_perItem[i] = 0;
    total_scheduleClusters_schedule_perItem[i] = 0;
    total_scheduleClusters_placeStitches_perItem[i] = 0;
    total_scheduleClusters_removeUnscheduledNodes_perItem[i] = 0;
    
    min_scheduleClusters_handleDoneNodes_perItem[i] = MAXLONG;
    min_scheduleClusters_copyResidentToWaiting_perItem[i] = MAXLONG;
    min_scheduleClusters_schedule_perItem[i] = MAXLONG;
    min_scheduleClusters_placeStitches_perItem[i] = MAXLONG;
    min_scheduleClusters_removeUnscheduledNodes_perItem[i] = MAXLONG;
    
    max_scheduleClusters_handleDoneNodes_perItem[i] = 0;
    max_scheduleClusters_copyResidentToWaiting_perItem[i] = 0;
    max_scheduleClusters_schedule_perItem[i] = 0;
    max_scheduleClusters_placeStitches_perItem[i] = 0;
    max_scheduleClusters_removeUnscheduledNodes_perItem[i] = 0;
  }
#endif
#if KEEPRECONFIGSTATISTICS
  total_stopPage = 0;
  total_startPage = 0;
  total_stopSegment = 0;
  total_startSegment = 0;
  total_dumpPageState = 0;
  total_dumpPageFIFO = 0;
  total_loadPageConfig = 0;
  total_loadPageState = 0;
  total_loadPageFIFO = 0;
  total_getSegmentPointers = 0;
  total_dumpSegmentFIFO = 0;
  total_setSegmentConfigPointers = 0;
  total_changeSegmentMode = 0;
  total_changeSegmentTRAandPBOandMAX = 0;
  total_resetSegmentDoneFlag = 0;
  total_loadSegmentFIFO = 0;
  total_memXferPrimaryToCMB = 0;
  total_memXferCMBToPrimary = 0;
  total_memXferCMBToCMB = 0;
  total_connectStream = 0;
#endif
}


///////////////////////////////////////////////////////////////////////////////
// ScoreSchedulerRandom::~ScoreSchedulerRandom:
//   Destructor for the scheduler.
//   Cleans up all internal structures.
//
// Parameters: None.
//
// Return value: None.
///////////////////////////////////////////////////////////////////////////////
ScoreSchedulerRandom::~ScoreSchedulerRandom() {
  unsigned int i;

 
  // destroy the mutex.
  if (pthread_mutex_destroy(&schedulerDataMutex) != 0) {
    cerr << "SCHEDERR: Error while destroying the scheduler data mutex!" << 
      endl;
  }

  // clean up the various arrays.
  delete(arrayCP);
  delete(arrayCMB);
  delete(cpStatus);
  delete(cmbStatus);

  // go through and delete all processes.
  // FIX ME! If the scheduler is deleted when there are still process
  //         operators running in it, currently, this data will not be
  //         resynchronized with the process!
  for (i = 0; i < SCORECUSTOMLIST_LENGTH(processList); i++) {
    ScoreProcess *mynode;

    SCORECUSTOMLIST_ITEMAT(processList, i, mynode);

    delete(mynode);
  }
  delete(processList);

  delete(operatorStreamsFromProcessor);
  delete(operatorStreamsToProcessor);

  delete(doneNodeCheckList);

  delete(waitingClusterList);
  delete(residentClusterList);
  delete(headClusterList);

  delete(processorIStreamList);
  delete(processorOStreamList);

  delete(doneNodeList);
#if RANDOM_SCHEDULER_VERSION == 1
  delete(freeableClusterList);
#endif
  delete(doneClusterList);
  delete(faultedMemSegList);
  delete(addedBufferLockStitchBufferList);
  delete(scheduledPageList);
  delete(scheduledMemSegList);
  delete(removedPageList);
  delete(removedMemSegList);
  delete(doneNotRemovedPageList);
  delete(doneNotRemovedMemSegList);
  delete(configChangedStitchSegList);
  delete(emptyStitchList);
  delete(stitchBufferList);
  delete(deadLockedProcesses);

  // delete all the spare stitch segments/streams.
  for (i = 0; i < SCORECUSTOMSTACK_LENGTH(spareSegmentStitchList); i++) {
    delete(spareSegmentStitchList->buffer[i]);
  }
  SCORECUSTOMSTACK_CLEAR(spareSegmentStitchList);
  for (i = 0; i < SCORECUSTOMSTACK_LENGTH(spareStreamStitchList); i++) {
    delete(spareStreamStitchList->buffer[i]);
  }
  SCORECUSTOMSTACK_CLEAR(spareStreamStitchList);

  processorNode->setNumIO(0, 0);
  delete(processorNode);

#ifdef RANDOM_SCHEDULER
  delete (stitchListToMarkScheduled);
#endif
}









