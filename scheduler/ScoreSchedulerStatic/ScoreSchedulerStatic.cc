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
#include "ScoreSchedulerStatic.h"
#include "ScoreRuntime.h"
#include "ScoreSimulator.h"
#include "ScoreConfig.h"
#include "ScoreStateGraph.h"
#include "ScoreDummyDonePage.h"
#include "ScoreCustomStack.h"
#include "ScoreCustomList.h"
#include "ScoreCustomQueue.h"
#include "ScoreCustomLinkedList.h"

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

unsigned long long current_scheduleClusters;

unsigned long long startClock, endClock, diffClock;
#endif

#if DOSCHEDULE_FOR_SCHEDTIME
#include <asm/msr.h>

unsigned long long current_gatherStatusInfo, current_findDonePagesSegments,
  current_findFaultedMemSeg, current_findFreeableClusters,
  current_dealWithDeadLock, current_scheduleClusters,
  current_performPlacement;

unsigned long long startClock, endClock, diffClock;
#endif

#if DOPROFILING
#include <asm/msr.h>

unsigned long long current_gatherStatusInfo, current_findDonePagesSegments,
  current_findFaultedMemSeg, current_findFreeableClusters,
  current_dealWithDeadLock, current_scheduleClusters,
  current_performPlacement;

unsigned long long total_getCurrentStatus, total_gatherStatusInfo,
  total_findDonePagesSegments, total_findFaultedMemSeg,
  total_findFreeableClusters, total_dealWithDeadLock,
  total_scheduleClusters, total_performPlacement,
  total_issueReconfigCommands, total_performCleanup,
  total_doSchedule;
unsigned long long min_getCurrentStatus, min_gatherStatusInfo,
  min_findDonePagesSegments, min_findFaultedMemSeg,
  min_findFreeableClusters, min_dealWithDeadLock,
  min_scheduleClusters, min_performPlacement,
  min_issueReconfigCommands, min_performCleanup,
  min_doSchedule;
unsigned long long max_getCurrentStatus, max_gatherStatusInfo,
  max_findDonePagesSegments, max_findFaultedMemSeg,
  max_findFreeableClusters, max_dealWithDeadLock,
  max_scheduleClusters, max_performPlacement,
  max_issueReconfigCommands, max_performCleanup,
  max_doSchedule;

unsigned long long startClock, endClock, diffClock;
#endif

#if KEEPRECONFIGSTATISTICS
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




// a pointer to the visualFile where state graphs are written
extern char *visualFile;




///////////////////////////////////////////////////////////////////////////////
// ScoreSchedulerStatic::ScoreSchedulerStatic:
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
ScoreSchedulerStatic::ScoreSchedulerStatic(char exitOnIdle, char noDeadlockDetection,
                               char noImplicitDoneNodes, 
                               char stitchBufferDontCare) {
  cerr << "*****STATIC CONSTR****\n";

  if (TIMEACC || DOPROFILING) {
    threadCounter = new ScoreThreadCounter(SCHEDULER);
    threadCounter->ScoreThreadCounterEnable();
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

  processorIStreamList =
    new ScoreCustomList<ScoreStream *>(SCORE_SCHEDULERPROCESSORISTREAMLIST_BOUND);
  processorOStreamList =
    new ScoreCustomList<ScoreStream *>(SCORE_SCHEDULERPROCESSOROSTREAMLIST_BOUND);

  doneNodeList = 
    new ScoreCustomList<ScoreGraphNode *>(SCORE_SCHEDULERDONENODELIST_BOUND);

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

  currentTraversal = 0;

  doExitOnIdle = exitOnIdle;
  doNoDeadlockDetection = noDeadlockDetection;
  doNotMakeNodesImplicitlyDone = noImplicitDoneNodes;
  noCareStitchBufferInClusters = stitchBufferDontCare;

  // newstuff: initialize to no new operators yet
  newOperators = 0;

  // newstuff: make empty schedule
  ssched = new ScoreSSched(numPhysicalCP, numPhysicalCMB);

#if SCHEDULECLUSTERS_FOR_SCHEDTIME
  current_scheduleClusters = 0;
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
  total_scheduleClusters = 0;
  total_scheduleClusters_handleDoneNodes = 0;
  total_scheduleClusters_bufferLockStitchBuffers = 0;
  total_scheduleClusters_handleFreeableClusters = 0;
  total_scheduleClusters_handleFreeableClusters_reprioritize = 0;
  total_scheduleClusters_performTrials = 0;
  total_scheduleClusters_performTrials_reprioritize = 0;
  total_scheduleClusters_backOutOfTrials = 0;
  total_scheduleClusters_addRemoveNodesOfClusters = 0;
  total_scheduleClusters_handleStitchBuffersOld = 0;
  total_scheduleClusters_handleStitchBuffersNew = 0;
  total_scheduleClusters_updateClusterLists = 0;
  total_scheduleClusters_cleanup = 0;

  min_scheduleClusters = MAXLONG;
  min_scheduleClusters_handleDoneNodes = MAXLONG;
  min_scheduleClusters_bufferLockStitchBuffers = MAXLONG;
  min_scheduleClusters_handleFreeableClusters = MAXLONG;
  min_scheduleClusters_handleFreeableClusters_reprioritize = MAXLONG;
  min_scheduleClusters_performTrials = MAXLONG;
  min_scheduleClusters_performTrials_reprioritize = MAXLONG;
  min_scheduleClusters_backOutOfTrials = MAXLONG;
  min_scheduleClusters_addRemoveNodesOfClusters = MAXLONG;
  min_scheduleClusters_handleStitchBuffersOld = MAXLONG;
  min_scheduleClusters_handleStitchBuffersNew = MAXLONG;
  min_scheduleClusters_updateClusterLists = MAXLONG;
  min_scheduleClusters_cleanup = MAXLONG;

  max_scheduleClusters = 0;
  max_scheduleClusters_handleDoneNodes = 0;
  max_scheduleClusters_bufferLockStitchBuffers = 0;
  max_scheduleClusters_handleFreeableClusters = 0;
  max_scheduleClusters_handleFreeableClusters_reprioritize = 0;
  max_scheduleClusters_performTrials = 0;
  max_scheduleClusters_performTrials_reprioritize = 0;
  max_scheduleClusters_backOutOfTrials = 0;
  max_scheduleClusters_addRemoveNodesOfClusters = 0;
  max_scheduleClusters_handleStitchBuffersOld = 0;
  max_scheduleClusters_handleStitchBuffersNew = 0;
  max_scheduleClusters_updateClusterLists = 0;
  max_scheduleClusters_cleanup = 0;
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
// ScoreSchedulerStatic::~ScoreSchedulerStatic:
//   Destructor for the scheduler.
//   Cleans up all internal structures.
//
// Parameters: None.
//
// Return value: None.
///////////////////////////////////////////////////////////////////////////////
ScoreSchedulerStatic::~ScoreSchedulerStatic() {
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
    ScoreProcess *node;

    SCORECUSTOMLIST_ITEMAT(processList, i, node);

    delete(node);
  }
  delete(processList);

  delete(operatorStreamsFromProcessor);
  delete(operatorStreamsToProcessor);

  delete(doneNodeCheckList);

  delete(processorIStreamList);
  delete(processorOStreamList);

  delete(doneNodeList);

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

  // newstuff: static schedule
  delete ssched;
}

///////////////////////////////////////////////////////////////////////////////
// ScoreSchedulerStatic::addOperator:
//   Adds an operators to the scheduler.
//
// Parameters:
//   sharedObject: the fully-resolved name of the shared object file containing
//                   the operator to instantiate.
//   argbuf: the arguments to the operator.
//   pid: the process id of the process instantiating this operator.
//
// Return value:
//   0 if successful; -1 if unsuccessful.
///////////////////////////////////////////////////////////////////////////////
int ScoreSchedulerStatic::addOperator(char *sharedObject, char *argbuf, pid_t pid) {
  construct_t construct;
  ScoreOperatorInstance *opi;
  ScoreProcess *process;
  graph operatorGraph;
  node_array<int> *componentNum;
  char abortOperatorInsert;
  unsigned int i, j;

  // get a lock on the scheduler data mutex.
  pthread_mutex_lock(&schedulerDataMutex);

#if DOPROFILING
  startClock = threadCounter->ScoreThreadSchCounterRead();
#endif

  // try to get function pointer.
  {
    void *newHandle = dlopen(sharedObject, RTLD_LAZY);

    if (newHandle != NULL) {
      construct = (construct_t) dlsym(newHandle, SCORE_CONSTRUCT_NAME);
      char *error = dlerror();

      if (error == NULL) {
	// instantiate an instance of the operator.
	opi = (*construct)(argbuf);
	  
	// store the handle.
	opi->sched_handle = newHandle;
      } else {
	cerr << "Could not find the construction function in " <<
	  sharedObject << " (dlerror = " << error << ")" << endl;

        return(-1);
      }
    } else {
      cerr << "Could not open shared object " << sharedObject <<
	" (dlerror = " << dlerror() << ")" << endl;
      
      return(-1);
    }
  }

  // copy over the shared object name into the operator instance.
  opi->sharedObjectName = new char[strlen(sharedObject)+1];
  strcpy(opi->sharedObjectName, sharedObject);

  opi->sched_livePages = opi->pages;
  opi->sched_liveSegments = opi->segments;

  if (VERBOSEDEBUG || DEBUG || PRINTSTATE) {
    cerr << "SCHED: PAGES===================" << endl;
    for (i = 0; i < opi->pages; i++) {
      int j;
      cerr << "SCHED:    PAGE " << i << 
	" (source=" << opi->page[i]->getSource() << ") " << 
	(unsigned int) opi->page[i] << endl;
      
      for (j = 0; j < opi->page[i]->getInputs(); j++) {
	cerr << "SCHED:       INPUT " << j << " srcFunc " << 
	  opi->page[i]->getSchedInput(j)->sched_srcFunc << " snkFunc " << 
	  opi->page[i]->getSchedInput(j)->sched_snkFunc << " ";
	if (opi->page[i]->getSchedInput(j)->sched_src == NULL) {
	  cerr << "SRCISNULL ";
	}
	if (opi->page[i]->getSchedInput(j)->sched_sink == NULL) {
	  cerr << "SINKISNULL ";
	}
	cerr << " " << (unsigned int) opi->page[i]->getSchedInput(j) << endl;
      }
      for (j = 0; j < opi->page[i]->getOutputs(); j++) {
	cerr << "SCHED:       OUTPUT " << j << " srcFunc " << 
	  opi->page[i]->getSchedOutput(j)->sched_srcFunc << " snkFunc " << 
	  opi->page[i]->getSchedOutput(j)->sched_snkFunc << " ";
	if (opi->page[i]->getSchedOutput(j)->sched_src == NULL) {
	  cerr << "SRCISNULL ";
	}
	if (opi->page[i]->getSchedOutput(j)->sched_sink == NULL) {
	  cerr << "SINKISNULL ";
	}
	cerr << " " << (unsigned int) opi->page[i]->getSchedOutput(j) << endl;
      }
    }
    cerr << "SCHED: ========================" << endl;
    cerr << "SCHED: SEGMENTS===================" << endl;
    for (i = 0; i < opi->segments; i++) {
      int j;
      cerr << "SCHED:    SEGMENT " << i << " " << 
	(unsigned int) opi->segment[i]->getInputs() << endl;
      
      for (j = 0; j < opi->segment[i]->getInputs(); j++) {
	cerr << "SCHED:       INPUT " << j << " srcFunc " << 
	  opi->segment[i]->getSchedInput(j)->sched_srcFunc << " snkFunc " << 
	  opi->segment[i]->getSchedInput(j)->sched_snkFunc << " ";
	if (opi->segment[i]->getSchedInput(j)->sched_src == NULL) {
	  cerr << "SRCISNULL ";
	}
	if (opi->segment[i]->getSchedInput(j)->sched_sink == NULL) {
	  cerr << "SINKISNULL ";
	}
	cerr << " " << (unsigned int) opi->segment[i]->getSchedInput(j) << endl;
      }
      for (j = 0; j < opi->segment[i]->getOutputs(); j++) {
	cerr << "SCHED:       OUTPUT " << j << " srcFunc " << 
	  opi->segment[i]->getSchedOutput(j)->sched_srcFunc << " snkFunc " << 
	  opi->segment[i]->getSchedOutput(j)->sched_snkFunc << " ";
	if (opi->segment[i]->getSchedOutput(j)->sched_src == NULL) {
	  cerr << "SRCISNULL ";
	}
	if (opi->segment[i]->getSchedOutput(j)->sched_sink == NULL) {
	  cerr << "SINKISNULL ";
	}
	cerr << (unsigned int) opi->segment[i]->getSchedOutput(j) << endl;
      }
    }
    cerr << "SCHED: ========================" << endl;
  }

  // newstuff: Used to be create new clusters
  // Modified to include appending to sschedPageList, sschedSegmentList

  for (i = 0; i < opi->pages; i++) {
    opi->page[i]->sched_parentOperator = opi;

    ScorePage *currentPage = opi->page[i];
    SCORECUSTOMLIST_APPEND(sschedPageList, currentPage);
  }

  for (i = 0; i < opi->segments; i++) {
    opi->segment[i]->sched_parentOperator = opi;

    ScoreSegment *currentSegment = opi->segment[i];
    SCORECUSTOMLIST_APPEND(sschedSegmentList, currentSegment);
  }

  // newstuff: [cut out clustering]

  // if this operator insertion is still valid, then insert the operator
  // along with all of its clusters/pages/segments.
  if (!abortOperatorInsert) {
    // try to find an entry in the process list for this process id. otherwise,
    // insert an entry into the table.
    process = NULL;
    for (i = 0; i < SCORECUSTOMLIST_LENGTH(processList); i++) {
      ScoreProcess *currentProcess;

      SCORECUSTOMLIST_ITEMAT(processList, i, currentProcess);

      if (currentProcess->pid == pid) {
	process = currentProcess;

	break;
      }
    }

    if (process == NULL) {
#if GET_FEEDBACK
      process = new ScoreProcess(pid, sharedObject);
#else
      process = new ScoreProcess(pid);
#endif
      
      SCORECUSTOMLIST_APPEND(processList, process);

      process->numPages = 0;
      process->numSegments = 0;
      process->numPotentiallyNonFiringPages = 0;
      process->numPotentiallyNonFiringSegments = 0;
    }

    // insert the operator into the process.
    // FIX ME! We might want to check to make sure there are no duplicate
    //         operators!

    // newstuff: here is where READFEEDBACK occurs
    process->addOperator(opi);

    // newstuff: mark existence of a new operator
    newOperators = 1;

    // initialize the status of each page/memory segment.
    for (i = 0; i < opi->pages; i++) {
      ScorePage *currentPage = opi->page[i];
      int currentState = currentPage->get_state();
#if RESETNODETOALLIO
#else
      ScoreIOMaskType currentConsumed = currentPage->inputs_consumed(currentState);
      ScoreIOMaskType currentProduced = currentPage->outputs_produced(currentState);
#endif
      unsigned int numInputs = (unsigned int) currentPage->getInputs();
      unsigned int numOutputs = (unsigned int) currentPage->getOutputs();
      
      currentPage->sched_isDone = 0;
      currentPage->sched_isResident = 0;
      currentPage->sched_isScheduled = 0;
      currentPage->sched_residentLoc = 0;
      currentPage->sched_fifoBuffer =
	malloc(SCORE_PAGEFIFO_SIZE);
      currentPage->sched_lastKnownState = currentState;
      currentPage->sched_potentiallyDidNotFireLastResident = 0;

      for (j = 0; j < numInputs; j++) {
#if RESETNODETOALLIO
	char isBeingConsumed = 1;
#else
	char isBeingConsumed = (currentConsumed >> j) & 1;
#endif

	if (isBeingConsumed) {
	  currentPage->sched_inputConsumption[j] = 1;
	  currentPage->sched_inputConsumptionOffset[j] = -1;
	}
      }
      for (j = 0; j < numOutputs; j++) {
#if RESETNODETOALLIO
	char isBeingProduced = 1;
#else
	char isBeingProduced = (currentProduced >> j) & 1;
#endif

	if (isBeingProduced) {
	  currentPage->sched_outputProduction[j] = 1;
	  currentPage->sched_outputProductionOffset[j] = -1;
	}
      }
    }
    for (i = 0; i < opi->segments; i++) {
      ScoreSegment *currentSegment = opi->segment[i];
#if RESETNODETOALLIO
      unsigned int numInputs = (unsigned int) currentSegment->getInputs();
      unsigned int numOutputs = (unsigned int) currentSegment->getOutputs();
#endif
      
      currentSegment->sched_isResident = 0;
      currentSegment->sched_residentLoc = 0;
      currentSegment->sched_isStitch = 0;
      currentSegment->sched_traAddr = 0;
      currentSegment->sched_pboAddr = 0;
      currentSegment->sched_readAddr = 0;
      currentSegment->sched_writeAddr = 0;
      currentSegment->sched_residentStart = 0;
      if (((unsigned int) currentSegment->length()) >
	  (SCORE_DATASEGMENTBLOCK_LOADSIZE / (SCORE_ALIGN_BITWIDTH_TO_8(currentSegment->width())/8))) {
	currentSegment->sched_maxAddr = 
          SCORE_DATASEGMENTBLOCK_LOADSIZE / (SCORE_ALIGN_BITWIDTH_TO_8(currentSegment->width())/8);
	currentSegment->sched_residentLength = 
          SCORE_DATASEGMENTBLOCK_LOADSIZE / (SCORE_ALIGN_BITWIDTH_TO_8(currentSegment->width())/8);
      } else {
	currentSegment->sched_maxAddr = currentSegment->length();
	currentSegment->sched_residentLength = currentSegment->length();
      }
      currentSegment->sched_fifoBuffer =
	malloc(currentSegment->getInputs()*SCORE_MEMSEGFIFO_SIZE);
      currentSegment->this_segment_is_done = 0;
      currentSegment->sched_this_segment_is_done = 0;

#if RESETNODETOALLIO
      for (j = 0; j < numInputs; j++) {
        currentSegment->sched_inputConsumption[j] = 1;
        currentSegment->sched_inputConsumptionOffset[j] = -1;
      }
      for (j = 0; j < numOutputs; j++) {
        currentSegment->sched_outputProduction[j] = 1;
        currentSegment->sched_outputProductionOffset[j] = -1;
      }
#else
      if (currentSegment->sched_mode == SCORE_CMB_SEQSRC) {
	currentSegment->
	  sched_outputProduction[SCORE_CMB_SEQSRC_DATA_OUTNUM] = 1;
	currentSegment->
	  sched_outputProductionOffset[SCORE_CMB_SEQSRC_DATA_OUTNUM] = -1;
      } else if (currentSegment->sched_mode == SCORE_CMB_SEQSINK) {
	currentSegment->
	  sched_inputConsumption[SCORE_CMB_SEQSINK_DATA_INNUM] = 1;
	currentSegment->
	  sched_inputConsumptionOffset[SCORE_CMB_SEQSINK_DATA_INNUM] = -1;
      } else if (currentSegment->sched_mode == SCORE_CMB_SEQSRCSINK) {
	currentSegment->
	  sched_inputConsumption[SCORE_CMB_SEQSRCSINK_DATAW_INNUM] = 1;
	currentSegment->
	  sched_inputConsumptionOffset[SCORE_CMB_SEQSRCSINK_DATAW_INNUM] = -1;
	currentSegment->
	  sched_outputProduction[SCORE_CMB_SEQSRCSINK_DATAR_OUTNUM] = 1;
	currentSegment->
	  sched_outputProductionOffset[SCORE_CMB_SEQSRCSINK_DATAR_OUTNUM] = 
	  -1;
      } else if (currentSegment->sched_mode == SCORE_CMB_RAMSRC) {
	currentSegment->
	  sched_inputConsumption[SCORE_CMB_RAMSRC_ADDR_INNUM] = 1;
	currentSegment->
	  sched_inputConsumptionOffset[SCORE_CMB_RAMSRC_ADDR_INNUM] = -1;
	currentSegment->
	  sched_outputProduction[SCORE_CMB_RAMSRC_DATA_OUTNUM] = 1;
	currentSegment->
	  sched_outputProductionOffset[SCORE_CMB_RAMSRC_DATA_OUTNUM] = -1;
      } else if (currentSegment->sched_mode == SCORE_CMB_RAMSINK) {
	currentSegment->
	  sched_inputConsumption[SCORE_CMB_RAMSINK_ADDR_INNUM] = 1;
	currentSegment->
	  sched_inputConsumptionOffset[SCORE_CMB_RAMSINK_ADDR_INNUM] = -1;
	currentSegment->
	  sched_inputConsumption[SCORE_CMB_RAMSINK_DATA_INNUM] = 1;
	currentSegment->
	  sched_inputConsumptionOffset[SCORE_CMB_RAMSINK_DATA_INNUM] = -1;
      } else if (currentSegment->sched_mode == SCORE_CMB_RAMSRCSINK) {
	currentSegment->
	  sched_inputConsumption[SCORE_CMB_RAMSRCSINK_ADDR_INNUM] = 1;
	currentSegment->
	  sched_inputConsumptionOffset[SCORE_CMB_RAMSRCSINK_ADDR_INNUM] = -1;
	currentSegment->
	  sched_inputConsumption[SCORE_CMB_RAMSRCSINK_WRITE_INNUM] = 1;
	currentSegment->
	  sched_inputConsumptionOffset[SCORE_CMB_RAMSRCSINK_WRITE_INNUM] = -1;
      }
#endif
    }

    // check all of the IO streams on the pages and memory segments to
    // see if they go to/from the processor operators. update the
    // processor operator IO stream list.
    // Also, if the IO streams are connected to a stitch buffer, then it
    // means that a previously instantiated operator that this operator is
    // now attached to had instantiated a stitch buffer for a processor IO
    // and this stitch buffer has been marked sched_mustBeInDataFlow. unmark
    // it!
    for (i = 0; i < opi->pages; i++) {
      ScorePage *currentPage = opi->page[i];
      int numInputs = currentPage->getInputs();
      int numOutputs = currentPage->getOutputs();
      int j;

      for (j = 0; j < numInputs; j++) {
        SCORE_STREAM currentStream = currentPage->getSchedInput(j);

        if (currentStream->srcFunc == STREAM_OPERATOR_TYPE) {
	  SCORECUSTOMLIST_APPEND(processorIStreamList, currentStream);
          SCORECUSTOMLIST_APPEND(process->processorIStreamList, currentStream);
	  SCORECUSTOMLIST_APPEND(operatorStreamsFromProcessor, currentStream);

	  currentStream->sched_isProcessorArrayStream = 1;

	  SCORE_MARKSTREAM(currentStream,threadCounter); 
        } else {
	  if (currentStream->sched_isProcessorArrayStream) {
            SCORECUSTOMLIST_REMOVE(processorOStreamList, currentStream);
            SCORECUSTOMLIST_REMOVE(process->processorOStreamList, 
				   currentStream);
            SCORECUSTOMLIST_REMOVE(operatorStreamsToProcessor, currentStream);
	    currentStream->sched_isProcessorArrayStream = 0;
          }

	  if ((currentStream->sched_srcFunc == STREAM_SEGMENT_TYPE) &&
	      (((ScoreSegment *) currentStream->sched_src)->sched_isStitch)) {
	    ScoreSegmentStitch *currentStitch =
	      (ScoreSegmentStitch *) currentStream->sched_src;

	    currentStitch->sched_mustBeInDataFlow = 0;
	  }
        }
      }
      for (j = 0; j < numOutputs; j++) {
        SCORE_STREAM currentStream = currentPage->getSchedOutput(j);

        if (currentStream->snkFunc == STREAM_OPERATOR_TYPE) {
          SCORECUSTOMLIST_APPEND(processorOStreamList, currentStream);
          SCORECUSTOMLIST_APPEND(process->processorOStreamList, currentStream);
	  SCORECUSTOMLIST_APPEND(operatorStreamsToProcessor, currentStream);

	  currentStream->sched_isProcessorArrayStream = 1;

	  SCORE_MARKSTREAM(currentStream,threadCounter); 
        } else {
          if (currentStream->sched_isProcessorArrayStream) {
            SCORECUSTOMLIST_REMOVE(processorIStreamList, currentStream);
	    SCORECUSTOMLIST_REMOVE(process->processorIStreamList, 
				   currentStream);
            SCORECUSTOMLIST_REMOVE(operatorStreamsFromProcessor, 
				   currentStream);
	    currentStream->sched_isProcessorArrayStream = 0;
          }

	  if ((currentStream->sched_snkFunc == STREAM_SEGMENT_TYPE) &&
	      (((ScoreSegment *) currentStream->sched_sink)->sched_isStitch)) {
	    ScoreSegmentStitch *currentStitch =
	      (ScoreSegmentStitch *) currentStream->sched_sink;

	    currentStitch->sched_mustBeInDataFlow = 0;
	  }
        }
      }
    }
    for (i = 0; i < opi->segments; i++) {
      ScoreSegment *currentSegment = opi->segment[i];
      int numInputs = currentSegment->getInputs();
      int numOutputs = currentSegment->getOutputs();
      int j;

      for (j = 0; j < numInputs; j++) {
        SCORE_STREAM currentStream = currentSegment->getSchedInput(j);

        if (currentStream->srcFunc == STREAM_OPERATOR_TYPE) {
          SCORECUSTOMLIST_APPEND(processorIStreamList, currentStream);
          SCORECUSTOMLIST_APPEND(process->processorIStreamList, currentStream);
	  SCORECUSTOMLIST_APPEND(operatorStreamsFromProcessor, currentStream);

	  currentStream->sched_isProcessorArrayStream = 1;

	  SCORE_MARKSTREAM(currentStream,threadCounter); 
        } else {
	  if (currentStream->sched_isProcessorArrayStream) {
            SCORECUSTOMLIST_REMOVE(processorOStreamList, currentStream);
            SCORECUSTOMLIST_REMOVE(process->processorOStreamList, 
				   currentStream);
            SCORECUSTOMLIST_REMOVE(operatorStreamsToProcessor, currentStream);

	    currentStream->sched_isProcessorArrayStream = 0;
          }

	  if ((currentStream->sched_srcFunc == STREAM_SEGMENT_TYPE) &&
	      (((ScoreSegment *) currentStream->sched_src)->sched_isStitch)) {
	    ScoreSegmentStitch *currentStitch =
	      (ScoreSegmentStitch *) currentStream->sched_src;

	    currentStitch->sched_mustBeInDataFlow = 0;
	  }
        }
      }
      for (j = 0; j < numOutputs; j++) {
        SCORE_STREAM currentStream = currentSegment->getSchedOutput(j);

        if (currentStream->snkFunc == STREAM_OPERATOR_TYPE) {
          SCORECUSTOMLIST_APPEND(processorOStreamList, currentStream);
          SCORECUSTOMLIST_APPEND(process->processorOStreamList, currentStream);
	  SCORECUSTOMLIST_APPEND(operatorStreamsToProcessor, currentStream);

	  currentStream->sched_isProcessorArrayStream = 1;

	  SCORE_MARKSTREAM(currentStream,threadCounter); 
        } else {
	  if (currentStream->sched_isProcessorArrayStream) {
            SCORECUSTOMLIST_REMOVE(processorIStreamList, currentStream);
            SCORECUSTOMLIST_REMOVE(process->processorIStreamList, 
				   currentStream);
            SCORECUSTOMLIST_REMOVE(operatorStreamsFromProcessor, 
				   currentStream);
	    currentStream->sched_isProcessorArrayStream = 0;
          }

	  if ((currentStream->sched_snkFunc == STREAM_SEGMENT_TYPE) &&
	      (((ScoreSegment *) currentStream->sched_sink)->sched_isStitch)) {
	    ScoreSegmentStitch *currentStitch =
	      (ScoreSegmentStitch *) currentStream->sched_sink;

	    currentStitch->sched_mustBeInDataFlow = 0;
	  }
        }
      }
    }

    // if the scheduler is currently idle, then send a timeslice seed.
    if (isIdle) {
      cerr << "SCHED: BECAUSE THE SCHEDULER WAS IDLE, SENDING A TIMESLICE " <<
	"SEED!" << endl;

      if (schedulerVirtualTime == 0) {
	sendSimulatorRunUntil(0);
      } else {
#if VERBOSEDEBUG || DEBUG
	cerr << "SCHED: REQUESTING NEXT TIMESLICE AT: " << 
          schedulerVirtualTime << endl;
#endif
	requestNextTimeslice();
      }

      isIdle = 0;
      isReawakening = 1;
    }

#if DOPROFILING
    endClock = threadCounter->ScoreThreadSchCounterRead();
    diffClock = endClock - startClock;
    cerr << "   addOperator() ==> " << 
      diffClock <<
      " cycle(s)" << endl;
#endif

    // release the scheduler data mutex.
    pthread_mutex_unlock(&schedulerDataMutex);

    return(0);
  } else { // if (!abortOperatorInsert) {
    void *oldHandle = opi->sched_handle;

    delete componentNum;
    componentNum = NULL;

    delete opi;
    opi = NULL;
    dlclose(oldHandle);

    // release the scheduler data mutex.
    pthread_mutex_unlock(&schedulerDataMutex);

    return(-1);
  }
}

///////////////////////////////////////////////////////////////////////////////
// ScoreSchedulerStatic::doSchedule:
//   Run array according to precomputed schedule.
//
// Parameters: none.
//
// Return value: none.
///////////////////////////////////////////////////////////////////////////////
void ScoreSchedulerStatic::doSchedule() {
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

  // newstuff: number of cycles to run next timeslice
  unsigned int cyclesToRun = 0;

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

  if (VISUALIZE_STATE)
    visualizeCurrentState();

#if 0
#if DOPROFILING
  doScheduleStartClock = threadCounter->ScoreThreadSchCounterRead();
#endif
#endif

  currentTimeslice++;
  if (VERBOSEDEBUG || DEBUG || DOPROFILING || PRINTSTATE) {
    cerr << "SCHED: Current timeslice: " << currentTimeslice << endl;
  }

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

      cerr << "SCHED:    DONE NODE: " << (unsigned int) currentNode << endl;
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

  // newstuff: [cut out findFreeableClusters()]

  // deal with any potential bufferlock.
  if (VERBOSEDEBUG || DEBUG) {
    cerr << "SCHED:    dealWithDeadLock()" << endl;
  }
  if (!doNoDeadlockDetection) {
    dealWithDeadLock();
  }

  if (VERBOSEDEBUG || DEBUG || PRINTSTATE) {
    cerr << "SCHED: NUMBER OF SCHEDULED PAGES: " << 
      SCORECUSTOMLIST_LENGTH(scheduledPageList) << endl;
    for (i = 0; i < SCORECUSTOMLIST_LENGTH(scheduledPageList); i++) {
      ScorePage *currentPage;

      SCORECUSTOMLIST_ITEMAT(scheduledPageList, i, currentPage);

      cerr << "SCHED:    SCHEDULED PAGE: " << 
	(unsigned int) currentPage << endl;
    }
    cerr << "SCHED: NUMBER OF SCHEDULED MEMSEG: " << 
      SCORECUSTOMLIST_LENGTH(scheduledMemSegList) << endl;
    for (i = 0; i < SCORECUSTOMLIST_LENGTH(scheduledMemSegList); i++) {
      ScoreSegment *currentMemSeg;

      SCORECUSTOMLIST_ITEMAT(scheduledMemSegList, i, currentMemSeg);

      cerr << "SCHED:    SCHEDULED MEMSEG: " << 
	(unsigned int) currentMemSeg << endl;
    }
    cerr << "SCHED: NUMBER OF REMOVED PAGES: " << 
      SCORECUSTOMLIST_LENGTH(removedPageList) << endl;
    for (i = 0; i < SCORECUSTOMLIST_LENGTH(removedPageList); i++) {
      ScorePage *currentPage;

      SCORECUSTOMLIST_ITEMAT(removedPageList, i, currentPage);

      cerr << "SCHED:    REMOVED PAGE: " << 
	(unsigned int) currentPage << endl;
    }
    cerr << "SCHED: NUMBER OF REMOVED MEMSEG: " << 
      SCORECUSTOMLIST_LENGTH(removedMemSegList) << endl;
    for (i = 0; i < SCORECUSTOMLIST_LENGTH(removedMemSegList); i++) {
      ScoreSegment *currentMemSeg;

      SCORECUSTOMLIST_ITEMAT(removedMemSegList, i, currentMemSeg);

      cerr << "SCHED:    REMOVED MEMSEG: " << 
	(unsigned int) currentMemSeg << endl;
    }
  }

  // newstuff: [cut out scheduleClusters()]

  if (newOperators) {
    // make new schedule
    createSchedule();

    // reset newOperators
    newOperators = 0;
  }

  cyclesToRun = applySchedule();

  // place the scheduled pages/memory segments.
  if (VERBOSEDEBUG || DEBUG) {
    cerr << "SCHED:    performPlacement()" << endl;
  }
  performPlacement();


  // newstuff: [commented out the following]
  /*
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
  */

  // newstuff: tell simulator number of cycles to run
  advanceSimulatorTimeOffset(cyclesToRun);

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
      total_scheduleClusters_handleDoneNodes +
      total_scheduleClusters_bufferLockStitchBuffers +
      total_scheduleClusters_handleFreeableClusters +
      total_scheduleClusters_handleFreeableClusters_reprioritize +
      total_scheduleClusters_handleEmptyFrontierList +
      total_scheduleClusters_performTrials +
      total_scheduleClusters_performTrials_reprioritize +
      total_scheduleClusters_backOutOfTrials +
      total_scheduleClusters_addRemoveNodesOfClusters +
      total_scheduleClusters_handleStitchBuffersOld +
      total_scheduleClusters_handleStitchBuffersNew +
      total_scheduleClusters_updateClusterLists +
      total_scheduleClusters_cleanup;

    cerr << "SCHED: =================================================" << endl;
    cerr << "SCHED: Total scheduleClusters_handleDoneNodes: " <<
      total_scheduleClusters_handleDoneNodes << " cycle(s) " << 
      "[" << min_scheduleClusters_handleDoneNodes << " : " <<
      total_scheduleClusters_handleDoneNodes/currentTimeslice << " : " <<
      max_scheduleClusters_handleDoneNodes << "] " <<
      "[" << (((double) total_scheduleClusters_handleDoneNodes)/
	      totalCycles)*100 << 
      "]%" << endl;
    cerr << "SCHED: Total scheduleClusters_bufferLockStitchBuffers: " <<
      total_scheduleClusters_bufferLockStitchBuffers << " cycle(s) " << 
      "[" << min_scheduleClusters_bufferLockStitchBuffers << " : " <<
      total_scheduleClusters_bufferLockStitchBuffers/currentTimeslice << 
      " : " <<
      max_scheduleClusters_bufferLockStitchBuffers << "] " <<
      "[" << (((double) total_scheduleClusters_bufferLockStitchBuffers)/
	      totalCycles)*100 << 
      "%]" << endl;
    cerr << "SCHED: Total scheduleClusters_handleFreeableClusters: " <<
      total_scheduleClusters_handleFreeableClusters << " cycle(s) " << 
      "[" << min_scheduleClusters_handleFreeableClusters << " : " <<
      total_scheduleClusters_handleFreeableClusters/currentTimeslice << 
      " : " <<
      max_scheduleClusters_handleFreeableClusters << "] " <<
      "[" << (((double) total_scheduleClusters_handleFreeableClusters)/
	      totalCycles)*100 << 
      "%]" << endl;
    cerr << "SCHED: Total scheduleClusters_handleFreeableClusters_reprioritize: " <<
      total_scheduleClusters_handleFreeableClusters_reprioritize << " cycle(s) " << 
      "[" << min_scheduleClusters_handleFreeableClusters_reprioritize << " : " <<
      total_scheduleClusters_handleFreeableClusters_reprioritize/currentTimeslice << 
      " : " <<
      max_scheduleClusters_handleFreeableClusters_reprioritize << "] " <<
      "[" << (((double) total_scheduleClusters_handleFreeableClusters_reprioritize)/
	      totalCycles)*100 << 
      "%]" << endl;
    cerr << "SCHED: Total scheduleClusters_handleEmptyFrontierList: " <<
      total_scheduleClusters_handleEmptyFrontierList << " cycle(s) " << 
      "[" << min_scheduleClusters_handleEmptyFrontierList << " : " <<
      total_scheduleClusters_handleEmptyFrontierList/currentTimeslice << 
      " : " <<
      max_scheduleClusters_handleEmptyFrontierList << "] " <<
      "[" << (((double) total_scheduleClusters_handleEmptyFrontierList)/
	      totalCycles)*100 << 
      "%]" << endl;
    cerr << "SCHED: Total scheduleClusters_performTrials: " <<
      total_scheduleClusters_performTrials << " cycle(s) " << 
      "[" << min_scheduleClusters_performTrials << " : " <<
      total_scheduleClusters_performTrials/currentTimeslice << " : " <<
      max_scheduleClusters_performTrials << "] " <<
      "[" << (((double) total_scheduleClusters_performTrials)/
	      totalCycles)*100 << 
      "%]" << endl;
    cerr << "SCHED: Total scheduleClusters_performTrials_reprioritize: " <<
      total_scheduleClusters_performTrials_reprioritize << " cycle(s) " << 
      "[" << min_scheduleClusters_performTrials_reprioritize << " : " <<
      total_scheduleClusters_performTrials_reprioritize/currentTimeslice << " : " <<
      max_scheduleClusters_performTrials_reprioritize << "] " <<
      "[" << (((double) total_scheduleClusters_performTrials_reprioritize)/
	      totalCycles)*100 << 
      "%]" << endl;
    cerr << "SCHED: Total scheduleClusters_backOutOfTrials: " <<
      total_scheduleClusters_backOutOfTrials << " cycle(s) " << 
      "[" << min_scheduleClusters_backOutOfTrials << " : " <<
      total_scheduleClusters_backOutOfTrials/currentTimeslice << " : " <<
      max_scheduleClusters_backOutOfTrials << "] " <<
      "[" << (((double) total_scheduleClusters_backOutOfTrials)/
	      totalCycles)*100 << 
      "%]" << endl;
    cerr << "SCHED: Total scheduleClusters_addRemoveNodesOfClusters: " <<
      total_scheduleClusters_addRemoveNodesOfClusters << " cycle(s) " << 
      "[" << min_scheduleClusters_addRemoveNodesOfClusters<< " : " <<
      total_scheduleClusters_addRemoveNodesOfClusters/currentTimeslice << 
      " : " <<
      max_scheduleClusters_addRemoveNodesOfClusters << "] " <<
      "[" << (((double) total_scheduleClusters_addRemoveNodesOfClusters)/
	      totalCycles)*100 << 
      "%]" << endl;
    cerr << "SCHED: Total scheduleClusters_handleStitchBuffersOld: " <<
      total_scheduleClusters_handleStitchBuffersOld << " cycle(s) " << 
      "[" << min_scheduleClusters_handleStitchBuffersOld << " : " <<
      total_scheduleClusters_handleStitchBuffersOld/currentTimeslice << " : " <<
      max_scheduleClusters_handleStitchBuffersOld << "] " <<
      "[" << (((double) total_scheduleClusters_handleStitchBuffersOld)/
	      totalCycles)*100 << 
      "%]" << endl;
    cerr << "SCHED: Total scheduleClusters_handleStitchBuffersNew: " <<
      total_scheduleClusters_handleStitchBuffersNew << " cycle(s) " << 
      "[" << min_scheduleClusters_handleStitchBuffersNew << " : " <<
      total_scheduleClusters_handleStitchBuffersNew/currentTimeslice << " : " <<
      max_scheduleClusters_handleStitchBuffersNew << "] " <<
      "[" << (((double) total_scheduleClusters_handleStitchBuffersNew)/
	      totalCycles)*100 << 
      "%]" << endl;
    cerr << "SCHED: Total scheduleClusters_updateClusterLists: " <<
      total_scheduleClusters_updateClusterLists << " cycle(s) " << 
      "[" << min_scheduleClusters_updateClusterLists << " : " <<
      total_scheduleClusters_updateClusterLists/currentTimeslice << " : " <<
      max_scheduleClusters_updateClusterLists << "] " <<
      "[" << (((double) total_scheduleClusters_updateClusterLists)/
              totalCycles)*100 << 
      "%]" << endl;
    cerr << "SCHED: Total scheduleClusters_cleanup: " <<
      total_scheduleClusters_cleanup << " cycle(s) " << 
      "[" << min_scheduleClusters_cleanup << " : " <<
      total_scheduleClusters_cleanup/currentTimeslice << " : " <<
      max_scheduleClusters_cleanup << "] " <<
      "[" << (((double) total_scheduleClusters_cleanup)/totalCycles)*100 << 
      "%]" << endl;
    cerr << "SCHED: *** TOTAL SCHEDULECLUSTERS(): " <<
      total_scheduleClusters << " cycle(s) " <<
      "[" << min_scheduleClusters << " : " <<
      total_scheduleClusters/currentTimeslice << " : " <<
      max_scheduleClusters << "] " <<
      "[" << (((double) total_scheduleClusters)/totalCycles)*100 << 
      "%]" << endl;
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
// ScoreSchedulerStatic::getCurrentTimeslice:
//   Get the current timeslice.
//
// Parameters: none.
//
// Return value: 
//   the current timeslice.
///////////////////////////////////////////////////////////////////////////////
unsigned int ScoreSchedulerStatic::getCurrentTimeslice() {
  return(currentTimeslice);
}

///////////////////////////////////////////////////////////////////////////////
// ScoreSchedulerStatic::getCurrentStatus:
//   Get the current status of the physical array.
//
// Parameters: none.
//
// Return value: none.
///////////////////////////////////////////////////////////////////////////////
void ScoreSchedulerStatic::getCurrentStatus() {
  unsigned int i;
  char *cpMask = NULL;
  char *cmbMask = NULL;


#if 0 && DOPROFILING
  startClock = threadCounter->ScoreThreadSchCounterRead();
#endif

  // build up masks.
  cpMask = new char[numPhysicalCP];
  for (i = 0; i < numPhysicalCP; i++) {
    if (arrayCP[i].active != NULL) {
      cpMask[i] = 1;
    } else {
      cpMask[i] = 0;
    }
  }
  cmbMask = new char[numPhysicalCMB];
  for (i = 0; i < numPhysicalCMB; i++) {
    if (arrayCMB[i].active != NULL) {
      cmbMask[i] = 1;
    } else {
      cmbMask[i] = 0;
    }
  }

  if (getArrayStatus(cpStatus, cmbStatus, cpMask, cmbMask) == -1) {
    cerr << "SCHEDERR: Error getting current physical array status!" << endl;
    exit(1);
  }

  // filter the information so that only pages/memory segments with active
  // pages/memory segments will have interesting status.
  for (i = 0; i < numPhysicalCP; i++) {
    if (arrayCP[i].active == NULL) {
      cpStatus[i].clearStatus();
    }
  }
  for (i = 0; i < numPhysicalCMB; i++) {
    if (arrayCMB[i].active == NULL) {
      cmbStatus[i].clearStatus();
    }
  }

#if 0 && DOPROFILING
  endClock = threadCounter->ScoreThreadSchCounterRead();
  diffClock = endClock - startClock;
  total_getCurrentStatus = total_getCurrentStatus + diffClock;
  if (diffClock < min_getCurrentStatus) {
    min_getCurrentStatus = diffClock;
  }
  if (diffClock > max_getCurrentStatus) {
    max_getCurrentStatus = diffClock;
  }
  cerr << "   getCurrentStatus() ==> " << 
    diffClock <<
    " cycle(s)" << endl;
#endif
}

///////////////////////////////////////////////////////////////////////////////
// ScoreSchedulerStatic::gatherStatusInfo:
//   Given the array status, stores current array status info into scheduler
//     data structures.
//
// Parameters: none.
//
// Return value: none.
///////////////////////////////////////////////////////////////////////////////
void ScoreSchedulerStatic::gatherStatusInfo() {
  unsigned int i, j;


#if DOPROFILING
  startClock = threadCounter->ScoreThreadSchCounterRead();
#endif

#if DOSCHEDULE_FOR_SCHEDTIME
  startClock = threadCounter->ScoreThreadSchCounterRead();
#endif

  // FIX ME! THIS IS A LITTLE INEFFICIENT SINCE WE WILL BE SETTING STREAMS
  //         POTENTIALLY TWICE! (SRC/SINK).
  for (i = 0; i < numPhysicalCP; i++) {
    if (arrayCP[i].active != NULL) {
      ScorePage *currentPage = arrayCP[i].active;
      ScoreProcess *currentProcess = currentPage->sched_parentProcess;
      unsigned int numInputs = currentPage->getInputs();
      unsigned int numOutputs = currentPage->getOutputs();
      char hasConsumedProduced = 0;

      currentPage->sched_lastKnownState = cpStatus[i].currentState;

      for (j = 0; j < numInputs; j++) {
	ScoreStream *currentStream = currentPage->getSchedInput(j);
	char isEmpty = (cpStatus[i].emptyInputs >> j) & 1;

	if (isEmpty) {
	  currentStream->sched_isPotentiallyEmpty = 1;
	  currentStream->sched_isPotentiallyFull = 0;
	} else {
	  currentStream->sched_isPotentiallyEmpty = 0;
	}

	if ((currentPage->sched_inputConsumption[j]+
	     currentPage->sched_inputConsumptionOffset[j]) !=
	    cpStatus[i].inputConsumption[j]) {
	  hasConsumedProduced = 1;
	}
	currentPage->sched_inputConsumption[j] =
	  cpStatus[i].inputConsumption[j] -
	  currentPage->sched_inputConsumptionOffset[j];

	currentPage->sched_lastKnownInputFIFONumTokens[j] =
	  cpStatus[i].inputFIFONumTokens[j];
      }
      for (j = 0; j < numOutputs; j++) {
	ScoreStream *currentStream = currentPage->getSchedOutput(j);
	char isFull = (cpStatus[i].fullOutputs >> j) & 1;

	if (isFull) {
	  currentStream->sched_isPotentiallyFull = 1;
	  currentStream->sched_isPotentiallyEmpty = 0;
	} else {
	  currentStream->sched_isPotentiallyFull = 0;
	}

	if ((currentPage->sched_outputProduction[j]+
	     currentPage->sched_outputProductionOffset[j]) !=
	    cpStatus[i].outputProduction[j]) {
	  hasConsumedProduced = 1;
	}
	currentPage->sched_outputProduction[j] =
	  cpStatus[i].outputProduction[j] -
	  currentPage->sched_outputProductionOffset[j];
      }

      if (!hasConsumedProduced) {
	if (!(currentPage->sched_potentiallyDidNotFireLastResident)) {
#if RESETNODETOALLIO
#else
	  int currentState = currentPage->sched_lastKnownState;
	  ScoreIOMaskType currentConsumed = currentPage->inputs_consumed(currentState);
	  ScoreIOMaskType currentProduced = currentPage->outputs_produced(currentState);
#endif

	  currentPage->sched_potentiallyDidNotFireLastResident = 1;

	  currentProcess->numPotentiallyNonFiringPages++;

	  // "reset" the input consumption and output production rates
	  // by setting the offset appropriately.
	  for (j = 0; j < numInputs; j++) {
#if RESETNODETOALLIO
	    char isBeingConsumed = 1;
#else
	    char isBeingConsumed = (currentConsumed >> j) & 1;
#endif

	    currentPage->sched_inputConsumptionOffset[j] =
	      currentPage->sched_inputConsumptionOffset[j] +
	      currentPage->sched_inputConsumption[j];
	    currentPage->sched_inputConsumption[j] = 0;

	    if (isBeingConsumed) {
	      currentPage->sched_inputConsumption[j] = 1;
	      currentPage->sched_inputConsumptionOffset[j] =
		currentPage->sched_inputConsumptionOffset[j] + -1;
	    }
	  }
	  for (j = 0; j < numOutputs; j++) {
#if RESETNODETOALLIO
	    char isBeingProduced = 1;
#else
	    char isBeingProduced = (currentProduced >> j) & 1;
#endif

	    currentPage->sched_outputProductionOffset[j] =
	      currentPage->sched_outputProductionOffset[j] +
	      currentPage->sched_outputProduction[j];
	    currentPage->sched_outputProduction[j] = 0;

	    if (isBeingProduced) {
	      currentPage->sched_outputProduction[j] = 1;
	      currentPage->sched_outputProductionOffset[j] =
		currentPage->sched_outputProductionOffset[j] + -1;
	    }
	  }
	}
      } else {
	if (currentPage->sched_potentiallyDidNotFireLastResident) {
	  currentPage->sched_potentiallyDidNotFireLastResident = 0;

	  currentProcess->numPotentiallyNonFiringPages--;
	}
      }
    }
  }
  for (i = 0; i < numPhysicalCMB; i++) {
    if (arrayCMB[i].active != NULL) {
      ScoreSegment *currentSegment = arrayCMB[i].active;
      ScoreProcess *currentProcess = currentSegment->sched_parentProcess;
      unsigned int numInputs = currentSegment->getInputs();
      unsigned int numOutputs = currentSegment->getOutputs();
      char hasConsumedProduced = 0;

      if (currentSegment->sched_readCount != cmbStatus[i].readCount) {
	hasConsumedProduced = 1;
      }
      if (currentSegment->sched_writeCount != cmbStatus[i].writeCount) {
	hasConsumedProduced = 1;
      }

      currentSegment->sched_readAddr = cmbStatus[i].readAddr;
      currentSegment->sched_writeAddr = cmbStatus[i].writeAddr;
      currentSegment->sched_readCount = cmbStatus[i].readCount;
      currentSegment->sched_writeCount = cmbStatus[i].writeCount;

      // interpolate the input and output consumption / production rates
      // from the read/write counts.
      if (currentSegment->sched_mode == SCORE_CMB_SEQSRC) {
	currentSegment->sched_outputProduction
	  [SCORE_CMB_SEQSRC_DATA_OUTNUM] = 
	  currentSegment->sched_readCount -
	  currentSegment->sched_outputProductionOffset
	  [SCORE_CMB_SEQSRC_DATA_OUTNUM];
      } else if (currentSegment->sched_mode == SCORE_CMB_SEQSINK) {
	currentSegment->sched_inputConsumption
	  [SCORE_CMB_SEQSINK_DATA_INNUM] = 
	  currentSegment->sched_writeCount -
	  currentSegment->sched_inputConsumptionOffset
	  [SCORE_CMB_SEQSINK_DATA_INNUM];
      } else if (currentSegment->sched_mode == SCORE_CMB_SEQSRCSINK) {
	currentSegment->sched_inputConsumption
	  [SCORE_CMB_SEQSRCSINK_DATAW_INNUM] = 
	  currentSegment->sched_writeCount -
	  currentSegment->sched_inputConsumptionOffset
	  [SCORE_CMB_SEQSRCSINK_DATAW_INNUM];
	currentSegment->sched_outputProduction
	  [SCORE_CMB_SEQSRCSINK_DATAR_OUTNUM] = 
	  currentSegment->sched_readCount -
	  currentSegment->sched_outputProductionOffset
	  [SCORE_CMB_SEQSRCSINK_DATAR_OUTNUM];
      } else if (currentSegment->sched_mode == SCORE_CMB_RAMSRC) {
	currentSegment->sched_inputConsumption
	  [SCORE_CMB_RAMSRC_ADDR_INNUM] = 
	  currentSegment->sched_readCount -
	  currentSegment->sched_inputConsumptionOffset
	  [SCORE_CMB_RAMSRC_ADDR_INNUM];
	currentSegment->sched_outputProduction
	  [SCORE_CMB_RAMSRC_DATA_OUTNUM] = 
	  currentSegment->sched_readCount -
	  currentSegment->sched_outputProductionOffset
	  [SCORE_CMB_RAMSRC_DATA_OUTNUM];
      } else if (currentSegment->sched_mode == SCORE_CMB_RAMSINK) {
	currentSegment->sched_inputConsumption
	  [SCORE_CMB_RAMSINK_ADDR_INNUM] = 
	  currentSegment->sched_writeCount -
	  currentSegment->sched_inputConsumptionOffset
	  [SCORE_CMB_RAMSINK_ADDR_INNUM];
	currentSegment->sched_inputConsumption
	  [SCORE_CMB_RAMSINK_DATA_INNUM] = 
	  currentSegment->sched_writeCount -
	  currentSegment->sched_inputConsumptionOffset
	  [SCORE_CMB_RAMSINK_DATA_INNUM];
      } else if (currentSegment->sched_mode == SCORE_CMB_RAMSRCSINK) {
	currentSegment->sched_inputConsumption
	  [SCORE_CMB_RAMSRCSINK_ADDR_INNUM] = 
	  currentSegment->sched_readCount + 
	  currentSegment->sched_writeCount -
	  currentSegment->sched_inputConsumptionOffset
	  [SCORE_CMB_RAMSRCSINK_ADDR_INNUM];
	currentSegment->sched_inputConsumption
	  [SCORE_CMB_RAMSRCSINK_DATAW_INNUM] = 
	  currentSegment->sched_writeCount -
	  currentSegment->sched_inputConsumptionOffset
	  [SCORE_CMB_RAMSRCSINK_DATAW_INNUM];
	currentSegment->sched_inputConsumption
	  [SCORE_CMB_RAMSRCSINK_WRITE_INNUM] = 
	  currentSegment->sched_readCount + 
	  currentSegment->sched_writeCount -
	  currentSegment->sched_inputConsumptionOffset
	  [SCORE_CMB_RAMSRCSINK_WRITE_INNUM];
	currentSegment->sched_outputProduction
	  [SCORE_CMB_RAMSRCSINK_DATAR_OUTNUM] = 
	  currentSegment->sched_readCount -
	  currentSegment->sched_outputProductionOffset
	  [SCORE_CMB_RAMSRCSINK_DATAR_OUTNUM];
      }

      for (j = 0; j < numInputs; j++) {
	ScoreStream *currentStream = currentSegment->getSchedInput(j);
	char isEmpty = (cmbStatus[i].emptyInputs >> j) & 1;

	if (isEmpty) {
	  currentStream->sched_isPotentiallyEmpty = 1;
	  currentStream->sched_isPotentiallyFull = 0;
	} else {
	  currentStream->sched_isPotentiallyEmpty = 0;
	}

	currentSegment->sched_lastKnownInputFIFONumTokens[j] =
	  cmbStatus[i].inputFIFONumTokens[j];
      }
      for (j = 0; j < numOutputs; j++) {
	ScoreStream *currentStream = currentSegment->getSchedOutput(j);
	char isFull = (cmbStatus[i].fullOutputs >> j) & 1;

	if (isFull) {
	  currentStream->sched_isPotentiallyFull = 1;
	  currentStream->sched_isPotentiallyEmpty = 0;
	} else {
	  currentStream->sched_isPotentiallyFull = 0;
	}
      }

      if (!hasConsumedProduced) {
	if (!(currentSegment->sched_potentiallyDidNotFireLastResident)) {
	  currentSegment->sched_potentiallyDidNotFireLastResident = 1;

	  currentProcess->numPotentiallyNonFiringSegments++;

	  // "reset" the input consumption and output production rates
	  // by setting the offset appropriately.
#if RESETNODETOALLIO
          for (j = 0; j < numInputs; j++) {
            currentSegment->sched_inputConsumptionOffset[j] =
              currentSegment->sched_inputConsumptionOffset[j] +
              currentSegment->sched_inputConsumption[j];
            currentSegment->sched_inputConsumption[j] = 0;

            currentSegment->sched_inputConsumption[j] = 1;
            currentSegment->sched_inputConsumptionOffset[j] =
              currentSegment->sched_inputConsumptionOffset[j] + -1;
          }
          for (j = 0; j < numOutputs; j++) {
            currentSegment->sched_outputProductionOffset[j] =
              currentSegment->sched_outputProductionOffset[j] +
              currentSegment->sched_outputProduction[j];
            currentSegment->sched_outputProduction[j] = 0;

            currentSegment->sched_outputProduction[j] = 1;
            currentSegment->sched_outputProductionOffset[j] =
              currentSegment->sched_outputProductionOffset[j] + -1;
          }
#else
	  for (j = 0; j < numInputs; j++) {
	    currentSegment->sched_inputConsumptionOffset[j] =
	      currentSegment->sched_inputConsumptionOffset[j] +
	      currentSegment->sched_inputConsumption[j];
	    currentSegment->sched_inputConsumption[j] = 0;
	  }
	  for (j = 0; j < numOutputs; j++) {
	    currentSegment->sched_outputProductionOffset[j] =
	      currentSegment->sched_outputProductionOffset[j] +
	      currentSegment->sched_outputProduction[j];
	    currentSegment->sched_outputProduction[j] = 0;
	  }
	  if (currentSegment->sched_mode == SCORE_CMB_SEQSRC) {
	    currentSegment->
	      sched_outputProduction[SCORE_CMB_SEQSRC_DATA_OUTNUM] = 1;
	    currentSegment->
	      sched_outputProductionOffset[SCORE_CMB_SEQSRC_DATA_OUTNUM] = 
	      currentSegment->
	      sched_outputProductionOffset[SCORE_CMB_SEQSRC_DATA_OUTNUM] + -1;
	  } else if (currentSegment->sched_mode == SCORE_CMB_SEQSINK) {
	    currentSegment->
	      sched_inputConsumption[SCORE_CMB_SEQSINK_DATA_INNUM] = 1;
	    currentSegment->
	      sched_inputConsumptionOffset[SCORE_CMB_SEQSINK_DATA_INNUM] = 
	      currentSegment->
	      sched_inputConsumptionOffset[SCORE_CMB_SEQSINK_DATA_INNUM] + -1;
	  } else if (currentSegment->sched_mode == SCORE_CMB_SEQSRCSINK) {
	    currentSegment->
	      sched_inputConsumption[SCORE_CMB_SEQSRCSINK_DATAW_INNUM] = 1;
	    currentSegment->
	      sched_inputConsumptionOffset[SCORE_CMB_SEQSRCSINK_DATAW_INNUM] = 
	      currentSegment->
	      sched_inputConsumptionOffset[SCORE_CMB_SEQSRCSINK_DATAW_INNUM] +
	      -1;
	    currentSegment->
	      sched_outputProduction[SCORE_CMB_SEQSRCSINK_DATAR_OUTNUM] = 1;
	    currentSegment->
	      sched_outputProductionOffset[SCORE_CMB_SEQSRCSINK_DATAR_OUTNUM] =
	      currentSegment->
	      sched_outputProductionOffset[SCORE_CMB_SEQSRCSINK_DATAR_OUTNUM] +
	      -1;
	  } else if (currentSegment->sched_mode == SCORE_CMB_RAMSRC) {
	    currentSegment->
	      sched_inputConsumption[SCORE_CMB_RAMSRC_ADDR_INNUM] = 1;
	    currentSegment->
	      sched_inputConsumptionOffset[SCORE_CMB_RAMSRC_ADDR_INNUM] = 
	      currentSegment->
	      sched_inputConsumptionOffset[SCORE_CMB_RAMSRC_ADDR_INNUM] + -1;
	    currentSegment->
	      sched_outputProduction[SCORE_CMB_RAMSRC_DATA_OUTNUM] = 1;
	    currentSegment->
	      sched_outputProductionOffset[SCORE_CMB_RAMSRC_DATA_OUTNUM] = 
	      currentSegment->
	      sched_outputProductionOffset[SCORE_CMB_RAMSRC_DATA_OUTNUM] + -1;
	  } else if (currentSegment->sched_mode == SCORE_CMB_RAMSINK) {
	    currentSegment->
	      sched_inputConsumption[SCORE_CMB_RAMSINK_ADDR_INNUM] = 1;
	    currentSegment->
	      sched_inputConsumptionOffset[SCORE_CMB_RAMSINK_ADDR_INNUM] = 
	      currentSegment->
	      sched_inputConsumptionOffset[SCORE_CMB_RAMSINK_ADDR_INNUM] + -1;
	    currentSegment->
	      sched_inputConsumption[SCORE_CMB_RAMSINK_DATA_INNUM] = 1;
	    currentSegment->
	      sched_inputConsumptionOffset[SCORE_CMB_RAMSINK_DATA_INNUM] = 
	      currentSegment->
	      sched_inputConsumptionOffset[SCORE_CMB_RAMSINK_DATA_INNUM] + -1;
	  } else if (currentSegment->sched_mode == SCORE_CMB_RAMSRCSINK) {
	    currentSegment->
	      sched_inputConsumption[SCORE_CMB_RAMSRCSINK_ADDR_INNUM] = 1;
	    currentSegment->
	      sched_inputConsumptionOffset[SCORE_CMB_RAMSRCSINK_ADDR_INNUM] =
	      currentSegment->
	      sched_inputConsumptionOffset[SCORE_CMB_RAMSRCSINK_ADDR_INNUM] +
	      -1;
	    currentSegment->
	      sched_inputConsumption[SCORE_CMB_RAMSRCSINK_WRITE_INNUM] = 1;
	    currentSegment->
	      sched_inputConsumptionOffset[SCORE_CMB_RAMSRCSINK_WRITE_INNUM] =
	      currentSegment->
	      sched_inputConsumptionOffset[SCORE_CMB_RAMSRCSINK_WRITE_INNUM] +
	      -1;
	  }
#endif
	}
      } else {
	if (currentSegment->sched_potentiallyDidNotFireLastResident) {
	  currentSegment->sched_potentiallyDidNotFireLastResident = 0;

	  currentProcess->numPotentiallyNonFiringSegments--;
	}
      }
    }
  }

#if DOSCHEDULE_FOR_SCHEDTIME
  endClock = threadCounter->ScoreThreadSchCounterRead();
  diffClock = endClock - startClock;
  current_gatherStatusInfo = diffClock;
#endif

#if DOPROFILING
  endClock = threadCounter->ScoreThreadSchCounterRead();
  diffClock = endClock - startClock;
  if (diffClock < min_gatherStatusInfo) {
    min_gatherStatusInfo = diffClock;
  }
  if (diffClock > max_gatherStatusInfo) {
    max_gatherStatusInfo = diffClock;
  }
  total_gatherStatusInfo = total_gatherStatusInfo + diffClock;
  current_gatherStatusInfo = diffClock;
  cerr << "   gatherStatusInfo() ==> " << 
    diffClock <<
    " cycle(s)" << endl;
#endif
}




///////////////////////////////////////////////////////////////////////////////
// ScoreSchedulerStatic::findDonePagesSegments:
//   Look at the physical array status and determine which pages/segments are 
//     done. Place all done pages on the done page list.
//
// Parameters: none.
//
// Return value: none.
///////////////////////////////////////////////////////////////////////////////
void ScoreSchedulerStatic::findDonePagesSegments() {
  unsigned int i, j;


#if DOPROFILING
  startClock = threadCounter->ScoreThreadSchCounterRead();
#endif

#if DOSCHEDULE_FOR_SCHEDTIME
  startClock = threadCounter->ScoreThreadSchCounterRead();
#endif

  // check the status of all physical CPs to see if there are any pages
  // which have signalled done.
  // check the status of all physical CMBs to see if there are any segments
  // which have signalled done.
  for (i = 0; i < numPhysicalCP; i++) {
    if (cpStatus[i].isDone) {
      ScorePage *donePage = arrayCP[i].active;
      unsigned int numInputs = (unsigned int) donePage->getInputs();
      unsigned int numOutputs = (unsigned int) donePage->getOutputs();

      if (VERBOSEDEBUG || DEBUG) {
	cerr << "SCHED: EXPLICIT DONE PAGE: " << (unsigned int) donePage << 
	  endl;
      }

      // set the done flag on the page.
      // make sure it is not already done!
      if (donePage->sched_isDone) {

	cerr << "SCHEDERR: Page at physical location " << i << 
	  " has already " << "signalled done!" << endl;

	return;

      } else {


	donePage->sched_isDone = 1;


	// register the done page with its I/O.
	for (j = 0; j < numInputs; j++) {
	  SCORE_STREAM attachedStream = donePage->getSchedInput(j);
	  
	  attachedStream->sched_sinkIsDone = 1;
	  // NOTE: DO NOT USE STREAM_UNBIND_SINK SINCE THIS DESTROYS SNKFUNC
	  //       WHICH IS USED TO DETERMINE WHEN TO GC THE STREAM!
	  attachedStream->sched_sink = NULL;
	  attachedStream->sched_snkNum = -1;
	  
	  // if the sink of a stream becomes done, that stream can then no
	  // longer be full!
	  attachedStream->sched_isPotentiallyFull = 0;
	}
	for (j = 0; j < numOutputs; j++) {
	  SCORE_STREAM attachedStream = donePage->getSchedOutput(j);
	  
	  attachedStream->sched_srcIsDone = 1;
	  // NOTE: DO NOT USE STREAM_UNBIND_SRC SINCE THIS DESTROYS SRCFUNC
	  //       WHICH IS USED TO DETERMINE WHEN TO GC THE STREAM!
	  attachedStream->sched_src = NULL;
	  attachedStream->sched_srcNum = -1;
	}

	
	// add this done page to the appropriate lists.
	SCORECUSTOMLIST_APPEND(doneNodeList, donePage);
	SCORECUSTOMQUEUE_QUEUE(doneNodeCheckList, donePage);
	
	// we don't want this status to influence other stages, so clear it.
	cpStatus[i].clearStatus();
	// if this signalled done, then make sure it is not marked
	// sched_potentiallyDidNotFireLastResident.
	if (donePage->sched_potentiallyDidNotFireLastResident) {
	  ScoreProcess *currentProcess = donePage->sched_parentProcess;
	  
	  donePage->sched_potentiallyDidNotFireLastResident = 0;
	  
	  currentProcess->numPotentiallyNonFiringPages--;
	}
      }
    }
  }
  for (i = 0; i < numPhysicalCMB; i++) {
    if (cmbStatus[i].isDone) {
      ScoreSegment *doneSegment = arrayCMB[i].active;
      unsigned int numInputs = (unsigned int) doneSegment->getInputs();
      unsigned int numOutputs = (unsigned int) doneSegment->getOutputs();

      if (VERBOSEDEBUG || DEBUG) {
	cerr << "SCHED: EXPLICIT DONE SEGMENT: " << 
	  (unsigned int) doneSegment << endl;
      }

      // if this is a stitch buffer that is in SEQSINK mode, then ignore
      // this done signal! we will let it retransmit this done signal
      // when we flip the mode of the stitch buffer to SEQSRCSINK and the
      // contents have been allowed to drain.
      if (!(doneSegment->sched_isStitch &&
	    (doneSegment->sched_mode == SCORE_CMB_SEQSINK))) {
	// set the done flag on the segment.
	// make sure it is not already done!
	if (doneSegment->sched_isDone) {
	  cerr << "SCHEDERR: Segment at physical location " << i << 
	    " has already " << "signalled done!" << endl;
	  
	  return;
	} else {
	  doneSegment->sched_isDone = 1;
	}
	
	// we need to set some dump flags.
	if (!(doneSegment->sched_isStitch)) {
	  if ((doneSegment->sched_mode == SCORE_CMB_SEQSRC) ||
	      (doneSegment->sched_mode == SCORE_CMB_RAMSRC)) {
	    doneSegment->sched_dumpOnDone = 0;
	  } else {
	    doneSegment->sched_dumpOnDone = 1;
	  }
	} else {
	  doneSegment->sched_dumpOnDone = 0;
	}


	// register the segment page with its I/O.
	for (j = 0; j < numInputs; j++) {
	  SCORE_STREAM attachedStream = doneSegment->getSchedInput(j);
	  
	  attachedStream->sched_sinkIsDone = 1;
	  // NOTE: DO NOT USE STREAM_UNBIND_SINK SINCE THIS DESTROYS SNKFUNC
	  //       WHICH IS USED TO DETERMINE WHEN TO GC THE STREAM!
	  attachedStream->sched_sink = NULL;
	  attachedStream->sched_snkNum = -1;
	  
	  // if the sink of a stream becomes done, that stream can then no
	  // longer be full!
	  attachedStream->sched_isPotentiallyFull = 0;
	}
	for (j = 0; j < numOutputs; j++) {
	  SCORE_STREAM attachedStream = doneSegment->getSchedOutput(j);
	  
	  attachedStream->sched_srcIsDone = 1;
	  // NOTE: DO NOT USE STREAM_UNBIND_SRC SINCE THIS DESTROYS SRCFUNC
	  //       WHICH IS USED TO DETERMINE WHEN TO GC THE STREAM!
	  attachedStream->sched_src = NULL;
	  attachedStream->sched_srcNum = -1;
	}

	
	// add this done segment to the appropriate lists.
	SCORECUSTOMLIST_APPEND(doneNodeList, doneSegment);
	SCORECUSTOMQUEUE_QUEUE(doneNodeCheckList, doneSegment);
	
	// we don't want this status to influence other stages, so clear it.
	cmbStatus[i].clearStatus();

	// if this signalled done, then make sure it is not marked
	// sched_potentiallyDidNotFireLastResident.
	if (doneSegment->sched_potentiallyDidNotFireLastResident) {
	  ScoreProcess *currentProcess = doneSegment->sched_parentProcess;
	  
	  doneSegment->sched_potentiallyDidNotFireLastResident = 0;
	  
	  currentProcess->numPotentiallyNonFiringSegments--;
	}
      } else {
	if (VERBOSEDEBUG || DEBUG) {
	  cerr << "SCHED: IGNORING EXPLICIT DONE STITCH BUFFER IN SEQSINK! " <<
	    (unsigned int) doneSegment << endl;
	}
      }

      // if this signalled done, then make sure it is not marked
      // sched_potentiallyDidNotFireLastResident.
      if (doneSegment->sched_potentiallyDidNotFireLastResident) {
	ScoreProcess *currentProcess = doneSegment->sched_parentProcess;

	doneSegment->sched_potentiallyDidNotFireLastResident = 0;
	
	currentProcess->numPotentiallyNonFiringSegments--;
      }
    }
  }

  // add the done node to the appropriate list.
  // check for implicitly done pages/segments.
  // in addition, set the done flag on all input/output streams to the node.
  if (!doNotMakeNodesImplicitlyDone) {
    while (!(SCORECUSTOMQUEUE_ISEMPTY(doneNodeCheckList))) {
      ScoreGraphNode *doneNode;
      unsigned int numInputs;
      unsigned int numOutputs;
      
      SCORECUSTOMQUEUE_DEQUEUE(doneNodeCheckList, doneNode);
      numInputs = (unsigned int) doneNode->getInputs();
      numOutputs = (unsigned int) doneNode->getOutputs();

      // check surrounding pages/segments.
      // also set the done flag on all input/output streams.
      for (i = 0; i < numInputs; i++) {
	SCORE_STREAM attachedStream = doneNode->getSchedInput(i);
	
	if (!(attachedStream->sched_srcIsDone)) {
	  if (attachedStream->sched_srcFunc != STREAM_OPERATOR_TYPE) {
	    ScoreGraphNode *attachedNode = attachedStream->sched_src;
	    unsigned int attachedNodeNumInputs = 
	      (unsigned int) attachedNode->getInputs();
	    unsigned int attachedNodeNumOutputs = 
	      (unsigned int) attachedNode->getOutputs();
	    
	    if (checkImplicitDonePagesSegments(attachedNode)) {
	      if (VERBOSEDEBUG || DEBUG) {
		cerr << "SCHED: IMPLICIT DONE NODE: " << 
		  (unsigned int) attachedNode << endl;
	      }
	      
	      attachedNode->sched_isDone = 1;
	      
	      // if this is a segment, then we need to set some dump flags.
	      if (attachedNode->isSegment()) {
		ScoreSegment *attachedSegment = (ScoreSegment *) attachedNode;
		
		if (!(attachedSegment->sched_isStitch)) {
		  if ((attachedSegment->sched_mode == SCORE_CMB_SEQSRC) ||
		      (attachedSegment->sched_mode == SCORE_CMB_RAMSRC)) {
		    attachedSegment->sched_dumpOnDone = 0;
		  } else {
		    attachedSegment->sched_dumpOnDone = 1;
		  }
		} else {
		  attachedSegment->sched_dumpOnDone = 0;
		}
	      }
	      

	      // register the attached node with its I/O.
	      for (j = 0; j < attachedNodeNumInputs; j++) {
		SCORE_STREAM attachedNodeStream = 
		  attachedNode->getSchedInput(j);
		
		attachedNodeStream->sched_sinkIsDone = 1;
		// NOTE: DO NOT USE STREAM_UNBIND_SINK SINCE THIS DESTROYS 
		//       SNKFUNC WHICH IS USED TO DETERMINE WHEN TO GC THE 
		//       STREAM!
		attachedNodeStream->sched_sink = NULL;
		attachedNodeStream->sched_snkNum = -1;
		
		// if the sink of a stream becomes done, that stream can 
		// then no longer be full!
		attachedNodeStream->sched_isPotentiallyFull = 0;
	      }
	      for (j = 0; j < attachedNodeNumOutputs; j++) {
		SCORE_STREAM attachedNodeStream = 
		  attachedNode->getSchedOutput(j);
		
		attachedNodeStream->sched_srcIsDone = 1;
		// NOTE: DO NOT USE STREAM_UNBIND_SRC SINCE THIS DESTROYS 
		//       SRCFUNC WHICH IS USED TO DETERMINE WHEN TO GC THE 
		//       STREAM!
		attachedNodeStream->sched_src = NULL;
		attachedNodeStream->sched_srcNum = -1;
	      }

	      
	      // add attached node to the done list.
	      SCORECUSTOMLIST_APPEND(doneNodeList, attachedNode);
	      SCORECUSTOMQUEUE_QUEUE(doneNodeCheckList, attachedNode);
	      
	      // if the node is currently, resident, then clear the physical
	      // status for that physical page/segment so it does not
	      // interfere with other stages.
	      if (attachedNode->sched_isResident) {
		if (attachedNode->isPage()) {
		  cpStatus[attachedNode->sched_residentLoc].clearStatus();
		} else {
		  cmbStatus[attachedNode->sched_residentLoc].clearStatus();
		}
	      }
	    }
	  }
	}
      }
      for (i = 0; i < numOutputs; i++) {
	SCORE_STREAM attachedStream = doneNode->getSchedOutput(i);
	
	if (!(attachedStream->sched_sinkIsDone)) {
	  if (attachedStream->sched_snkFunc != STREAM_OPERATOR_TYPE) {
	    ScoreGraphNode *attachedNode = attachedStream->sched_sink;
	    unsigned int attachedNodeNumInputs = 
	      (unsigned int) attachedNode->getInputs();
	    unsigned int attachedNodeNumOutputs = 
	      (unsigned int) attachedNode->getOutputs();
	    
	    if (checkImplicitDonePagesSegments(attachedNode)) {
	      if (VERBOSEDEBUG || DEBUG) {
		cerr << "SCHED: IMPLICIT DONE NODE: " << 
		  (unsigned int) attachedNode << endl;
	      }
	      
	      attachedNode->sched_isDone = 1;
	      
	      // if this is a segment, then we need to set some dump flags.
	      if (attachedNode->isSegment()) {
		ScoreSegment *attachedSegment = (ScoreSegment *) attachedNode;
		
		if (!(attachedSegment->sched_isStitch)) {
		  if ((attachedSegment->sched_mode == SCORE_CMB_SEQSRC) ||
		      (attachedSegment->sched_mode == SCORE_CMB_RAMSRC)) {
		    attachedSegment->sched_dumpOnDone = 0;
		  } else {
		    attachedSegment->sched_dumpOnDone = 1;
		  }
		} else {
		  attachedSegment->sched_dumpOnDone = 0;
		}
	      }
	      

	      // register the attached node with its I/O.
	      for (j = 0; j < attachedNodeNumInputs; j++) {
		SCORE_STREAM attachedNodeStream = 
		  attachedNode->getSchedInput(j);
		
		attachedNodeStream->sched_sinkIsDone = 1;
		// NOTE: DO NOT USE STREAM_UNBIND_SINK SINCE THIS DESTROYS 
		//       SNKFUNC WHICH IS USED TO DETERMINE WHEN TO GC THE 
		//       STREAM!
		attachedNodeStream->sched_sink = NULL;
		attachedNodeStream->sched_snkNum = -1;
		
		// if the sink of a stream becomes done, that stream can 
		// then no longer be full!
		attachedNodeStream->sched_isPotentiallyFull = 0;
	      }
	      for (j = 0; j < attachedNodeNumOutputs; j++) {
		SCORE_STREAM attachedNodeStream = 
		  attachedNode->getSchedOutput(j);
		
		attachedNodeStream->sched_srcIsDone = 1;
		// NOTE: DO NOT USE STREAM_UNBIND_SRC SINCE THIS DESTROYS 
		//       SRCFUNC WHICH IS USED TO DETERMINE WHEN TO GC THE 
		//       STREAM!
		attachedNodeStream->sched_src = NULL;
		attachedNodeStream->sched_srcNum = -1;
	      }

	      
	      // add attached node to the done list.
	      SCORECUSTOMLIST_APPEND(doneNodeList, attachedNode);
	      SCORECUSTOMQUEUE_QUEUE(doneNodeCheckList, attachedNode);
	      
	      // if the node is currently, resident, then clear the physical
	      // status for that physical page/segment so it does not
	      // interfere with other stages.
	      if (attachedNode->sched_isResident) {
		if (attachedNode->isPage()) {
		  cpStatus[attachedNode->sched_residentLoc].clearStatus();
		} else {
		  cmbStatus[attachedNode->sched_residentLoc].clearStatus();
		}
	      }
	    }
	  }
	}
      }
    }
  }



#if DOSCHEDULE_FOR_SCHEDTIME
  endClock = threadCounter->ScoreThreadSchCounterRead();
  diffClock = endClock - startClock;
  current_findDonePagesSegments = diffClock;
#endif

#if DOPROFILING
  endClock = threadCounter->ScoreThreadSchCounterRead();
  diffClock = endClock - startClock;
  if (diffClock < min_findDonePagesSegments) {
    min_findDonePagesSegments = diffClock;
  }
  if (diffClock > max_findDonePagesSegments) {
    max_findDonePagesSegments = diffClock;
  }
  total_findDonePagesSegments = total_findDonePagesSegments + diffClock;
  current_findDonePagesSegments = diffClock;
  cerr << "   findDonePagesSegments() ==> " << 
    diffClock <<
    " cycle(s)" << endl;
#endif
}


///////////////////////////////////////////////////////////////////////////////
// ScoreSchedulerStatic::checkImplicitDonePagesSegments:
//   Check to see if a page/segment is implicitly done. (not explicitly
//     signalling done).
//
// Parameters:
//   currentNode: node to check.
//
// Return value: 0 if false; 1 if true.
///////////////////////////////////////////////////////////////////////////////
int ScoreSchedulerStatic::checkImplicitDonePagesSegments(ScoreGraphNode 
						   *currentNode) {
  unsigned int i;
  unsigned int numOutputs = (unsigned int) currentNode->getOutputs();
  
  // make sure it is not already done.
  if (currentNode->sched_isDone) {
    return(0);
  }

  // if it is a page, then it is implicitly done if all of its outputs are
  //   done.
  // if it is a segment, then it is implicitly done if all of its outputs are
  //   done. if it can accept writes, it also must have all of its inputs done.
  for (i = 0; i < numOutputs; i++) {
    SCORE_STREAM attachedStream = currentNode->getSchedOutput(i);

    if (!(attachedStream->sched_sinkIsDone)) {
      return(0);
    }
  }
  if (currentNode->isPage()) {
    return(1);
  } else {
    ScoreSegment *currentSegment = (ScoreSegment *) currentNode;
    unsigned int numInputs = (unsigned int) currentNode->getInputs();

    if (!(currentSegment->sched_isStitch) &&
	((currentSegment->sched_mode == SCORE_CMB_SEQSINK) ||
	 (currentSegment->sched_mode == SCORE_CMB_RAMSINK) ||
	 (currentSegment->sched_mode == SCORE_CMB_SEQSRCSINK) ||
	 (currentSegment->sched_mode == SCORE_CMB_RAMSRCSINK))) {
      for (i = 0; i < numInputs; i++) {
	SCORE_STREAM attachedStream = currentNode->getSchedInput(i);
	
	if (!(attachedStream->sched_srcIsDone)) {
	  return(0);
	}
      }

      return(1);
    } else {
      return(1);
    }
  }
}




///////////////////////////////////////////////////////////////////////////////
// ScoreSchedulerStatic::findFaultedMemSeg:
//   Look at the physical array status and determine which memory segments
//     have faulted on their address. Place these memory segments on the
//     faulted segment list.
//
// Parameters: none.
//
// Return value: none.
///////////////////////////////////////////////////////////////////////////////
void ScoreSchedulerStatic::findFaultedMemSeg() {
  unsigned int i;

#if DOPROFILING
  startClock = threadCounter->ScoreThreadSchCounterRead();
#endif

#if DOSCHEDULE_FOR_SCHEDTIME
  startClock = threadCounter->ScoreThreadSchCounterRead();
#endif

  // check the status of all physical CMBs to see if there are any segments
  // which have faulted on their address.
  // FIX ME! FOR STITCH BUFFERS! WE SHOULD GUARANTEE THAT IF WE FIX AN
  //         OVERFLOWING STITCH BUFFER, THAT WE SET ITS INPUTS TO BE
  //         NOT POTENTIALLY-FULL!
  // FIX ME! SHOULD WE MODIFY THE POTENTIALLY-FULL BITS ON INPUTS FOR
  //         OTHER FAULTING SEGMENTS?
  for (i = 0; i < numPhysicalCMB; i++) {
    if (cmbStatus[i].isFaulted) {
      ScoreSegment *faultedSegment = arrayCMB[i].active;
      

      // set the faulted flag on the segment and record the fault address.
      // make sure it is not a stitch buffer.
      if (faultedSegment->sched_isStitch) {
	if (faultedSegment->sched_mode == SCORE_CMB_SEQSRCSINK) {
	  // FIX ME! WE SHOULD DECIDE WHETHER OR NOT TO INCREASE THE STITCH
	  // BUFFER SIZE! (WHAT IF IT IS A BUFFERLOCK STITCH BUFFER?)
	  // cerr << "SCHEDERR: STITCH BUFFER (SEQSRCSINK) BECAME FULL! " <<
	  //  "DON'T KNOW WHAT TO DO!" << endl;
	} else if (faultedSegment->sched_mode == SCORE_CMB_SEQSINK) {
	  // FIX ME! WE SHOULD DECIDE WHETHER OR NOT TO INCREASE THE STITCH
	  // BUFFER SIZE! (WHAT IF IT IS A BUFFERLOCK STITCH BUFFER?)
	  // cerr << "SCHEDERR: STITCH BUFFER (SEQSINK) BECAME FULL! " <<
	  //  "DON'T KNOW WHAT TO DO!" << endl;
	}
      } else {
	if (VERBOSEDEBUG || DEBUG) {
	  cerr << "SCHED: Detected a faulting CMB at physical CMB location " <<
	    i << "! (addr=" << cmbStatus[i].faultedAddr << ")" << endl;
	}

	// make sure that this is not a real segfault (i.e. really out of
	// bounds!).
	if (cmbStatus[i].faultedAddr < faultedSegment->length()) {
	  faultedSegment->sched_isFaulted = 1;
	  faultedSegment->sched_faultedAddr = cmbStatus[i].faultedAddr;

	  SCORECUSTOMLIST_APPEND(faultedMemSegList, faultedSegment);
	  
	  // clear the status of the CMB location the segment is active in.
	  cmbStatus[i].clearStatus();
	  
	} else {
	  cerr << "SCHEDERR: Segmentation fault at physical CMB location " <<
	    i << "! (addr=" << cmbStatus[i].faultedAddr << ") " << 
	    "(length=" << arrayCMB[i].active->length() << ")" << endl;

	  // FIX ME! WOULD LIKE TO BE ABLE TO KILL THE PROCESS WITHOUT KILLING
	  // THE RUNTIME SYSTEM! THIS IS ALL I CAN DO FOR NOW SO THAT THE
	  // USER SEES THE SEGFAULT!
	  exit(1);
	}
      }
    }
  }

#if DOSCHEDULE_FOR_SCHEDTIME
  endClock = threadCounter->ScoreThreadSchCounterRead();
  diffClock = endClock - startClock;
  current_findFaultedMemSeg = diffClock;
#endif

#if DOPROFILING
  endClock = threadCounter->ScoreThreadSchCounterRead();
  diffClock = endClock - startClock;
  if (diffClock < min_findFaultedMemSeg) {
    min_findFaultedMemSeg = diffClock;
  }
  if (diffClock > max_findFaultedMemSeg) {
    max_findFaultedMemSeg = diffClock;
  }
  total_findFaultedMemSeg = total_findFaultedMemSeg + diffClock;
  current_findFaultedMemSeg = diffClock;
  cerr << "   findFaultedMemSeg() ==> " << 
    diffClock <<
    " cycle(s)" << endl;
#endif
}


///////////////////////////////////////////////////////////////////////////////
// ScoreSchedulerStatic::dealWithDeadLock:
//   Determine if any processes have deadlocked. If so, deal with it!
//
// Parameters: none.
//
// Return value: none.
///////////////////////////////////////////////////////////////////////////////
void ScoreSchedulerStatic::dealWithDeadLock() {
  unsigned int i;


#if DOPROFILING
  startClock = threadCounter->ScoreThreadSchCounterRead();
#endif

#if DOSCHEDULE_FOR_SCHEDTIME
  startClock = threadCounter->ScoreThreadSchCounterRead();
#endif

  // try to find potentially dead locked processes.
  findPotentiallyDeadLockedProcesses();

  if (VERBOSEDEBUG || DEBUG) {
    cerr << 
      "SCHED: ************************ STARTING DEADLOCK CHECK LOOP" << endl;
  }

  // iterate through the dead locked processes and determine if it is
  // really dead locked and if so, which streams should have stitch
  // buffers added (if bufferlocked).
  for (i = 0; i < SCORECUSTOMLIST_LENGTH(deadLockedProcesses); i++) {
    ScoreProcess *currentProcess;
    list<ScoreStream *> bufferLockedStreams;
    list<list<ScoreStream *> *> deadLockedCycles;
    

    SCORECUSTOMLIST_ITEMAT(deadLockedProcesses, i, currentProcess);

    if ((1|| VERBOSEDEBUG || DEBUG) && !DOPROFILING && 
	!DOSCHEDULE_FOR_SCHEDTIME) {
      if (!isPseudoIdeal) {
        cerr << "SCHED: ***************** CHECKING DEADLOCK ON PID " <<
  	  currentProcess->pid << endl;
      }
    }

    // try to find dead locked streams.
    findDeadLock(currentProcess, &bufferLockedStreams, &deadLockedCycles);

    if (deadLockedCycles.length() != 0) {
      if ((1|| VERBOSEDEBUG || DEBUG) && !DOPROFILING &&
	  !DOSCHEDULE_FOR_SCHEDTIME) {
        if (!isPseudoIdeal) {
	  cerr <<
	    "SCHED: ***** POTENTIAL DEADLOCK DETECTED! *****" <<
	    endl;
        }
      }
      if (VERBOSEDEBUG || DEBUG) {
	cerr <<
	  "SCHED: ***** NUMBER OF CYCLES THAT ARE DEADLOCKED: " <<
	  deadLockedCycles.length() << endl;
      }

      currentProcess->numConsecutiveDeadLocks++;

      if (currentProcess->numConsecutiveDeadLocks >=
          SCORE_NUM_CONSECUTIVE_DEADLOCKS_TO_KILL) {
        resolveDeadLockedCycles(&deadLockedCycles);
      } else {
        if ((1|| VERBOSEDEBUG || DEBUG) && !DOPROFILING &&
	    !DOSCHEDULE_FOR_SCHEDTIME) {
          if (!isPseudoIdeal) {
            cerr <<
              "SCHED: NUMBER OF CONSECUTIVE DEADLOCKS: " << 
              currentProcess->numConsecutiveDeadLocks << 
              " (TO KILL: " << SCORE_NUM_CONSECUTIVE_DEADLOCKS_TO_KILL << 
              ")" << endl;
          }
        }
      }
    } else {
      currentProcess->numConsecutiveDeadLocks = 0;
    }

    if (bufferLockedStreams.length() != 0) {
      if (VERBOSEDEBUG || DEBUG) {
	cerr <<
	  "SCHED: ***** POTENTIAL BUFFERLOCK DETECTED! *****" <<
	  endl;
	cerr <<
	  "SCHED: ***** NUMBER OF STREAMS THAT SHOULD HAVE STITCH BUFFERS " <<
	  "ADDED: " << bufferLockedStreams.length() << endl;
      }

      resolveBufferLockedStreams(&bufferLockedStreams);
    }

    if ((1|| VERBOSEDEBUG || DEBUG) && !DOPROFILING &&
	!DOSCHEDULE_FOR_SCHEDTIME) {
      if (!isPseudoIdeal) {
        cerr << "SCHED: ***************** DONE CHECKING DEADLOCK ON PID " <<
  	  currentProcess->pid << endl;
      }
    }
  }

  if (VERBOSEDEBUG || DEBUG) {
    cerr << 
      "SCHED: ************************ ENDING DEADLOCK CHECK LOOP" << endl;
  }

  SCORECUSTOMLIST_CLEAR(deadLockedProcesses);

#if DOSCHEDULE_FOR_SCHEDTIME
  endClock = threadCounter->ScoreThreadSchCounterRead();
  diffClock = endClock - startClock;
  current_dealWithDeadLock = diffClock;
#endif

#if DOPROFILING
  endClock = threadCounter->ScoreThreadSchCounterRead();
  diffClock = endClock - startClock;
  if (diffClock < min_dealWithDeadLock) {
    min_dealWithDeadLock = diffClock;
  }
  if (diffClock > max_dealWithDeadLock) {
    max_dealWithDeadLock = diffClock;
  }
  total_dealWithDeadLock = total_dealWithDeadLock + diffClock;
  current_dealWithDeadLock = diffClock;
  cerr << "   dealWithDeadLock() ==> " << 
    diffClock <<
    " cycle(s)" << endl;
#endif
}

///////////////////////////////////////////////////////////////////////////////
// ScoreSchedulerStatic::createSchedule:
//   Create a static schedule.
//
// Parameters: none.
//
// Return value: none.
///////////////////////////////////////////////////////////////////////////////
void ScoreSchedulerStatic::createSchedule() {
  // create a table of what runs where

  /*

    if need to erase old schedule
      ssched->deleteAllSlices();
  
    foreach slice
      sschedSlice_t *tempslice = NULL;
      if ((tempslice = ssched->addSlice()) == NULL) {
        cout << "failed" << endl;
        exit(0);
      }
      tempslice->cyclesToRun = NUMCYCLES;

      get pages from sschedPageList, insert in tempslice->pageList
      get segments from sschedSegmentList, insert in tempslice->segmentList

   */

  
}

///////////////////////////////////////////////////////////////////////////////
// ScoreSchedulerStatic::applySchedule:
//   Apply a static schedule.
//
// Parameters: none.
//
// Return value: number of cycles to run current time slice
///////////////////////////////////////////////////////////////////////////////
unsigned int ScoreSchedulerStatic::applySchedule() {
  unsigned int cyclesToRun;

  // look in schedule to get slice containing pages/segments to place on array
  sschedSlice_t *currentSlice;
  currentSlice = ssched->getNextSlice();
  cyclesToRun = currentSlice->cyclesToRun;
  

  /*
    add pages/segments to scheduledPageList, scheduledMemSegList    

    update:

      removedPageList
      removedMemSegList
      doneNotRemovedPageList
      doneNotRemovedMemSegList
      scheduledPageList
      scheduledMemSegList
      faultedMemSegList
  */

  return cyclesToRun;

  /*
    sample code here:

  // add the pages/segments which are scheduled and removed onto the scheduled
  //   and removed pages/segments list.
  for (i = 0; i < SCORECUSTOMLIST_LENGTH(doneNodeList); i++) {
    ScoreGraphNode *currentNode;

    SCORECUSTOMLIST_ITEMAT(doneNodeList, i, currentNode);
    
    if (currentNode->sched_isResident) {
      if (currentNode->isPage()) {
	SCORECUSTOMLIST_APPEND(removedPageList, 
			       (ScorePage *) currentNode);
      } else {
	SCORECUSTOMLIST_APPEND(removedMemSegList, 
			       (ScoreSegment *) currentNode);
      }
    } else {
      if (currentNode->isPage()) {
	SCORECUSTOMLIST_APPEND(doneNotRemovedPageList, 
			       (ScorePage *) currentNode);
      } else {
	SCORECUSTOMLIST_APPEND(doneNotRemovedMemSegList, 
			       (ScoreSegment *) currentNode);
      }
    }
  }

  for (i = 0; i < SCORECUSTOMSTACK_LENGTH(scheduledClusterTrial); i++) {
    ScoreCluster *currentCluster;

    SCORECUSTOMSTACK_ITEMAT(scheduledClusterTrial, i, currentCluster);

    // make sure cluster is not already resident!
    if (!(currentCluster->isResident)) {
      for (j = 0; j < SCORECUSTOMLIST_LENGTH(currentCluster->nodeList);
	   j++) {
	ScoreGraphNode *currentNode;
	
	SCORECUSTOMLIST_ITEMAT(currentCluster->nodeList, j, currentNode);
	
	if (currentNode->isPage()) {
	  SCORECUSTOMLIST_APPEND(scheduledPageList,
				 (ScorePage *) currentNode);
	} else {
	  SCORECUSTOMLIST_APPEND(scheduledMemSegList,
				 (ScoreSegment *) currentNode);
	}
      }
    }
  }

  // add stitch buffers which are scheduled/removed/changed mode onto the
  //   scheduled and removed/changed mode segments list.
  // NOTE: Newly formed stitch buffers will be taken care of later.
  // NOTE: We assume that stitch buffers only feeding the processor will
  //       have been on the processor and will NOT be removed from the
  //       array until they have finished feeding the processor!
  for (i = 0; i < SCORECUSTOMLIST_LENGTH(stitchBufferList); i++) {
    ScoreSegmentStitch *currentStitch;

    SCORECUSTOMLIST_ITEMAT(stitchBufferList, i, currentStitch);

    if (!(currentStitch->sched_isDone) &&
	!(currentStitch->sched_isOnlyFeedProcessor)) {
      int currentMode = currentStitch->sched_mode;
      int newMode;
      SCORE_STREAM inStream = currentStitch->getSchedInStream();
      SCORE_STREAM outStream = currentStitch->getSchedOutStream();
      ScoreGraphNode *inNode = inStream->sched_src;
      ScoreGraphNode *outNode = outStream->sched_sink;
      char srcScheduled = 0, sinkScheduled = 0;

      if (inStream->sched_srcIsDone) {
	srcScheduled = 0;
      } else if (inStream->sched_srcFunc == STREAM_OPERATOR_TYPE) {
	// do nothing!
      } else if (inNode->sched_isScheduled) {
	srcScheduled = 1;
      } else {
	srcScheduled = 0;
      }
      
      if (outStream->sched_sinkIsDone) {
	sinkScheduled = 0;
      } else if (outStream->sched_snkFunc == STREAM_OPERATOR_TYPE) {
	// do nothing!
      } else if (outNode->sched_isScheduled) {
	sinkScheduled = 1;
      } else {
	sinkScheduled = 0;
      }

      // take care of special case of src being OPERATOR or sink being
      // OPERATOR.
      // NOTE: WE HAD BETTER NOT HAVE STITCH BUFFERS BETWEEN 2 PROCESSOR
      //       OPERATORS!
      if (inStream->sched_srcFunc == STREAM_OPERATOR_TYPE) {
	srcScheduled = sinkScheduled;
      }
      if (outStream->sched_snkFunc == STREAM_OPERATOR_TYPE) {
	sinkScheduled = srcScheduled;
      }
      
      // if this is a "only feed processor" stitch buffer, then set the
      // sinkScheduled appropriately.
      if (currentStitch->sched_isOnlyFeedProcessor) {
	sinkScheduled = 1;
      }

      if (srcScheduled && sinkScheduled) {
	newMode = SCORE_CMB_SEQSRCSINK;
      } else if (srcScheduled) {
	newMode = SCORE_CMB_SEQSINK;
      } else if (sinkScheduled) {
	// the reason we need to use SRCSINK is that there may still be
	// tokens on the input queue that haven't been processed!
	newMode = SCORE_CMB_SEQSRCSINK;
      } else {
	newMode = currentMode;
      }
      
      currentStitch->sched_mode = newMode;

      if (currentStitch->sched_isScheduled && 
	  !(currentStitch->sched_isResident)) {
	SCORECUSTOMLIST_APPEND(scheduledMemSegList, currentStitch);

        if (currentStitch->sched_isNewStitch) {
          currentStitch->sched_old_mode = newMode;
        } else {
          currentStitch->sched_old_mode = currentMode;
        }

	if ((currentStitch->sched_old_mode == SCORE_CMB_SEQSINK) &&
	    (currentStitch->sched_mode == SCORE_CMB_SEQSRCSINK)) {
	  currentStitch->sched_this_segment_is_done = 0;
	}
      } else if (!(currentStitch->sched_isScheduled) && 
		 currentStitch->sched_isResident) {
	SCORECUSTOMLIST_APPEND(removedMemSegList, currentStitch);
      } else if (currentStitch->sched_isScheduled && 
		 currentStitch->sched_isResident) {
	if (currentMode != newMode) {
	  SCORECUSTOMLIST_APPEND(configChangedStitchSegList, currentStitch);

	  currentStitch->sched_old_mode = currentMode;
	}
      }
    }
  }

  */

}


///////////////////////////////////////////////////////////////////////////////
// ScoreSchedulerStatic::performPlacement:
//   Given scheduledPageList, scheduledMemSegList, removedPageList, and
//     removedMemSegList, doneNotRemovedPageList, doneNotRemovedMemSegList,
//     faultedMemSegList find the best placement for the pages.
//
// Parameters: none.
//
// Return value: none.
///////////////////////////////////////////////////////////////////////////////
void ScoreSchedulerStatic::performPlacement() {
  unsigned int i, j;

#if PROFILE_PERFORMPLACEMENT
  unsigned long long startTime, endTime;
#endif

#if DOPROFILING
  startClock = threadCounter->ScoreThreadSchCounterRead();
#endif

#if PROFILE_PERFORMPLACEMENT
  startTime = threadCounter->ScoreThreadSchCounterRead();
#endif

#if DOSCHEDULE_FOR_SCHEDTIME
  startClock = threadCounter->ScoreThreadSchCounterRead();
#endif

  //////////////////////////////////////////////////////////////////////////
  // - REMOVE THE PAGES/SEGMENTS THAT ARE SUPPOSED TO BE REMOVED.
  // - IF THE PAGE/SEGMENT IS NOT DONE, THEN MARK ITS SEGMENT BLOCK CACHED 
  //     (BUT DO NOT ACTUALLY DUMP THEM BACK TO MAIN MEMORY).
  // - IF THE PAGE/SEGMENT IS DONE, THEN ACTUALLY FREE ITS SEGMENT BLOCK
  //     (IF THE SEGMENT IS MARKED DUMPONDONE, THEN ARRANGE FOR ITS DATA TO
  //      BE DUMPED BACK TO MAIN MEMORY).
  // - IF THERE WERE ANY PAGES/SEGMENTS WHICH WERE DONE BUT NOT CURRENTLY
  //     RESIDENT, THEN FREE THEIR CACHED SEGMENT BLOCKS (IF ANY).
  //     (IF A SEGMENT IS MARKED DUMPONDONE, THEN ARRANGE FOR ITS DATA TO
  //      BE DUMPED BACK TO MAIN MEMORY).
  //////////////////////////////////////////////////////////////////////////

  // remove the pages/memory segments that were indicated.
  for (i = 0; i < SCORECUSTOMLIST_LENGTH(removedPageList); i++) {
    ScorePage *removedPage;
    unsigned int removedPageLoc;
    ScoreSegmentBlock *cachedBlock;
    ScoreSegmentTable *cachedTable;

    SCORECUSTOMLIST_ITEMAT(removedPageList, i, removedPage);
    removedPageLoc = removedPage->sched_residentLoc;
    cachedBlock = removedPage->sched_cachedSegmentBlock;
    cachedTable = cachedBlock->parentTable;

    arrayCP[removedPageLoc].scheduled = NULL;

    SCORECUSTOMQUEUE_QUEUE(unusedPhysicalCPs, removedPageLoc);

    if (removedPage->sched_isDone) {
      cachedTable->freeUsedLevel1Block(cachedBlock);
      removedPage->sched_cachedSegmentBlock = NULL;
    } else {
      cachedTable->markCachedLevel1Block(cachedBlock);

      removedPage->sched_dumpSegmentBlock = 
        removedPage->sched_cachedSegmentBlock;
    }
  }
  for (i = 0; i < SCORECUSTOMLIST_LENGTH(removedMemSegList); i++) {
    ScoreSegment *removedSegment;
    unsigned int removedSegmentLoc;
    ScoreSegmentBlock *cachedBlock;
    ScoreSegmentTable *cachedTable;

    SCORECUSTOMLIST_ITEMAT(removedMemSegList, i, removedSegment);
    removedSegmentLoc = removedSegment->sched_residentLoc;
    cachedBlock = removedSegment->sched_cachedSegmentBlock;
    cachedTable = cachedBlock->parentTable;

    arrayCMB[removedSegmentLoc].scheduled = NULL;
    
    SCORECUSTOMLINKEDLIST_APPEND(unusedPhysicalCMBs, 
				 removedSegmentLoc, 
				 arrayCMB[removedSegmentLoc].unusedPhysicalCMBsItem);

    if (removedSegment->sched_isDone) {
      if (removedSegment->sched_dumpOnDone) {
	if (!(removedSegment->sched_isStitch)) {
	  // FIX ME! SHOULD REALLY ADVANCE DATA BUFFER POINTER ACCORDING TO
	  // ACTUAL DATA WIDTH!
	  cachedTable->addToDumpBlockList(cachedBlock, 0,
	    (((unsigned long long *) removedSegment->data())+
	     removedSegment->sched_residentStart),
	    removedSegment->sched_residentLength*(SCORE_ALIGN_BITWIDTH_TO_8(removedSegment->width())/8));
	} else {
	  // FIX ME! SHOULD REALLY ADVANCE DATA BUFFER POINTER ACCORDING TO
	  // ACTUAL DATA WIDTH!
	  cachedTable->addToDumpBlockList_useOwnerAddrs(cachedBlock, 0,
	    (((unsigned long long *) removedSegment->data())+
	     removedSegment->sched_residentStart));
	}
      }

      cachedTable->freeUsedLevel0Block(cachedBlock);
      removedSegment->sched_cachedSegmentBlock = NULL;
    } else {
      cachedTable->markCachedLevel0Block(cachedBlock);

      removedSegment->sched_dumpSegmentBlock = 
        removedSegment->sched_cachedSegmentBlock;
    }
  }
  for (i = 0; i < SCORECUSTOMLIST_LENGTH(doneNotRemovedPageList); i++) {
    ScorePage *donePage;

    SCORECUSTOMLIST_ITEMAT(doneNotRemovedPageList, i, donePage);

    if (donePage->sched_cachedSegmentBlock != NULL) {
      ScoreSegmentBlock *cachedBlock = 
	donePage->sched_cachedSegmentBlock;
      ScoreSegmentTable *cachedTable = cachedBlock->parentTable;

      cachedTable->freeCachedLevel1Block(cachedBlock);
      donePage->sched_cachedSegmentBlock = NULL;
    }
  }
  SCORECUSTOMLIST_CLEAR(doneNotRemovedPageList);
  for (i = 0; i < SCORECUSTOMLIST_LENGTH(doneNotRemovedMemSegList); i++) {
    ScoreSegment *doneSegment;

    SCORECUSTOMLIST_ITEMAT(doneNotRemovedMemSegList, i, doneSegment);

    if (doneSegment->sched_cachedSegmentBlock != NULL) {
      ScoreSegmentBlock *cachedBlock = 
	doneSegment->sched_cachedSegmentBlock;
      ScoreSegmentTable *cachedTable = cachedBlock->parentTable;

      if (doneSegment->sched_dumpOnDone) {
	if (!(doneSegment->sched_isStitch)) {
	  // FIX ME! SHOULD REALLY ADVANCE DATA BUFFER POINTER ACCORDING TO
	  // ACTUAL DATA WIDTH!
	  cachedTable->addToDumpBlockList(cachedBlock, 0,
            (((unsigned long long *) doneSegment->data())+
             doneSegment->sched_residentStart),
            doneSegment->sched_residentLength*(SCORE_ALIGN_BITWIDTH_TO_8(doneSegment->width())/8));
	} else {
	  // FIX ME! SHOULD REALLY ADVANCE DATA BUFFER POINTER ACCORDING TO
	  // ACTUAL DATA WIDTH!
	  cachedTable->addToDumpBlockList_useOwnerAddrs(cachedBlock, 0,
            (((unsigned long long *) doneSegment->data())+
             doneSegment->sched_residentStart));
	}
      }

      cachedTable->freeCachedLevel0Block(cachedBlock);
      doneSegment->sched_cachedSegmentBlock = NULL;
    }
  }
  SCORECUSTOMLIST_CLEAR(doneNotRemovedMemSegList);

#if PROFILE_PERFORMPLACEMENT
  endTime = threadCounter->ScoreThreadSchCounterRead();
  cerr << "****** PERFORMPLACMENT0: " << endTime-startTime << endl;

  startTime = threadCounter->ScoreThreadSchCounterRead();
#endif

  //////////////////////////////////////////////////////////////////////////
  // - CHECK ALL OF THE PAGES WHICH ARE TO BE ADDED THAT HAVE THEIR 
  //     SEGMENT BLOCKS CURRENTLY CACHED ON THE ARRAY IN A CMB. MARK 
  //     THOSE CACHED SEGMENT BLOCKS AS USED TO KEEP THEM FROM BEING EVICTED.
  // - IN ADDITION, LOCK DOWN THE LOCATIONS FOR ALL OF THE ADDED PAGES
  //     INTO FREE PHYSICAL PAGES (CURRENTLY, NO PLACEMENT OPTIMIZATION
  //     IS CONSIDERED).
  // - CHECK ALL OF THE SEGMENTS WHICH ARE TO BE ADDED THAT HAVE THEIR
  //     SEGMENT BLOCKS CURRENTLY CACHED ON THE ARRAY IN A CMB. IF THE
  //     CMB IN WHICH THE SEGMENT BLOCK IS CACHED IS CURRENTLY FREE, THEN
  //     LOCK THE SEGMENT INTO THE FREE PHYSICAL CMB AS WELL AS MARKING
  //     THE CACHED SEGMENT BLOCK AS USED TO KEEP IT FROM BEING EVICTED.
  //     OTHERWISE, ARRANGE FOR THE CACHED SEGMENT BLOCK TO BE DUMPED
  //     BACK TO MAIN MEMORY (IF THE SEGMENT IS SEQ/RAMSRC THEN DO NOT
  //     DUMP BACK THE DATA).
  //////////////////////////////////////////////////////////////////////////

  // FIX ME! PERHAPS TRY TO AVOID ALLOCATING BLOCKS IN CMBS WHICH WILL BE
  //         USED FOR LOADING CONFIG/STATE/FIFO ON THIS TIMESLICE!

  // FIX ME! SHOULD ALLOW CACHING IN MULTIPLE LOCATIONS!

  // FIX ME! LATER ON, SHOULD TRY CACHING OF THE CONFIG/STATE/FIFO INSIDE OF
  //         CPS/CMBS THENSELF AND SKIP THE LOADING PROCESS ALSO!

  // FIX ME! SHOULD TRY TO DO CMB-TO-CMB DIRECT TRANSFERS INSTEAD OF
  //         DUMPING TO PRIMARY FIRST THEM BACK TO ARRAY!

  // FIX ME! SHOULD FAVOR EVICTING SEGMENTS THAT ARE READ-ONLY!

  // make sure that, if any of the scheduled pages/segments have cached
  //   segment blocks, those blocks are locked to prevent them from being
  //   freed.
  // try to lock down the locations for scheduled segments if they already
  //   have cached data segments.
  // we can first place all of the pages, because their placement currently
  // is relatively unimportant. (i.e. it will not affect allocation of
  // blocks in CMBs).
  // NOTE: FIX ME! In the future, this may not be the case when we start
  //       being more intelligent and worrying about placement/routing
  //       etc.
  for (i = 0; i < SCORECUSTOMLIST_LENGTH(scheduledPageList); i++) {
    ScorePage *addedPage;
    unsigned int unusedLoc;

    SCORECUSTOMLIST_ITEMAT(scheduledPageList, i, addedPage);
    SCORECUSTOMQUEUE_DEQUEUE(unusedPhysicalCPs, unusedLoc);

    arrayCP[unusedLoc].scheduled = addedPage;
    addedPage->sched_residentLoc = unusedLoc;

    if (addedPage->sched_cachedSegmentBlock != NULL) {
      ScoreSegmentBlock *cachedBlock = addedPage->sched_cachedSegmentBlock;
      ScoreSegmentTable *cachedTable = cachedBlock->parentTable;

      cachedTable->markUsedLevel1Block(cachedBlock);
    }
  }
  for (i = 0; i < SCORECUSTOMLIST_LENGTH(scheduledMemSegList); i++) {
    ScoreSegment *addedSegment;

    SCORECUSTOMLIST_ITEMAT(scheduledMemSegList, i, addedSegment);

    if (addedSegment->sched_cachedSegmentBlock != NULL) {
      ScoreSegmentBlock *cachedBlock = addedSegment->sched_cachedSegmentBlock;
      ScoreSegmentTable *cachedTable = cachedBlock->parentTable;
      unsigned int cachedLoc = cachedTable->loc;

      // check to see if the CMB location is free; if so, lock down the
      // data segment block and assign the segment to this CMB.
      if (arrayCMB[cachedLoc].scheduled == NULL) {
	arrayCMB[cachedLoc].scheduled = addedSegment;
	addedSegment->sched_residentLoc = cachedLoc;

	SCORECUSTOMLINKEDLIST_DELITEM(unusedPhysicalCMBs,
				      arrayCMB[cachedLoc].unusedPhysicalCMBsItem);

	cachedTable->markUsedLevel0Block(cachedBlock);

	addedSegment->sched_pboAddr = cachedBlock->start;
      } else {
	ScoreSegmentBlock *cachedBlock =
	  addedSegment->sched_cachedSegmentBlock;

	// FIX ME! IF WE WERE DOING CMB-TO-CMB TRANSFERS, WE SHOULDN'T DUMP
	//         THE SEGMENT BLOCK BACK TO MEMORY!

	// FIX ME! SHOULD REALLY ADVANCE DATA BUFFER POINTER ACCORDING TO
	// ACTUAL DATA WIDTH!
	if ((addedSegment->sched_mode != SCORE_CMB_SEQSRC) &&
	    (addedSegment->sched_mode != SCORE_CMB_RAMSRC)) {
	  if (!(addedSegment->sched_isStitch)) {
	    cachedTable->addToDumpBlockList(cachedBlock, (0<<2),
              (((unsigned long long *) addedSegment->data())+
	       addedSegment->sched_residentStart),
              addedSegment->sched_residentLength*(SCORE_ALIGN_BITWIDTH_TO_8(addedSegment->width())/8));
	  } else {
	    cachedTable->addToDumpBlockList_useOwnerAddrs(cachedBlock, (0<<2),
              (((unsigned long long *) addedSegment->data())+
	       addedSegment->sched_residentStart));
	  }
	}
	cachedTable->addToDumpBlockList(cachedBlock, 
	  SCORE_DATASEGMENTBLOCK_LOADSIZE,
          addedSegment->sched_fifoBuffer,
          SCORE_MEMSEGFIFO_SIZE);

	cachedTable->freeCachedLevel0Block(cachedBlock);
	addedSegment->sched_cachedSegmentBlock = NULL;
      }
    }
  }

  //////////////////////////////////////////////////////////////////////////
  // - FOR ALL REMAINING SEGMENTS THAT ARE TO BE ADDED BUT DO NOT HAVE
  //     THEIR DATA/FIFO CURRENTLY CACHED IN A FREE PHYSICAL CMB,
  //     ARBITRARILY ASSIGN IT TO A FREE PHYSICAL CMB. THEN ATTEMPT TO
  //     ALLOCATE A FREE LEVEL0 SEGMENT BLOCK IN THAT PHYSICAL CMB. IF
  //     NO FREE LEVEL0 SEGMENT BLOCK EXISTS IN THAT CMB, THEN EVICT A
  //     CACHED (BUT NOT USED) LEVEL0 SEGMENT BLOCK. (WE CAN GUARANTEE
  //     SUCCESS BECAUSE WE WILL GUARANTEE IN ALLOCATION THAT WE NEVER
  //     ALLOCATE ALL OF A CMB'S BLOCKS TO LEVEL1 BLOCKS).
  //     - WHEN EVICTING A LEVEL0 SEGMENT BLOCK, IF THE SEGMENT IS A
  //         SEQ/RAMSRC THEN ITS DATA WILL NOT BE DUMPED TO MAIN MEMORY.
  //     - WHEN LOADING A SEGMENT INTO A LEVEL0 SEGMENT BLOCK, IF THE
  //         FIFO DATA IS NOT VALID (I.E. THE SEGMENT HAS NEVER BEEN
  //         RESIDENT BEFORE), THE FIFO DATA IS NOT LOADED.
  // - FOR ALL REMAINING PAGES THAT ARE TO BE ADDED BUT DO NOT HAVE
  //     THEIR CONFIG/STATE/FIFO CURRENTLY CACHED IN A FREE PHYSICAL CMB,
  //     FIRST TRY TO FIND A FREE LEVEL1 SEGMENT BLOCK. IF NO SUCH SEGMENT
  //     BLOCK EXISTS, FIND A CACHED (BUT NOT USED) LEVEL1 SEGMENT BLOCK.
  //     IF NO SUCH SEGMENT BLOCK EXISTS, FIND A CACHED (BUT NOT USED)
  //     LEVEL0 SEGMENT BLOCK. WE ATTEMPT TO ALLOCATE THE SEGMENT BLOCKS
  //     EVENLY THROUGHOUT THE PHYSICAL CMBS TO MAXIMIZE PARALLEL LOAD/DUMP
  //     OPPORTUNITIES.
  //////////////////////////////////////////////////////////////////////////

  // since we are going to guarantee in the segment tables of each CMB
  // that there will at least be a level0 block for the resident segment
  // (we may have to bump off a cached segment, but we should never have to
  // bump off a level1 block for a currently resident page), then we
  // can simply assign the remaining segments to whatever unused physical CMB
  // there are left.
  for (i = 0; i < SCORECUSTOMLIST_LENGTH(scheduledMemSegList); i++) {
    ScoreSegment *addedSegment;
    ScoreSegmentStitch *addedStitch;

    SCORECUSTOMLIST_ITEMAT(scheduledMemSegList, i, addedSegment);
    addedStitch = (ScoreSegmentStitch *) addedSegment;

    if (addedSegment->sched_cachedSegmentBlock == NULL) {
      unsigned int unusedLoc;
      ScoreSegmentBlock *cachedBlock = NULL;
      ScoreSegmentTable *cachedTable;

      SCORECUSTOMLINKEDLIST_POP(unusedPhysicalCMBs, unusedLoc);
      cachedTable = arrayCMB[unusedLoc].segmentTable;

      arrayCMB[unusedLoc].scheduled = addedSegment;
      addedSegment->sched_residentLoc = unusedLoc;

      arrayCMB[unusedLoc].unusedPhysicalCMBsItem = SCORECUSTOMLINKEDLIST_NULL;

      cachedBlock = cachedTable->allocateLevel0Block(addedSegment);
    
      // if we could not get a free block, then we will have to forcibly
      // evict a cached block.
      if (cachedBlock == NULL) {
	// we will now forcibly evict a cached block.
	// FIX ME! IN THE FUTURE, WE MAY WANT TO CONSIDER EVICTING LEVEL1
	//         BLOCKS ALSO! FOR NOW, KEEP IT SIMPLE AND JUST EVICT A
	//         LEVEL0 BLOCK SINCE IT IS GUARANTEED THAT THERE IS AN
	//         UNUSED CACHED LEVEL0 BLOCK AVAILABLE!
	
	ScoreSegmentBlock *evictedBlock;
	ScoreGraphNode *evictedNode;
	ScoreSegment *evictedSegment;

	SCORECUSTOMLINKEDLIST_FRONT(cachedTable->cachedList_Level0Blocks,
				    evictedBlock);
	evictedNode = evictedBlock->owner;
	evictedSegment = (ScoreSegment *) evictedNode;

	// FIX ME! SHOULD REALLY ADVANCE DATA BUFFER POINTER ACCORDING TO
	// ACTUAL DATA WIDTH!
	if ((evictedSegment->sched_mode != SCORE_CMB_SEQSRC) &&
	    (evictedSegment->sched_mode != SCORE_CMB_RAMSRC)) {
	  if (!(evictedSegment->sched_isStitch)) {
	    cachedTable->addToDumpBlockList(evictedBlock, 0,
              (((unsigned long long *) evictedSegment->data())+
	       evictedSegment->sched_residentStart),
              evictedSegment->sched_residentLength*(SCORE_ALIGN_BITWIDTH_TO_8(evictedSegment->width())/8));
	  } else {
	    cachedTable->addToDumpBlockList_useOwnerAddrs(evictedBlock, 0,
              (((unsigned long long *) evictedSegment->data())+
	       evictedSegment->sched_residentStart));
	  }
	}
	cachedTable->addToDumpBlockList(evictedBlock, 
          SCORE_DATASEGMENTBLOCK_LOADSIZE,
          evictedSegment->sched_fifoBuffer,
          SCORE_MEMSEGFIFO_SIZE);

	cachedTable->freeCachedLevel0Block(evictedBlock);
	evictedSegment->sched_cachedSegmentBlock = NULL;

	cachedBlock = cachedTable->allocateLevel0Block(addedSegment);
      }

      addedSegment->sched_cachedSegmentBlock = cachedBlock;
      
      addedSegment->sched_pboAddr = cachedBlock->start;

      if (!(addedSegment->sched_isStitch && addedStitch->sched_isNewStitch)) {
	if (!(addedSegment->sched_isStitch)) {
	  cachedTable->addToLoadBlockList(cachedBlock, 0,
            (((unsigned long long *) addedSegment->data())+
             addedSegment->sched_residentStart),
            addedSegment->sched_residentLength*(SCORE_ALIGN_BITWIDTH_TO_8(addedSegment->width())/8));
	} else {
	  cachedTable->addToLoadBlockList_useOwnerAddrs(cachedBlock, 0,
            (((unsigned long long *) addedSegment->data())+
             addedSegment->sched_residentStart));
	}
      } else {
        addedStitch->sched_isNewStitch = 0;
      }
      if (addedSegment->sched_isFIFOBufferValid) {
	cachedTable->addToLoadBlockList(cachedBlock,
          SCORE_DATASEGMENTBLOCK_LOADSIZE,
          addedSegment->sched_fifoBuffer,
          SCORE_MEMSEGFIFO_SIZE);
      }
    }
  }

  // for now, since the location of the config/state/fifo for a page are
  // unimportant (except for the fact that we would like to spread them
  // out to increase parallel context switching opportunities) then we
  // simply need to find any free level1 block (or create such a block
  // by evicting a cached level0 or level1 block).
  for (i = 0; i < SCORECUSTOMLIST_LENGTH(scheduledPageList); i++) {
    ScorePage *addedPage;

    SCORECUSTOMLIST_ITEMAT(scheduledPageList, i, addedPage);

    if (addedPage->sched_cachedSegmentBlock == NULL) {
      unsigned int cachedTableLoc = 0;
      ScoreSegmentBlock *cachedBlock = NULL;
      ScoreSegmentTable *cachedTable = NULL;

      // randomly pick a CMB to try. then, try to get a free level1, then
      // try to evict a level1, finally try to evict a level0... do this
      // numPhysicalCMB number of times... if this still fails, then
      // we will try 1 last ditch effort to sequentially go through the
      // CMBs... if that finally fails, then we don't have enough resources
      // to cache the config/state/fifo for all resident CPs.
      for (j = 0; j < numPhysicalCMB; j++) {
	cachedTableLoc = random()%numPhysicalCMB;
	cachedTable = arrayCMB[cachedTableLoc].segmentTable;
	cachedBlock = cachedTable->allocateLevel1Block(addedPage);

	if (cachedBlock != NULL) {
	  break;
	}
	
	if (!(SCORECUSTOMLINKEDLIST_ISEMPTY(cachedTable->cachedList_Level1Blocks))) {
	  ScoreSegmentBlock *evictedBlock;
	  ScoreGraphNode *evictedNode;
	  ScorePage *evictedPage;

	  SCORECUSTOMLINKEDLIST_FRONT(cachedTable->cachedList_Level1Blocks,
				      evictedBlock);
	  evictedNode = evictedBlock->owner;
	  evictedPage = (ScorePage *) evictedNode;
	      
	  cachedTable->addToDumpBlockList(evictedBlock,
	    SCORE_PAGECONFIG_SIZE, 
            evictedPage->bitstream(),
	    SCORE_PAGESTATE_SIZE);
	  cachedTable->addToDumpBlockList(evictedBlock,
            (SCORE_PAGECONFIG_SIZE+SCORE_PAGESTATE_SIZE),
            evictedPage->sched_fifoBuffer,
            SCORE_PAGEFIFO_SIZE);

	  cachedTable->freeCachedLevel1Block_nomerge(evictedBlock);
	  evictedPage->sched_cachedSegmentBlock = NULL;
	      
	  cachedBlock = cachedTable->allocateLevel1Block(addedPage);
	      
	  break;
	}

	// NOTE: We will skip any CMB which does not have more than
	//       1 level0 block combined in cached and free list!
	//       This is to guarantee that segments resident in a CMB
	//       will always be able to get a level0 segment block
	//       without having to evict a used level1 segment block!
	if (!(SCORECUSTOMLINKEDLIST_ISEMPTY(cachedTable->cachedList_Level0Blocks)) &&
	    ((SCORECUSTOMLINKEDLIST_LENGTH(cachedTable->cachedList_Level0Blocks)+
	      SCORECUSTOMLINKEDLIST_LENGTH(cachedTable->freeList_Level0Blocks)+
	      SCORECUSTOMLINKEDLIST_LENGTH(cachedTable->usedList_Level0Blocks)) != 1)) {
	  ScoreSegmentBlock *evictedBlock;
	  ScoreGraphNode *evictedNode;
	  ScoreSegment *evictedSegment;

	  SCORECUSTOMLINKEDLIST_FRONT(cachedTable->cachedList_Level0Blocks,
				      evictedBlock);
	  evictedNode = evictedBlock->owner;
	  evictedSegment = (ScoreSegment *) evictedNode;

	  // FIX ME! SHOULD REALLY ADVANCE DATA BUFFER POINTER ACCORDING TO
	  // ACTUAL DATA WIDTH!
	  if ((evictedSegment->sched_mode != SCORE_CMB_SEQSRC) &&
	      (evictedSegment->sched_mode != SCORE_CMB_RAMSRC)) {
	    if (!(evictedSegment->sched_isStitch)) {
	      cachedTable->addToDumpBlockList(evictedBlock, 0,
                (((unsigned long long *) evictedSegment->data())+
	         evictedSegment->sched_residentStart),
                evictedSegment->sched_residentLength*(SCORE_ALIGN_BITWIDTH_TO_8(evictedSegment->width())/8));
	    } else {
	      cachedTable->addToDumpBlockList_useOwnerAddrs(evictedBlock, 0,
                (((unsigned long long *) evictedSegment->data())+
	         evictedSegment->sched_residentStart));
	    }
	  }
	  cachedTable->addToDumpBlockList(evictedBlock, 
            SCORE_DATASEGMENTBLOCK_LOADSIZE,
            evictedSegment->sched_fifoBuffer,
            SCORE_MEMSEGFIFO_SIZE);

	  cachedTable->freeCachedLevel0Block(evictedBlock);
	  evictedSegment->sched_cachedSegmentBlock = NULL;

	  cachedBlock = cachedTable->allocateLevel1Block(addedPage);
	      
	  break;
	}
      }
      if (cachedBlock == NULL) {
	for (cachedTableLoc = 0; cachedTableLoc < numPhysicalCMB; cachedTableLoc++) {
	  cachedTable = arrayCMB[cachedTableLoc].segmentTable;
	  cachedBlock = cachedTable->allocateLevel1Block(addedPage);

	  if (cachedBlock != NULL) {
	    break;
	  }
	
	  if (!(SCORECUSTOMLINKEDLIST_ISEMPTY(cachedTable->cachedList_Level1Blocks))) {
	    ScoreSegmentBlock *evictedBlock;
	    ScoreGraphNode *evictedNode;
	    ScorePage *evictedPage;
	      
	    SCORECUSTOMLINKEDLIST_FRONT(cachedTable->cachedList_Level1Blocks,
					evictedBlock);
	    evictedNode = evictedBlock->owner;
	    evictedPage = (ScorePage *) evictedNode;
	      
	    cachedTable->addToDumpBlockList(evictedBlock,
					    SCORE_PAGECONFIG_SIZE, 
					    evictedPage->bitstream(),
					    SCORE_PAGESTATE_SIZE);
	    cachedTable->addToDumpBlockList(evictedBlock,
					    (SCORE_PAGECONFIG_SIZE+SCORE_PAGESTATE_SIZE),
					    evictedPage->sched_fifoBuffer,
					    SCORE_PAGEFIFO_SIZE);

	    cachedTable->freeCachedLevel1Block_nomerge(evictedBlock);
	    evictedPage->sched_cachedSegmentBlock = NULL;
	      
	    cachedBlock = cachedTable->allocateLevel1Block(addedPage);
	      
	    break;
	  }

	  // NOTE: We will skip any CMB which does not have more than
	  //       1 level0 block combined in cached and free list!
	  //       This is to guarantee that segments resident in a CMB
	  //       will always be able to get a level0 segment block
	  //       without having to evict a used level1 segment block!
	  if (!(SCORECUSTOMLINKEDLIST_ISEMPTY(cachedTable->cachedList_Level0Blocks)) &&
	      ((SCORECUSTOMLINKEDLIST_LENGTH(cachedTable->cachedList_Level0Blocks)+
		SCORECUSTOMLINKEDLIST_LENGTH(cachedTable->freeList_Level0Blocks)+
		SCORECUSTOMLINKEDLIST_LENGTH(cachedTable->usedList_Level0Blocks)) != 1)) {
	    ScoreSegmentBlock *evictedBlock;
	    ScoreGraphNode *evictedNode;
	    ScoreSegment *evictedSegment;

	    SCORECUSTOMLINKEDLIST_FRONT(cachedTable->cachedList_Level0Blocks,
					evictedBlock);
	    evictedNode = evictedBlock->owner;
	    evictedSegment = (ScoreSegment *) evictedNode;

	    // FIX ME! SHOULD REALLY ADVANCE DATA BUFFER POINTER ACCORDING TO
	    // ACTUAL DATA WIDTH!
	    if ((evictedSegment->sched_mode != SCORE_CMB_SEQSRC) &&
		(evictedSegment->sched_mode != SCORE_CMB_RAMSRC)) {
	      if (!(evictedSegment->sched_isStitch)) {
		cachedTable->addToDumpBlockList(evictedBlock, 0,
	          (((unsigned long long *) evictedSegment->data())+
	          evictedSegment->sched_residentStart),
	          evictedSegment->sched_residentLength*(SCORE_ALIGN_BITWIDTH_TO_8(evictedSegment->width())/8));
	      } else {
		cachedTable->addToDumpBlockList_useOwnerAddrs(evictedBlock, 0,
	          (((unsigned long long *) evictedSegment->data())+
	          evictedSegment->sched_residentStart));
	      }
	    }
	    cachedTable->addToDumpBlockList(evictedBlock, 
					    SCORE_DATASEGMENTBLOCK_LOADSIZE,
					    evictedSegment->sched_fifoBuffer,
					    SCORE_MEMSEGFIFO_SIZE);

	    cachedTable->freeCachedLevel0Block(evictedBlock);
	    evictedSegment->sched_cachedSegmentBlock = NULL;

	    cachedBlock = cachedTable->allocateLevel1Block(addedPage);
	      
	    break;
	  }
	}
      }

      // FIX ME! THERE MIGHT BE A SITUATION WHERE THERE JUST ISN'T ENOUGH
      // CMBS TO CACHE ALL CPS!
      if (cachedBlock == NULL) {
	cerr << "FIX ME! NOT ENOUGH PHYSICAL CMBS TO CACHE ALL RESIDENT CPS!"
	     << endl;
	exit(1);
      }

      addedPage->sched_cachedSegmentBlock = cachedBlock;

      cachedTable->addToLoadBlockList(cachedBlock, 0,
        addedPage->bitstream(),
        SCORE_PAGECONFIG_SIZE);
      cachedTable->addToLoadBlockList(cachedBlock, 
        SCORE_PAGECONFIG_SIZE,
        addedPage->bitstream(),
        SCORE_PAGESTATE_SIZE);
      if (addedPage->sched_isFIFOBufferValid) {
	cachedTable->addToLoadBlockList(cachedBlock,
          (SCORE_PAGECONFIG_SIZE+SCORE_PAGESTATE_SIZE),
          addedPage->sched_fifoBuffer,
          SCORE_PAGEFIFO_SIZE);
      }
    }
  }

#if PROFILE_PERFORMPLACEMENT
  endTime = threadCounter->ScoreThreadSchCounterRead();
  cerr << "****** PERFORMPLACMENT1: " << endTime-startTime << endl;

  startTime = threadCounter->ScoreThreadSchCounterRead();
#endif

  //////////////////////////////////////////////////////////////////////////
  // - FOR ALL FAULTED MEMORY SEGMENTS, LOAD THE NEW SECTION OF DATA INTO
  //     THE SEGMENT BLOCK. IF THE SEGMENT IS SEQ/RAM*SINK THEN FIRST
  //     ARRANGE FOR THE CURRENT CONTENTS TO BE DUMPED BACK TO MAIN MEMORY.
  //////////////////////////////////////////////////////////////////////////

  for (i = 0; i < SCORECUSTOMLIST_LENGTH(faultedMemSegList); i++) {
    ScoreSegment *faultedSegment;
    ScoreSegmentBlock *cachedBlock;
    ScoreSegmentTable *cachedTable;
    unsigned int newBlockStart, newBlockEnd, newBlockLength;
    unsigned int oldTRA;

    SCORECUSTOMLIST_ITEMAT(faultedMemSegList, i, faultedSegment);
    cachedBlock = faultedSegment->sched_cachedSegmentBlock;
    cachedTable = cachedBlock->parentTable;
    oldTRA = faultedSegment->sched_traAddr;

    // FIX ME! SHOULD REALLY ADVANCE DATA BUFFER POINTER ACCORDING TO
    // ACTUAL DATA WIDTH!
    if ((faultedSegment->sched_mode != SCORE_CMB_SEQSRC) &&
	(faultedSegment->sched_mode != SCORE_CMB_RAMSRC)) {
      cachedTable->addToDumpBlockList(cachedBlock, 0,
        (((unsigned long long *) faultedSegment->data())+
	 faultedSegment->sched_residentStart),
	faultedSegment->sched_residentLength*(SCORE_ALIGN_BITWIDTH_TO_8(faultedSegment->width())/8));
    }

    // figure out what the new TRA and block size will be.
    newBlockStart =
      (faultedSegment->sched_faultedAddr/
       (SCORE_DATASEGMENTBLOCK_LOADSIZE/(SCORE_ALIGN_BITWIDTH_TO_8(faultedSegment->width())/8)))*
      (SCORE_DATASEGMENTBLOCK_LOADSIZE/(SCORE_ALIGN_BITWIDTH_TO_8(faultedSegment->width())/8));
    newBlockEnd = newBlockStart+
      (SCORE_DATASEGMENTBLOCK_LOADSIZE/(SCORE_ALIGN_BITWIDTH_TO_8(faultedSegment->width())/8));
    if (newBlockEnd > ((unsigned int) faultedSegment->length())) {
      newBlockEnd = faultedSegment->length();
    }
    newBlockLength = newBlockEnd-newBlockStart;
	  
    // store the new TRA and MAX.
    faultedSegment->sched_traAddr = newBlockStart;
    faultedSegment->sched_maxAddr = newBlockLength;
    
    faultedSegment->sched_residentStart = newBlockStart;
    faultedSegment->sched_residentLength = newBlockLength;

    // FIX ME! SHOULD REALLY ADVANCE DATA BUFFER POINTER ACCORDING TO
    // ACTUAL DATA WIDTH!
    cachedTable->addToLoadBlockList(cachedBlock, 0,
      (((unsigned long long *) faultedSegment->data())+
       faultedSegment->sched_residentStart),
      faultedSegment->sched_residentLength*(SCORE_ALIGN_BITWIDTH_TO_8(faultedSegment->width())/8));

    if (VERBOSEDEBUG || DEBUG) {
      cerr << "SCHED: Changing TRA from " << oldTRA <<
	" to " << faultedSegment->sched_traAddr << endl;
    }
  }

#if DOSCHEDULE_FOR_SCHEDTIME
  endClock = threadCounter->ScoreThreadSchCounterRead();
  diffClock = endClock - startClock;
  current_performPlacement = diffClock;
#endif

#if PROFILE_PERFORMPLACEMENT
  endTime = threadCounter->ScoreThreadSchCounterRead();
  cerr << "****** PERFORMPLACMENT0: " << endTime-startTime << endl;
#endif

#if DOPROFILING
  endClock = threadCounter->ScoreThreadSchCounterRead();
  diffClock = endClock - startClock;
  if (diffClock < min_performPlacement) {
    min_performPlacement = diffClock;
  }
  if (diffClock > max_performPlacement) {
    max_performPlacement = diffClock;
  }
  total_performPlacement = total_performPlacement + diffClock;
  current_performPlacement = diffClock;
  cerr << "   performPlacement() ==> " << 
    diffClock <<
    " cycle(s)" << endl;
#endif
}


///////////////////////////////////////////////////////////////////////////////
// ScoreSchedulerStatic::issueReconfigCommands:
//   Issue the reconfiguration commands to the array in order to load/dump
//     the correct pages/memory segments.
//
// Parameters: none.
//
// Return value: none.
///////////////////////////////////////////////////////////////////////////////
void ScoreSchedulerStatic::issueReconfigCommands() {
  unsigned int i;
  list<ScorePage *> dumpPageState_todo;
  list<ScorePage *> dumpPageFIFO_todo;
  list<ScoreSegment *> dumpSegmentFIFO_todo;
  list<ScorePage *> loadPageConfig_todo;
  list<ScorePage *> loadPageState_todo;
  list<ScorePage *> loadPageFIFO_todo;
  list<ScoreSegment *> loadSegmentFIFO_todo;


#if 0 && DOPROFILING
  startClock = threadCounter->ScoreThreadSchCounterRead();
#endif

  // if there is nothing to do, then just return.
  if ((SCORECUSTOMLIST_LENGTH(scheduledPageList) == 0) &&
      (SCORECUSTOMLIST_LENGTH(scheduledMemSegList) == 0) &&
      (SCORECUSTOMLIST_LENGTH(removedPageList) == 0) &&
      (SCORECUSTOMLIST_LENGTH(removedMemSegList) == 0) &&
      (SCORECUSTOMLIST_LENGTH(configChangedStitchSegList) == 0) &&
      (SCORECUSTOMLIST_LENGTH(faultedMemSegList) == 0)) {
    return;
  }

  // stop every page and CMB.
  batchCommandBegin();
  for (i = 0; i < numPhysicalCP; i++) {
    if (arrayCP[i].active != NULL) {
      stopPage(arrayCP[i].active);
#if KEEPRECONFIGSTATISTICS
      total_stopPage++;
#endif
    }
  }
  for (i = 0; i < numPhysicalCMB; i++) {
    if (arrayCMB[i].active != NULL) {
      stopSegment(arrayCMB[i].active);
#if KEEPRECONFIGSTATISTICS
      total_stopSegment++;
#endif
    }
  }
  batchCommandEnd();

#if GET_FEEDBACK
  if (gFeedbackMode == MAKEFEEDBACK)
    makeFeedback();
#endif

  // Don't know how expensive this really would be. We may be able to
  // do a more limited status check. This is the only way to get accurate
  // stats. We don't really need all of this info!
  // Currently, just used to determine if a stitch buffer is actually
  // empty with no input FIFO data. 
  // FIX ME!
  getCurrentStatus();

  // if segments are being removed, then get the values of their pointers.
  // NOTE: The reason this has been moved so far up is that this is also
  //       where we determine if a stitch buffer is empty and can be removed.
  //       If it can, then we can avoid some array commands.
  for (i = 0; i < numPhysicalCMB; i++) {
    if ((arrayCMB[i].active != arrayCMB[i].scheduled) &&
	(arrayCMB[i].active != NULL)) {
      if (!(arrayCMB[i].active->sched_isDone) ||
	  arrayCMB[i].active->sched_dumpOnDone) {
	batchCommandBegin();
	getSegmentPointers(arrayCMB[i].active);
#if KEEPRECONFIGSTATISTICS
	total_getSegmentPointers++;
#endif
	batchCommandEnd();
      }

      if (!(arrayCMB[i].active->sched_isDone)) {
	if (arrayCMB[i].active->sched_isStitch) {
	  ScoreSegmentStitch *currentStitch = 
	    (ScoreSegmentStitch *) arrayCMB[i].active;

	  if (!(currentStitch->sched_mustBeInDataFlow)) {
	    if (cmbStatus[i].readAddr == cmbStatus[i].writeAddr) {
              // we need to make sure that there are no input FIFO tokens
              // and that this stitch buffer did not signal done but not
              // get caught before.
              if (!(cmbStatus[i].isDone) && 
                  (cmbStatus[i].inputFIFONumTokens[
                     SCORE_CMB_STITCH_DATAW_INNUM] == 0)) {
	        SCORECUSTOMLIST_APPEND(emptyStitchList, currentStitch);
	        currentStitch->sched_isEmptyAndWillBeRemoved = 1;
              }
	    }
	  }
	}
      }
    }
  }

  // FIX ME! DO THIS BETTER WHEN WE HAVE DETERMINED THIS VERSION WORKS.
  // WE ESSENTIALLY JUST STOP ALL PAGES, DUMP ALL OLD PAGES, LOAD IN NEW
  // PAGES, AND RUN.

  // FIX ME! SHOULD MAKE SURE THAT ANY INPUTS/OUTPUTS THAT ARE NOT CONNECTED
  // ANYWHERE ARE PROPERLY TERMINATED (i.e. always assert empty or never
  // assert full!).

  // fill in the todo lists.
  for (i = 0; i < SCORECUSTOMLIST_LENGTH(scheduledPageList); i++) {
    ScorePage *currentPage;

    SCORECUSTOMLIST_ITEMAT(scheduledPageList, i, currentPage);

    loadPageConfig_todo.append(currentPage);
    loadPageState_todo.append(currentPage);
    loadPageFIFO_todo.append(currentPage);
  }
  loadPageConfig_todo.append(NULL);
  loadPageState_todo.append(NULL);
  loadPageFIFO_todo.append(NULL);

  for (i = 0; i < SCORECUSTOMLIST_LENGTH(scheduledMemSegList); i++) {
    ScoreSegment *currentSegment;

    SCORECUSTOMLIST_ITEMAT(scheduledMemSegList, i, currentSegment);

    loadSegmentFIFO_todo.append(currentSegment);
  }
  loadSegmentFIFO_todo.append(NULL);

  for (i = 0; i < SCORECUSTOMLIST_LENGTH(removedPageList); i++) {
    ScorePage *currentPage;

    SCORECUSTOMLIST_ITEMAT(removedPageList, i, currentPage);

    if (!(currentPage->sched_isDone)) {
      dumpPageState_todo.append(currentPage);
      dumpPageFIFO_todo.append(currentPage);
    }
  }
  dumpPageState_todo.append(NULL);
  dumpPageFIFO_todo.append(NULL);

  for (i = 0; i < SCORECUSTOMLIST_LENGTH(removedMemSegList); i++) {
    ScoreSegment *currentSegment;

    SCORECUSTOMLIST_ITEMAT(removedMemSegList, i, currentSegment);

    if (!(currentSegment->sched_isDone)) {
      if (!(currentSegment->sched_isStitch &&
	    ((ScoreSegmentStitch *) 
	     currentSegment)->sched_isEmptyAndWillBeRemoved)) {
	dumpSegmentFIFO_todo.append(currentSegment);
      }
    }
  }
  dumpSegmentFIFO_todo.append(NULL);

  // NOTE: WE TRY TO PARALLEL DUMP AS MANY OF THEM AS POSSIBLE FROM CMBs!
  while ((dumpPageState_todo.length() != 1) ||
	 (dumpPageFIFO_todo.length() != 1) ||
	 (dumpSegmentFIFO_todo.length() != 1)) {
    list<unsigned int> busyCPs, busyCMBs;
    ScorePage *currentPage = NULL;
    ScoreSegment *currentSegment = NULL;

    batchCommandBegin();

    currentPage = dumpPageState_todo.pop();
    while (currentPage != NULL) {
      unsigned int currentLoc = currentPage->sched_residentLoc;
      unsigned int cachedLoc = 
	currentPage->sched_dumpSegmentBlock->parentTable->loc;

      if (!(arrayCPBusy[currentLoc]) &&
	  !(arrayCMBBusy[cachedLoc])) {
	arrayCPBusy[currentLoc] = 1;
	arrayCMBBusy[cachedLoc] = 1;
	busyCPs.append(currentLoc);
	busyCMBs.append(cachedLoc);
	    
	dumpPageState(currentPage);
#if KEEPRECONFIGSTATISTICS
	total_dumpPageState++;
#endif
      } else {
	dumpPageState_todo.append(currentPage);
      }
      currentPage = dumpPageState_todo.pop();
    }
    dumpPageState_todo.append(NULL);

    currentPage = dumpPageFIFO_todo.pop();
    while (currentPage != NULL) {
      unsigned int currentLoc = currentPage->sched_residentLoc;
      unsigned int cachedLoc = 
	currentPage->sched_dumpSegmentBlock->parentTable->loc;
	    
      if (!(arrayCPBusy[currentLoc]) &&
	  !(arrayCMBBusy[cachedLoc])) {
	arrayCPBusy[currentLoc] = 1;
	arrayCMBBusy[cachedLoc] = 1;
	busyCPs.append(currentLoc);
	busyCMBs.append(cachedLoc);
	      
	dumpPageFIFO(currentPage);
#if KEEPRECONFIGSTATISTICS
	total_dumpPageFIFO++;
#endif
      } else {
	dumpPageFIFO_todo.append(currentPage);
      }
      currentPage = dumpPageFIFO_todo.pop();
    }
    dumpPageFIFO_todo.append(NULL);

    currentSegment = dumpSegmentFIFO_todo.pop();
    while (currentSegment != NULL) {
      unsigned int currentLoc = currentSegment->sched_residentLoc;
      unsigned int cachedLoc = 
	currentSegment->sched_dumpSegmentBlock->parentTable->loc;

      if (!(arrayCMBBusy[currentLoc]) &&
	  !(arrayCMBBusy[cachedLoc])) {
	arrayCMBBusy[currentLoc] = 1;
	arrayCMBBusy[cachedLoc] = 1;
	busyCMBs.append(currentLoc);
	busyCMBs.append(cachedLoc);
	    
	dumpSegmentFIFO(currentSegment);
#if KEEPRECONFIGSTATISTICS
	total_dumpSegmentFIFO++;
#endif
      } else {
	dumpSegmentFIFO_todo.append(currentSegment);
      }
      currentSegment = dumpSegmentFIFO_todo.pop();
    }
    dumpSegmentFIFO_todo.append(NULL);

    batchCommandEnd();

    // clear the busy flags.
    {
      unsigned int busyLoc;

      while (busyCPs.length() != 0) {
	busyLoc = busyCPs.pop();

	arrayCPBusy[busyLoc] = 0;
      }
      while (busyCMBs.length() != 0) {
	busyLoc = busyCMBs.pop();

	arrayCMBBusy[busyLoc] = 0;
      }
    }
  }

  // clear the CPs and CMBs that are changing.
  for (i = 0; i < numPhysicalCP; i++) {
    if ((arrayCP[i].active != arrayCP[i].scheduled) &&
	(arrayCP[i].active != NULL)) {
      arrayCP[i].active->sched_isResident = 0;
      arrayCP[i].active->sched_residentLoc = 0;
      arrayCP[i].active = NULL;
    }
  }
  for (i = 0; i < numPhysicalCMB; i++) {
    if ((arrayCMB[i].active != arrayCMB[i].scheduled) &&
	(arrayCMB[i].active != NULL)) {
      arrayCMB[i].active->sched_isResident = 0;
      arrayCMB[i].active->sched_residentLoc = 0;
      arrayCMB[i].active = NULL;
    }
  }

  // dump/load any blocks that need to be dumped/load back to/from 
  // primary memory.
  for (i = 0; i < numPhysicalCMB; i++) {
    unsigned int j;
    ScoreSegmentLoadDumpBlock *dumpBlocks =
      arrayCMB[i].segmentTable->dumpBlocks;
    unsigned int dumpBlocks_count =
      arrayCMB[i].segmentTable->dumpBlocks_count;
    ScoreSegmentLoadDumpBlock *loadBlocks =
      arrayCMB[i].segmentTable->loadBlocks;
    unsigned int loadBlocks_count =
      arrayCMB[i].segmentTable->loadBlocks_count;

    for (j = 0; j < dumpBlocks_count; j++) {
      ScoreSegmentLoadDumpBlock *dumpBlock = &(dumpBlocks[j]);

      if (!(dumpBlock->owner->isSegment() &&
	    ((ScoreSegment *) dumpBlock->owner)->sched_isStitch &&
	    ((ScoreSegmentStitch *) 
	     dumpBlock->owner)->sched_isEmptyAndWillBeRemoved)) {
	if (!(dumpBlock->bufferSizeDependsOnOwnerAddrs)) {
	  batchCommandBegin();
	  memXferCMBToPrimary(i,
			      dumpBlock->blockStart,
			      dumpBlock->bufferSize,
			      dumpBlock->buffer);
#if KEEPRECONFIGSTATISTICS
	  total_memXferCMBToPrimary++;
#endif
	  batchCommandEnd();
	} else {
	  // CURRENTLY THE ONLY SEGMENTS THAT HAVE THEIR DATA DUMPED/LOADED
	  // VIA bufferSizeDependsOnOwnerAddrs ARE STITCH BUFFERS!
	  ScoreSegmentStitch *dumpStitch = 
	    (ScoreSegmentStitch *) dumpBlock->owner;
	  unsigned int dumpStitchReadAddr = dumpStitch->sched_readAddr;
	  unsigned int dumpStitchWriteAddr = dumpStitch->sched_writeAddr;

#if VERBOSEDEBUG || DEBUG
	  unsigned int originalSize =
	    dumpStitch->length() * 
            (SCORE_ALIGN_BITWIDTH_TO_8(dumpStitch->width())/8);
	  unsigned int newSize = 0;
#endif

	  // ignore if stitch buffer is empty!
	  if (dumpStitchReadAddr != dumpStitchWriteAddr) {
	    if (dumpStitchReadAddr < dumpStitchWriteAddr) {
	      // the valid block is 1 contiguous section!
	      unsigned int dumpStitchSize = 
		(dumpStitchWriteAddr - dumpStitchReadAddr)*
		(SCORE_ALIGN_BITWIDTH_TO_8(dumpStitch->width())/8);
	      unsigned int dumpStitchStart =
		dumpBlock->blockStart +
		(dumpStitchReadAddr*(SCORE_ALIGN_BITWIDTH_TO_8(dumpStitch->width())/8));
	      void *dumpStitchBuffer =
		(void *) (((unsigned long long *) dumpBlock->buffer) +
			  dumpStitchReadAddr);

	      batchCommandBegin();
	      memXferCMBToPrimary(i,
				  dumpStitchStart,
				  dumpStitchSize,
				  dumpStitchBuffer);
#if KEEPRECONFIGSTATISTICS
	      total_memXferCMBToPrimary++;
#endif
	      batchCommandEnd();
#if VERBOSEDEBUG
	      newSize = newSize + dumpStitchSize;
#endif
	    } else {
	      // the valid block is actually 2 contiguous sections!
	      unsigned int dumpStitchSizeLower = 
		(dumpStitchWriteAddr - 0)*
		(SCORE_ALIGN_BITWIDTH_TO_8(dumpStitch->width())/8);
	      unsigned int dumpStitchStartLower =
		dumpBlock->blockStart +
		(0*(SCORE_ALIGN_BITWIDTH_TO_8(dumpStitch->width())/8));
	      void *dumpStitchBufferLower =
		(void *) (((unsigned long long *) dumpBlock->buffer) +
			  0);
	      unsigned int dumpStitchSizeUpper = 
		(dumpStitch->length() - dumpStitchReadAddr)*
		(SCORE_ALIGN_BITWIDTH_TO_8(dumpStitch->width())/8);
	      unsigned int dumpStitchStartUpper =
		dumpBlock->blockStart +
		(dumpStitchReadAddr*(SCORE_ALIGN_BITWIDTH_TO_8(dumpStitch->width())/8));
	      void *dumpStitchBufferUpper =
		(void *) (((unsigned long long *) dumpBlock->buffer) +
			  dumpStitchReadAddr);

	      batchCommandBegin();
	      memXferCMBToPrimary(i,
				  dumpStitchStartLower,
				  dumpStitchSizeLower,
				  dumpStitchBufferLower);
#if KEEPRECONFIGSTATISTICS
	      total_memXferCMBToPrimary++;
#endif
	      batchCommandEnd();

	      batchCommandBegin();
	      memXferCMBToPrimary(i,
				  dumpStitchStartUpper,
				  dumpStitchSizeUpper,
				  dumpStitchBufferUpper);
#if KEEPRECONFIGSTATISTICS
	      total_memXferCMBToPrimary++;
#endif
	      batchCommandEnd();
#ifndef VERBOSEDEBUG
	      newSize = newSize + dumpStitchSizeLower + dumpStitchSizeUpper;
#endif
	    }
	  }

#ifndef VERBOSEDEBUG
	  cerr << "SCHED: =====> YOU JUST SAVED DUMPING AN EXTRA: " << 
            (originalSize-newSize) << endl;
#endif
	}
      }
    }

    for (j = 0; j < loadBlocks_count; j++) {
      ScoreSegmentLoadDumpBlock *loadBlock = &(loadBlocks[j]);

      if (!(loadBlock->bufferSizeDependsOnOwnerAddrs)) {
	batchCommandBegin();
	memXferPrimaryToCMB(loadBlock->buffer,
			    i,
			    loadBlock->blockStart,
			    loadBlock->bufferSize);
#if KEEPRECONFIGSTATISTICS
	total_memXferPrimaryToCMB++;
#endif
	batchCommandEnd();
      } else {
	// CURRENTLY THE ONLY SEGMENTS THAT HAVE THEIR DATA DUMPED/LOADED
	// VIA bufferSizeDependsOnOwnerAddrs ARE STITCH BUFFERS!
	ScoreSegmentStitch *loadStitch = 
	  (ScoreSegmentStitch *) loadBlock->owner;
	unsigned int loadStitchReadAddr = loadStitch->sched_readAddr;
	unsigned int loadStitchWriteAddr = loadStitch->sched_writeAddr;
	
#if VERBOSEDEBUG
	  unsigned int originalSize =
	    loadStitch->length() * 
            (SCORE_ALIGN_BITWIDTH_TO_8(loadStitch->width())/8);
	  unsigned int newSize = 0;
#endif

	// ignore if stitch buffer is empty!
	if (loadStitchReadAddr != loadStitchWriteAddr) {
	  if (loadStitchReadAddr < loadStitchWriteAddr) {
	    // the valid block is 1 contiguous section!
	    unsigned int loadStitchSize = 
	      (loadStitchWriteAddr - loadStitchReadAddr)*
	      (SCORE_ALIGN_BITWIDTH_TO_8(loadStitch->width())/8);
	    unsigned int loadStitchStart =
	      loadBlock->blockStart +
	      (loadStitchReadAddr*(SCORE_ALIGN_BITWIDTH_TO_8(loadStitch->width())/8));
	    void *loadStitchBuffer =
	      (void *) (((unsigned long long *) loadBlock->buffer) +
			loadStitchReadAddr);

	    batchCommandBegin();
	    memXferPrimaryToCMB(loadStitchBuffer,
				i,
				loadStitchStart,
				loadStitchSize);
#if KEEPRECONFIGSTATISTICS
	    total_memXferPrimaryToCMB++;
#endif
	    batchCommandEnd();
#if VERBOSEDEBUG
	    newSize = newSize + loadStitchSize;
#endif
	  } else {
	    // the valid block is actually 2 contiguous sections!
	    unsigned int loadStitchSizeLower = 
	      (loadStitchWriteAddr - 0)*
	      (SCORE_ALIGN_BITWIDTH_TO_8(loadStitch->width())/8);
	    unsigned int loadStitchStartLower =
	      loadBlock->blockStart +
	      (0*(SCORE_ALIGN_BITWIDTH_TO_8(loadStitch->width())/8));
	    void *loadStitchBufferLower =
	      (void *) (((unsigned long long *) loadBlock->buffer) +
			0);
	    unsigned int loadStitchSizeUpper = 
	      (loadStitch->length() - loadStitchReadAddr)*
	      (SCORE_ALIGN_BITWIDTH_TO_8(loadStitch->width())/8);
	    unsigned int loadStitchStartUpper =
	      loadBlock->blockStart +
	      (loadStitchReadAddr*(SCORE_ALIGN_BITWIDTH_TO_8(loadStitch->width())/8));
	    void *loadStitchBufferUpper =
	      (void *) (((unsigned long long *) loadBlock->buffer) +
			loadStitchReadAddr);

	    batchCommandBegin();
	    memXferPrimaryToCMB(loadStitchBufferLower,
				i,
				loadStitchStartLower,
				loadStitchSizeLower);
#if KEEPRECONFIGSTATISTICS
	    total_memXferPrimaryToCMB++;
#endif
	    batchCommandEnd();

	    batchCommandBegin();
	    memXferPrimaryToCMB(loadStitchBufferUpper,
				i,
				loadStitchStartUpper,
				loadStitchSizeUpper);
#if KEEPRECONFIGSTATISTICS
	    total_memXferPrimaryToCMB++;
#endif
	    batchCommandEnd();
#if VERBOSEDEBUG
	    newSize = newSize + loadStitchSizeLower + loadStitchSizeUpper;
#endif
	  }
	}

#if VERBOSEDEBUG
	  cerr << "SCHED: =====> YOU JUST SAVED LOADING AN EXTRA: " << 
            (originalSize-newSize) << endl;
#endif
      }
    }

    arrayCMB[i].segmentTable->dumpBlocks_count = 0;
    arrayCMB[i].segmentTable->loadBlocks_count = 0;
  }

  // take care of any faulted CMBs by updating the TRA and PBO and MAX.
  for (i = 0; i < SCORECUSTOMLIST_LENGTH(faultedMemSegList); i++) {
    ScoreSegment *currentSegment;

    SCORECUSTOMLIST_ITEMAT(faultedMemSegList, i, currentSegment);

    // change the TRA and PBO and MAX for the segment.
    batchCommandBegin();
    changeSegmentTRAandPBOandMAX(currentSegment);
#if KEEPRECONFIGSTATISTICS
    total_changeSegmentTRAandPBOandMAX++;
#endif
    batchCommandEnd();
  }

  // go through to each physical page and CMB, and, if a new page or
  // CMB is to be loaded, load it.
  for (i = 0; i < numPhysicalCMB; i++) {
    if ((arrayCMB[i].active != arrayCMB[i].scheduled) &&
        (arrayCMB[i].scheduled != NULL)) {
      arrayCMB[i].active = arrayCMB[i].scheduled;
      arrayCMB[i].active->sched_isResident = 1;
      arrayCMB[i].active->sched_residentLoc = i;
      arrayCMB[i].actual = arrayCMB[i].active;

      // load the config/pointers.
      batchCommandBegin();
      setSegmentConfigPointers(arrayCMB[i].scheduled);
#if KEEPRECONFIGSTATISTICS
      total_setSegmentConfigPointers++;
#endif
      batchCommandEnd();
    }
  }
  // FIX ME! IS IT OKAY TO LOAD IN THE FIFO AND STATE BEFORE THE CONFIG??
  while ((loadPageConfig_todo.length() != 1) ||
	 (loadPageState_todo.length() != 1) ||
	 (loadPageFIFO_todo.length() != 1) ||
	 (loadSegmentFIFO_todo.length() != 1)) {
    list<unsigned int> busyCPs, busyCMBs;
    ScorePage *currentPage = NULL;
    ScoreSegment *currentSegment = NULL;

    batchCommandBegin();

    currentPage = loadPageConfig_todo.pop();
    while (currentPage != NULL) {
      unsigned int currentLoc = currentPage->sched_residentLoc;
      unsigned int cachedLoc = 
	currentPage->sched_cachedSegmentBlock->parentTable->loc;

      if (!(arrayCPBusy[currentLoc]) &&
	  !(arrayCMBBusy[cachedLoc])) {
	arrayCP[currentLoc].active = arrayCP[currentLoc].scheduled;
	arrayCP[currentLoc].active->sched_isResident = 1;
	arrayCP[currentLoc].actual = arrayCP[currentLoc].active;
	
	arrayCPBusy[currentLoc] = 1;
	arrayCMBBusy[cachedLoc] = 1;
	busyCPs.append(currentLoc);
	busyCMBs.append(cachedLoc);

	loadPageConfig(currentPage);
#if KEEPRECONFIGSTATISTICS
	total_loadPageConfig++;
#endif
      } else {
	loadPageConfig_todo.append(currentPage);
      }
      currentPage = loadPageConfig_todo.pop();
    }
    loadPageConfig_todo.append(NULL);

    currentPage = loadPageState_todo.pop();
    while (currentPage != NULL) {
      unsigned int currentLoc = currentPage->sched_residentLoc;
      unsigned int cachedLoc = 
	currentPage->sched_cachedSegmentBlock->parentTable->loc;

      if (!(arrayCPBusy[currentLoc]) &&
	  !(arrayCMBBusy[cachedLoc])) {
	arrayCPBusy[currentLoc] = 1;
	arrayCMBBusy[cachedLoc] = 1;
	busyCPs.append(currentLoc);
	busyCMBs.append(cachedLoc);

	loadPageState(currentPage);
#if KEEPRECONFIGSTATISTICS
	total_loadPageState++;
#endif
      } else {
	loadPageState_todo.append(currentPage);
      }
      currentPage = loadPageState_todo.pop();
    }
    loadPageState_todo.append(NULL);

    currentPage = loadPageFIFO_todo.pop();
    while (currentPage != NULL) {
      unsigned int currentLoc = currentPage->sched_residentLoc;

      // if FIFO data exists, load it. otherwise, initialize the FIFOs.
      if (currentPage->sched_isFIFOBufferValid) {
	unsigned int cachedLoc = 
	  currentPage->sched_cachedSegmentBlock->parentTable->loc;

	if (!(arrayCPBusy[currentLoc]) &&
	    !(arrayCMBBusy[cachedLoc])) {
	  arrayCPBusy[currentLoc] = 1;
	  arrayCMBBusy[cachedLoc] = 1;
	  busyCPs.append(currentLoc);
	  busyCMBs.append(cachedLoc);
	    
	  loadPageFIFO(currentPage);
#if KEEPRECONFIGSTATISTICS
	  total_loadPageFIFO++;
#endif
	} else {
	  loadPageFIFO_todo.append(currentPage);
	}
      } else {
	currentPage->sched_isFIFOBufferValid = 1;
	// FIX ME! DO NOT KNOW WHAT TO DO IN ORDER TO INITIALIZE FIFOS!
      }
      currentPage = loadPageFIFO_todo.pop();
    }
    loadPageFIFO_todo.append(NULL);

    currentSegment = loadSegmentFIFO_todo.pop();
    while (currentSegment != NULL) {
      unsigned int currentLoc = currentSegment->sched_residentLoc;

      // if FIFO data exists, load it. otherwise, initialize the FIFOs.
      if (currentSegment->sched_isFIFOBufferValid) {
	unsigned int cachedLoc = 
	  currentSegment->sched_cachedSegmentBlock->parentTable->loc;

	if (!(arrayCMBBusy[currentLoc]) &&
	    !(arrayCMBBusy[cachedLoc])) {
	  arrayCMBBusy[currentLoc] = 1;
	  arrayCMBBusy[cachedLoc] = 1;
	  busyCMBs.append(currentLoc);
	  busyCMBs.append(cachedLoc);
	    
	  loadSegmentFIFO(currentSegment);
#if KEEPRECONFIGSTATISTICS
	  total_loadSegmentFIFO++;
#endif
	} else {
	  loadSegmentFIFO_todo.append(currentSegment);
	}
      } else {
	currentSegment->sched_isFIFOBufferValid = 1;
	// FIX ME! DO NOT KNOW WHAT TO DO IN ORDER TO INITIALIZE FIFOS!
      }
      currentSegment = loadSegmentFIFO_todo.pop();
    }
    loadSegmentFIFO_todo.append(NULL);

    batchCommandEnd();

    // clear the busy flags.
    {
      unsigned int busyLoc;

      while (busyCPs.length() != 0) {
	busyLoc = busyCPs.pop();

	arrayCPBusy[busyLoc] = 0;
      }
      while (busyCMBs.length() != 0) {
	busyLoc = busyCMBs.pop();

	arrayCMBBusy[busyLoc] = 0;
      }
    }
  }

  for (i = 0; i < SCORECUSTOMLIST_LENGTH(configChangedStitchSegList);
       i++) {
    ScoreSegment *currentSegment;

    SCORECUSTOMLIST_ITEMAT(configChangedStitchSegList, i, currentSegment);

    // change the segment mode.
    batchCommandBegin();
    changeSegmentMode(currentSegment);
#if KEEPRECONFIGSTATISTICS
    total_changeSegmentMode++;
#endif
    batchCommandEnd();

    if ((currentSegment->sched_old_mode == SCORE_CMB_SEQSINK) &&
        (currentSegment->sched_mode == SCORE_CMB_SEQSRCSINK)) {
      batchCommandBegin();
      resetSegmentDoneFlag(currentSegment);
#if KEEPRECONFIGSTATISTICS
      total_resetSegmentDoneFlag++;
#endif
      batchCommandEnd();
    }
  }

  // reconnect the streams.
  // FIX ME! SHOULD FIND A BETTER WAY TO DO THIS! RIGHT NOW, I AM JUST
  // RECONNECTING ALL STREAMS TO ENSURE CORRECTNESS.
  batchCommandBegin();
  for (i = 0; i < numPhysicalCP; i++) {
    ScorePage *currentPage = arrayCP[i].active;

    if ((currentPage != NULL) && !(currentPage->sched_isDone)) {
      unsigned int numOutputs = currentPage->getOutputs();
      unsigned int j;

      for (j = 0; j < numOutputs; j++) {
	SCORE_STREAM outputStream = currentPage->getSchedOutput(j);

	ScoreGraphNode *outputNode = outputStream->sched_sink;
	unsigned int outputNodeInputNumber = outputStream->sched_snkNum;

	if (outputStream->sched_sinkIsDone) {
	  // FIX ME! MUST FIGURE OUT HOW TO INDICATE A STREAM THAT CONNECTS
	  // TO NOWHERE BUT MUST ALWAYS ALLOW STREAM_WRITES.
	  outputStream->syncSchedToReal();
	} else if (outputStream->sched_snkFunc == STREAM_OPERATOR_TYPE) {
	  // FIX ME! MUST FIGURE OUT HOW TO CONNECT ARRAY<->PROCESSOR.
	  outputStream->syncSchedToReal();
	} else {
	  connectStream(currentPage, j, outputNode, outputNodeInputNumber);
#if KEEPRECONFIGSTATISTICS
	  total_connectStream++;
#endif
	}
      }
    }
  }
  for (i = 0; i < numPhysicalCMB; i++) {
    ScoreSegment *currentSegment = arrayCMB[i].active;

    // NOTE: Since stitch buffers might have outputs, but may be in a
    //       mode such that we do not want to connect outputs, then
    //       check for that.
    if ((currentSegment != NULL) &&
	(!(currentSegment->sched_isStitch) ||
	 !(currentSegment->mode == SCORE_CMB_SEQSINK))) {
      unsigned int numOutputs = currentSegment->getOutputs();
      unsigned int j;

      for (j = 0; j < numOutputs; j++) {
	SCORE_STREAM outputStream = currentSegment->getSchedOutput(j);

	ScoreGraphNode *outputNode = outputStream->sched_sink;
	unsigned int outputNodeInputNumber = outputStream->sched_snkNum;
	
	if (outputStream->sched_sinkIsDone) {
	  // FIX ME! MUST FIGURE OUT HOW TO INDICATE A STREAM THAT CONNECTS
	  // TO NOWHERE BUT MUST ALWAYS ALLOW STREAM_WRITES.
	  outputStream->syncSchedToReal();
	} else if (outputStream->sched_snkFunc == STREAM_OPERATOR_TYPE) {
	  // FIX ME! MUST FIGURE OUT HOW TO CONNECT ARRAY<->PROCESSOR.
	  outputStream->syncSchedToReal();
	} else {
	  connectStream(currentSegment, j, outputNode, 
			outputNodeInputNumber);
#if KEEPRECONFIGSTATISTICS
	  total_connectStream++;
#endif
	}
      }
    }
  }
  // FIX ME! MUST FIGURE OUT HOW TO MAKE A DISCONNECT INPUT CONNECTED TO
  // NOTHING!
  for (i = 0; i < SCORECUSTOMLIST_LENGTH(processorIStreamList); i++) {
    ScoreStream *outputStream;
    // ScoreGraphNode *outputNode;
    // unsigned int outputNodeInputNumber;

    SCORECUSTOMLIST_ITEMAT(processorIStreamList, i, outputStream);
    // outputNode = outputStream->sched_sink;
    // outputNodeInputNumber = outputStream->sched_snkNum;

    // FIX ME! SHOULD TAKE CARE OF INPUTS COMING FROM THE PROCESSOR!
    // connectStream(NULL, 0, outputNode, outputNodeInputNumber); ????
    outputStream->syncSchedToReal();
  }
  batchCommandEnd();

  // start every page and CMB.
  // make sure that any done pages left on the array are not started again!
  batchCommandBegin();
  for (i = 0; i < numPhysicalCP; i++) {
    if ((arrayCP[i].active != NULL) &&
	!(arrayCP[i].active->sched_isDone)) {
      startPage(arrayCP[i].active);
#if KEEPRECONFIGSTATISTICS
      total_startPage++;
#endif
    }
  }
  for (i = 0; i < numPhysicalCMB; i++) {
    if (arrayCMB[i].active != NULL) {
      startSegment(arrayCMB[i].active);
#if KEEPRECONFIGSTATISTICS
      total_startSegment++;
#endif
    }
  }
  batchCommandEnd();

  // clean up lists.
  SCORECUSTOMLIST_CLEAR(scheduledPageList);
  SCORECUSTOMLIST_CLEAR(scheduledMemSegList);
  SCORECUSTOMLIST_CLEAR(removedPageList);
  SCORECUSTOMLIST_CLEAR(removedMemSegList);
  SCORECUSTOMLIST_CLEAR(configChangedStitchSegList);
  SCORECUSTOMLIST_CLEAR(faultedMemSegList);

#if 0 && DOPROFILING
  endClock = threadCounter->ScoreThreadSchCounterRead();
  diffClock = endClock - startClock;
  if (diffClock < min_issueReconfigCommands) {
    min_issueReconfigCommands = diffClock;
  }
  if (diffClock > max_issueReconfigCommands) {
    max_issueReconfigCommands = diffClock;
  }
  total_issueReconfigCommands = total_issueReconfigCommands + diffClock;
  cerr << "   issueReconfigCommands() ==> " << 
    diffClock <<
    " cycle(s)" << endl;
#endif
}


///////////////////////////////////////////////////////////////////////////////
// ScoreSchedulerStatic::performCleanup:
//   Perform any cleanup operations.
//
// Parameters: none.
//
// Return value: none.
///////////////////////////////////////////////////////////////////////////////
void ScoreSchedulerStatic::performCleanup() {
  unsigned int i, j;

#if PROFILE_PERFORMCLEANUP
  unsigned long long startTime, endTime;
#endif

#if 0 && DOPROFILING
  startClock = threadCounter->ScoreThreadSchCounterRead();
#endif

  // for each done page/segment, remove it from its operator, and 
  // process. if this node is the last node in the operator, or 
  // process, then remove those as well.
  // in addition, free any memory and return access to the user if necessary.
  for (i = 0; i < SCORECUSTOMLIST_LENGTH(doneNodeList); i++) {
    ScoreGraphNode *currentNode;
    ScoreSegment *currentSegment;
    ScoreOperatorInstance *parentOperator;
    ScoreProcess *parentProcess;
    unsigned int numInputs;
    unsigned int numOutputs;

    SCORECUSTOMLIST_ITEMAT(doneNodeList, i, currentNode);
    currentSegment = (ScoreSegment *) currentNode;
    parentOperator = currentNode->sched_parentOperator;
    parentProcess = currentNode->sched_parentProcess;
    numInputs = (unsigned int) currentNode->getInputs();
    numOutputs = (unsigned int) currentNode->getOutputs();

    if (!(currentNode->isSegment() && currentSegment->sched_isStitch)) {
      // if this is a user segment, return it to the user process.
      if (currentNode->isSegment()) {
	ScoreSegment *currentSegment = (ScoreSegment *) currentNode;

	currentSegment->returnAccess();
      }

      // decrement the page/segment count of the parent operator.
      // also in the parent process (if it is a page).
      if (currentNode->isPage()) {
	ScorePage *currentPage = (ScorePage *) currentNode;

	parentOperator->sched_livePages--;
	parentProcess->numPages--;

	if (currentPage->sched_potentiallyDidNotFireLastResident) {
	  parentProcess->numPotentiallyNonFiringPages--;
	}

	for (j = 0; j < parentOperator->pages; j++) {
	  if (parentOperator->page[j] == currentPage) {
	    parentOperator->page[j] = NULL;
	  }
	}
      } else {
	parentOperator->sched_liveSegments--;

	parentProcess->numSegments--;
	if (currentSegment->sched_potentiallyDidNotFireLastResident) {
	  parentProcess->numPotentiallyNonFiringSegments--;
	}

	for (j = 0; j < parentOperator->segments; j++) {
	  if (parentOperator->segment[j] == currentNode) {
	    parentOperator->segment[j] = NULL;
	  }
	}
      }

      // remove the node from the node list of the parent process.
      SCORECUSTOMLIST_REMOVE(parentProcess->nodeList, currentNode);
      
      // check to see if the input/output streams are in the processor
      // I/O tables. if so, then remove them.
      for (j = 0; j < numInputs; j++) {
	SCORE_STREAM currentStream = currentNode->getSchedInput(j);

	if (currentStream->sched_srcFunc == STREAM_OPERATOR_TYPE) {
	  if (currentStream->sched_isProcessorArrayStream) {
	    SCORECUSTOMLIST_REMOVE(processorIStreamList, currentStream);
	    SCORECUSTOMLIST_REMOVE(parentProcess->processorIStreamList, 
				   currentStream);
	    currentStream->sched_isProcessorArrayStream = 0;
	  }
	}
      }
      for (j = 0; j < numOutputs; j++) {
	SCORE_STREAM currentStream = currentNode->getSchedOutput(j);

	if (currentStream->sched_snkFunc == STREAM_OPERATOR_TYPE) {
	  if (currentStream->sched_isProcessorArrayStream) {
	    SCORECUSTOMLIST_REMOVE(processorOStreamList, currentStream);
	    SCORECUSTOMLIST_REMOVE(parentProcess->processorOStreamList, 
				   currentStream);
	    currentStream->sched_isProcessorArrayStream = 0;
	  }
	}
      }
    
      // delete the node itself.
      // NOTE: We sync the scheduler view to real view just in case it hasn't
      //       had a chance to be resident.
      currentNode->syncSchedToReal();
      if (currentNode->isPage()) {
	delete((ScorePage *) currentNode);
      } else {
	delete((ScoreSegment *) currentNode);
      }
      currentNode = NULL;
      
      // check to see if the operator should be deleted.
      if ((parentOperator->sched_livePages == 0) && 
	  (parentOperator->sched_liveSegments == 0)) {
	void *oldHandle = parentOperator->sched_handle;

	SCORECUSTOMLIST_REMOVE(parentProcess->operatorList, parentOperator);
	delete(parentOperator);
	parentOperator = NULL;
	dlclose(oldHandle);
      }
      
      // check to see if the process should be deleted.
      if ((SCORECUSTOMLIST_LENGTH(parentProcess->operatorList) == 0) &&
	  (SCORECUSTOMLIST_LENGTH(parentProcess->nodeList) == 0) &&
          //	  (SCORECUSTOMLIST_LENGTH(parentProcess->clusterList) == 0) &&
	  (SCORECUSTOMLIST_LENGTH(parentProcess->stitchBufferList) == 0)) {
	SCORECUSTOMLIST_REMOVE(processList, parentProcess);
	
	delete(parentProcess);
	parentProcess = NULL;
      }
    } else {
      ScoreSegmentStitch *currentStitch =
	(ScoreSegmentStitch *) currentSegment;
      SCORE_STREAM inStream = currentStitch->getSchedInStream();
      SCORE_STREAM outStream = currentStitch->getSchedOutStream();

      SCORECUSTOMLIST_REMOVE(stitchBufferList, currentStitch);
      SCORECUSTOMLIST_REMOVE(parentProcess->stitchBufferList, currentStitch);

      parentProcess->numSegments--;
      if (currentStitch->sched_potentiallyDidNotFireLastResident) {
	parentProcess->numPotentiallyNonFiringSegments--;
      }

      // check to see if the input/output streams are in the processor
      // I/O tables. if so, then remove them.
      if (inStream->sched_srcFunc == STREAM_OPERATOR_TYPE) {
	if (inStream->sched_isProcessorArrayStream) {
	  SCORECUSTOMLIST_REMOVE(processorIStreamList, inStream);
	  SCORECUSTOMLIST_REMOVE(parentProcess->processorIStreamList, 
				 inStream);
	  inStream->sched_isProcessorArrayStream = 0;
	}
      }
      if (outStream->sched_snkFunc == STREAM_OPERATOR_TYPE) {
	if (outStream->sched_isProcessorArrayStream) {
	  SCORECUSTOMLIST_REMOVE(processorOStreamList, outStream);
	  SCORECUSTOMLIST_REMOVE(parentProcess->processorOStreamList, 
				 outStream);
	  outStream->sched_isProcessorArrayStream = 0;
	}
      }

      // since we will not actually be deleting the stitch segment, we
      // need to manually call free_hw and close_hw on the I/O.
      STREAM_FREE_HW(inStream);
      STREAM_CLOSE_HW(outStream);

      // return the stitch buffer to the spare stitch segment list.
      currentStitch->reset();
      SCORECUSTOMSTACK_PUSH(spareSegmentStitchList, currentStitch);
      currentStitch = NULL;

      // check to see if the process should be deleted.
      if ((SCORECUSTOMLIST_LENGTH(parentProcess->operatorList) == 0) &&
	  (SCORECUSTOMLIST_LENGTH(parentProcess->nodeList) == 0) &&
          //	  (SCORECUSTOMLIST_LENGTH(parentProcess->clusterList) == 0) &&
	  (SCORECUSTOMLIST_LENGTH(parentProcess->stitchBufferList) == 0)) {
	SCORECUSTOMLIST_REMOVE(processList, parentProcess);
	
	delete(parentProcess);
	parentProcess = NULL;
      }
    }
  }

  // take care of removing any empty stitch buffers that should be
  // removed from the dataflow graph.
  for (i = 0; i < SCORECUSTOMLIST_LENGTH(emptyStitchList); i++) {
    ScoreSegmentStitch *emptyStitch;
    SCORE_STREAM inStream;
    SCORE_STREAM outStream;
    ScoreProcess *parentProcess;
    char isStreamReversed = 0;

    SCORECUSTOMLIST_ITEMAT(emptyStitchList, i, emptyStitch);
    inStream = emptyStitch->getSchedInStream();
    outStream = emptyStitch->getSchedOutStream();
    parentProcess = emptyStitch->sched_parentProcess;

    // depending on which stream (I or O) is the stitch stream, unstitch
    // the stitch buffer from the dataflow graph.
    // also, return the stitch stream to the spare list.
    // NOTE: ACTUALLY FOR SIMULATION, THE STITCH STREAM MUST BE ON THE
    //       INSTREAM TO GUARANTEE WE DON'T ACCIDENTLY REMOVE THE FIFO TO
    //       THE DOWNSTREAM NODE TOO!
    if (inStream->sched_isStitch) {
      emptyStitch->unbindSchedInput(SCORE_CMB_STITCH_DATAW_INNUM);
      emptyStitch->unbindSchedOutput(SCORE_CMB_STITCH_DATAR_OUTNUM);

      if (!(inStream->sched_srcIsDone)) {
	ScoreGraphNode *srcNode = inStream->sched_src;
	ScoreCluster *srcCluster = srcNode->sched_parentCluster;
	int srcNum = inStream->sched_srcNum;
	ScoreStreamType *srcType = srcNode->outputType(srcNum);

	srcNode->unbindSchedOutput(srcNum);
	srcNode->bindSchedOutput(srcNum,
				 outStream,
				 srcType);

	SCORECUSTOMLIST_REPLACE(srcCluster->outputList, inStream, outStream);
      }

      outStream->producerClosed = inStream->producerClosed;
      outStream->producerClosed_hw = inStream->producerClosed_hw;
      outStream->sched_srcIsDone = inStream->sched_srcIsDone;

      ((ScoreStreamStitch *) inStream)->reset();
      SCORECUSTOMSTACK_PUSH(spareStreamStitchList, inStream);
    } else {
#if 0
      cerr << "SCHEDERR: WHOOPS! FOR MOST STITCH BUFFERS, THE STITCH STREAM " <<
        "SHOULD BE ON THE INPUT! NOT OUTPUT! " << 
        (unsigned int) emptyStitch << endl;
      cerr << "SCHEDERR: CANNOT EMPTY STITCH BUFFERS WITH STITCH ON OUTPUT! " <<
        "(ACTUALLY JUST FOR SIMULATION!)" << endl;
      exit(1);
#else
      // for now, there is always the possibility of this happening through
      // C++ composition. so, just ignore such stitch!
      isStreamReversed = 1;

      // also mark the it "must be in dataflow" since there is not chance
      // for it to have its streams reversed again!
      emptyStitch->sched_mustBeInDataFlow = 1;
#endif
    }

    if (!isStreamReversed) {
      // if the stitch buffer still has a cached block on the array, then
      // deallocate it.
      if (emptyStitch->sched_cachedSegmentBlock != NULL) {
	ScoreSegmentBlock *cachedBlock =
	  emptyStitch->sched_cachedSegmentBlock;
	ScoreSegmentTable *cachedTable = cachedBlock->parentTable;

	cachedTable->freeCachedLevel0Block(cachedBlock);
	emptyStitch->sched_cachedSegmentBlock = NULL;
      }

      SCORECUSTOMLIST_REMOVE(stitchBufferList, emptyStitch);
      SCORECUSTOMLIST_REMOVE(parentProcess->stitchBufferList, emptyStitch);

      parentProcess->numSegments--;
      if (emptyStitch->sched_potentiallyDidNotFireLastResident) {
	parentProcess->numPotentiallyNonFiringSegments--;
      }

      // we don't have to check processor I/O tables since we know that
      // we will not remove empty stitch buffers that are at processor I/O
      // boundaries! (at least not yet! -mmchu 03/15/00).

      // return the stitch buffer to the spare stitch segment list.
      emptyStitch->reset();
      SCORECUSTOMSTACK_PUSH(spareSegmentStitchList, emptyStitch);
      emptyStitch = NULL;
    }
  }
  
  SCORECUSTOMLIST_CLEAR(doneNodeList);
  //  SCORECUSTOMLIST_CLEAR(doneClusterList);
  SCORECUSTOMLIST_CLEAR(emptyStitchList);

#if 0 && DOPROFILING
  endClock = threadCounter->ScoreThreadSchCounterRead();
  diffClock = endClock - startClock;
  if (diffClock < min_performCleanup) {
    min_performCleanup = diffClock;
  }
  if (diffClock > max_performCleanup) {
    max_performCleanup = diffClock;
  }
  total_performCleanup = total_performCleanup + diffClock;
  cerr << "   performCleanup() ==> " << 
    diffClock <<
    " cycle(s)" << endl;
#endif
}

///////////////////////////////////////////////////////////////////////////////
// ScoreSchedulerStatic::findPotentiallyDeadLockedProcesses:
//   Finds processes which are potentially dead locked.
//
// Parameters:
//   none.
//
// Return value:
//   none.
///////////////////////////////////////////////////////////////////////////////
void ScoreSchedulerStatic::findPotentiallyDeadLockedProcesses() {
  unsigned int i;


  // for now, we just see if all pages in the process have been non-firing
  // in their last timeslice resident.
  // NOTE: We cannot currently detect deadlock in processes which are 
  //       ScoreSegment-only...
  // NOTE: We are hoping that scheduling is fair and that all pages have a
  //       chance to run on the array! (otherwise, it may take a while to
  //       determine buffer/deadlock!).
  for (i = 0; i < SCORECUSTOMLIST_LENGTH(processList); i++) {
    ScoreProcess *currentProcess;

    SCORECUSTOMLIST_ITEMAT(processList, i, currentProcess);

    if (!((currentProcess->numPages == 0) &&
	  (currentProcess->numSegments == 0))) {
      if ((currentProcess->numPotentiallyNonFiringPages ==
	   currentProcess->numPages) && 
	  (currentProcess->numPotentiallyNonFiringSegments ==
	   currentProcess->numSegments)) {
	SCORECUSTOMLIST_APPEND(deadLockedProcesses, currentProcess);
      } else {
        currentProcess->numConsecutiveDeadLocks = 0;
      }
    } else {
      currentProcess->numConsecutiveDeadLocks = 0;
    }
  }
}


///////////////////////////////////////////////////////////////////////////////
// ScoreSchedulerStatic::findDeadLock:
//   Finds streams which are buffer locked or dead locked given the state 
//     each page is currently in.
//
// Parameters:
//   currentProcess: the process to check for buffer locked streams.
//   bufferLockedStreams: a pointer to the list where buffer locked streams
//                        should be stored.
//   deadLockedCycles: a pointer to the list where lists of dead locked streams
//                     in a cycle will be stored.
//
// Return value:
//   none.
///////////////////////////////////////////////////////////////////////////////
void ScoreSchedulerStatic::findDeadLock(
  ScoreProcess *currentProcess,
  list<ScoreStream *> *bufferLockedStreams,
  list<list<ScoreStream *> *> *deadLockedCycles) {
  unsigned int i, j;
  list_item listItem;
  list<ScoreStream *> traversedStreams;
  list<list<ScoreStream *> *> foundCycles;


  // built a processor node to represent the processor.
  // FIX ME! WE CURRENTLY REPRESENT THE PROCESSOR AS ONE LARGE NODE AND
  //         ASSUME THAT IT IS READING FROM AND WRITING TO ALL OF ITS IO
  //         STREAMS! THIS IS CONSERVATIVE AND INEFFICIENT!
  // FIX ME! WE ARE CURRENTLY "MAGICALLY" GETTING THE FULL/EMPTY STATUS
  //         OF PROCESSOR-ARRAY STREAMS! SHOULD FIGURE IT OUT FROM THE
  //         MEMORY BUFFER! !!! OR PERHAPS, FOR PROCESSOR->ARRAY STREAMS
  //         THEY SHOULD NEVER BE FULL! (AUTOMATICALLY EXPANDED).
  processorNode->setNumIO(SCORECUSTOMLIST_LENGTH(currentProcess->processorOStreamList), 
			  SCORECUSTOMLIST_LENGTH(currentProcess->processorIStreamList));
  for (i = 0; i < SCORECUSTOMLIST_LENGTH(currentProcess->processorOStreamList);
       i++) {
    ScoreStream *currentStream;

    SCORECUSTOMLIST_ITEMAT(currentProcess->processorOStreamList,
			   i, currentStream);

    processorNode->bindSchedInput_localbind(i, currentStream);

    // FIX ME!
    if (STREAM_EMPTY(currentStream)) {
      currentStream->sched_isPotentiallyEmpty = 1;
      currentStream->sched_isPotentiallyFull = 0;
    }
  }
  for (i = 0; i < SCORECUSTOMLIST_LENGTH(currentProcess->processorIStreamList);
       i++) {
    ScoreStream *currentStream;

    SCORECUSTOMLIST_ITEMAT(currentProcess->processorIStreamList, 
			   i, currentStream);

    processorNode->bindSchedOutput_localbind(i, currentStream);

    // FIX ME!
    if (STREAM_FULL(currentStream)) {
      currentStream->sched_isPotentiallyFull = 1;
      currentStream->sched_isPotentiallyEmpty = 0;
    }
  }

  // build up the current dependency graph.
  for (i = 0; i < SCORECUSTOMLIST_LENGTH(currentProcess->operatorList);
       i++) {
    ScoreOperatorInstance *currentOperator;

    SCORECUSTOMLIST_ITEMAT(currentProcess->operatorList, i, currentOperator);

    // check the inputs to see if it is being read in the current state, and
    // if so, if it has any tokens available.
    // check the outputs to see if it is being written in the current state,
    // and if so, if it has any space for tokens available.
    for (i = 0; i < currentOperator->pages; i++) {
      ScorePage *currentPage = currentOperator->page[i];


      // ignore done pages.
      if (currentPage != NULL) {
	if (!(currentPage->sched_isDone)) {
	  unsigned int numInputs = currentPage->getInputs();
	  unsigned int numOutputs = currentPage->getOutputs();
	  int currentState = currentPage->sched_lastKnownState;
	  ScoreIOMaskType currentConsumed = currentPage->inputs_consumed(currentState);
	  ScoreIOMaskType currentProduced = currentPage->outputs_produced(currentState);
	  
	  currentPage->sched_visited = 0;
	  currentPage->sched_visited2 = 0;
	  
	  for (j = 0; j < numInputs; j++) {
	    char isBeingConsumed = (currentConsumed >> j) & 1;
	    
	    currentPage->sched_dependentOnInputBuffer[j] = 0;
	    
	    if (isBeingConsumed) {
	      SCORE_STREAM currentInput = currentPage->getSchedInput(j);
	      char hasInputTokens = !(currentInput->sched_isPotentiallyEmpty);
	      
	      if (!hasInputTokens) {
		currentPage->sched_dependentOnInputBuffer[j] = 1;
	      }
	    }
	  }
	  for (j = 0; j < numOutputs; j++) {
	    char isBeingProduced = (currentProduced >> j) & 1;
	    
	    currentPage->sched_dependentOnOutputBuffer[j] = 0;
	    
	    if (isBeingProduced) {
	      SCORE_STREAM currentOutput = currentPage->getSchedOutput(j);
	      char hasOutputTokenSpace = 
		!(currentOutput->sched_isPotentiallyFull);
	      
	      if (!hasOutputTokenSpace) {
		currentPage->sched_dependentOnOutputBuffer[j] = 1;
	      }
	    }
	  }
	}
      }
    }
    // THIS MIGHT BE SUPERFLOUS TO DO ALL THE TIME. PERHAPS THIS SHOULD BE
    // SET WHEN THE SEGMENT IS INSTANTIATED? (BECAUSE SEGMENTS CAN BE THOUGHT
    // OF AS READING FROM ALL OF ITS INPUTS AND GENERATING ALL OF ITS OUTPUTS
    // EVERYTIME) (NOT ENTIRELY ACCURATE...)
    for (i = 0; i < currentOperator->segments; i++) {
      ScoreSegment *currentSegment = currentOperator->segment[i];

      // ignore done segments.
      if (currentSegment != NULL) {
	if (!(currentSegment->sched_isDone)) {
	  unsigned int numInputs = currentSegment->getInputs();
	  unsigned int numOutputs = currentSegment->getOutputs();
	  
	  currentSegment->sched_visited = 0;
	  currentSegment->sched_visited2 = 0;
	  
	  for (j = 0; j < numInputs; j++) {
	    char isBeingConsumed = 1;
	    
	    currentSegment->sched_dependentOnInputBuffer[j] = 0;
	    
	    if (isBeingConsumed) {
	      SCORE_STREAM currentInput = currentSegment->getSchedInput(j);
	      char hasInputTokens = !(currentInput->sched_isPotentiallyEmpty);
	      
	      if (!hasInputTokens) {
		currentSegment->sched_dependentOnInputBuffer[j] = 1;
	      }
	    }
	  }
	  for (j = 0; j < numOutputs; j++) {
	    char isBeingProduced = 1;
	    
	    currentSegment->sched_dependentOnOutputBuffer[j] = 0;
	    
	    if (isBeingProduced) {
	      SCORE_STREAM currentOutput = currentSegment->getSchedOutput(j);
	      char hasOutputTokenSpace = 
		!(currentOutput->sched_isPotentiallyFull);
	      
	      if (!hasOutputTokenSpace) {
		currentSegment->sched_dependentOnOutputBuffer[j] = 1;
	      }
	    }
	  }
	}
      }
    }
  }
  // THIS MIGHT BE SUPERFLUOUS TO DO ALL THE TIME. PERHAPS THIS SHOULD BE
  // SET WHEN THE SEGMENT IS INSTANTIATED? (BECAUSE SEGMENTS CAN BE THOUGHT
  // OF AS READING FROM ALL OF ITS INPUTS AND GENERATING ALL OF ITS OUTPUTS
  // EVERYTIME) (NOT ENTIRELY ACCURATE...)
  for (i = 0; i < SCORECUSTOMLIST_LENGTH(currentProcess->stitchBufferList);
       i++) {
    ScoreSegment *currentStitch;

    SCORECUSTOMLIST_ITEMAT(currentProcess->stitchBufferList, i, currentStitch);

    // ignore done stitch buffers.
    if (!(currentStitch->sched_isDone)) {
      unsigned int numInputs = currentStitch->getInputs();
      unsigned int numOutputs = currentStitch->getOutputs();

      currentStitch->sched_visited = 0;
      currentStitch->sched_visited2 = 0;
      
      for (j = 0; j < numInputs; j++) {
	char isBeingConsumed = 1;
	
	currentStitch->sched_dependentOnInputBuffer[j] = 0;
	
	if (isBeingConsumed) {
	  SCORE_STREAM currentInput = currentStitch->getSchedInput(j);
	  char hasInputTokens = !(currentInput->sched_isPotentiallyEmpty);
	  
	  if (!hasInputTokens) {
	    currentStitch->sched_dependentOnInputBuffer[j] = 1;
	  }
	}
      }
      for (j = 0; j < numOutputs; j++) {
	char isBeingProduced = 1;
	
	currentStitch->sched_dependentOnOutputBuffer[j] = 0;
	
	if (isBeingProduced) {
	  SCORE_STREAM currentOutput = currentStitch->getSchedOutput(j);
	  char hasOutputTokenSpace = !(currentOutput->sched_isPotentiallyFull);
	  
	  if (!hasOutputTokenSpace) {
	    currentStitch->sched_dependentOnOutputBuffer[j] = 1;
	  }
	}
      }
    }
  }
  {
    unsigned int numInputs = processorNode->getInputs();
    unsigned int numOutputs = processorNode->getOutputs();

    processorNode->sched_visited = 0;
    processorNode->sched_visited2 = 0;

    for (j = 0; j < numInputs; j++) {
      char isBeingConsumed = 1;

      processorNode->sched_dependentOnInputBuffer[j] = 0;

      if (isBeingConsumed) {
        SCORE_STREAM currentInput = processorNode->getSchedInput(j);
        char hasInputTokens = !(currentInput->sched_isPotentiallyEmpty);

        if (!hasInputTokens) {
          processorNode->sched_dependentOnInputBuffer[j] = 1;
        }
      }
    }
    for (j = 0; j < numOutputs; j++) {
      char isBeingProduced = 1;

      processorNode->sched_dependentOnOutputBuffer[j] = 0;

      if (isBeingProduced) {
        SCORE_STREAM currentOutput = processorNode->getSchedOutput(j);
        char hasOutputTokenSpace = !(currentOutput->sched_isPotentiallyFull);

        if (!hasOutputTokenSpace) {
          processorNode->sched_dependentOnOutputBuffer[j] = 1;
        }
      }
    }
  }

  // now, traverse the graph with a DFS and find loops.
  traversedStreams.clear();
  foundCycles.clear();
  deadLockedCycles->clear();
  for (i = 0; i < SCORECUSTOMLIST_LENGTH(currentProcess->operatorList);
       i++) {
    ScoreOperatorInstance *currentOperator;

    SCORECUSTOMLIST_ITEMAT(currentProcess->operatorList, i, currentOperator);

    for (i = 0; i < currentOperator->pages; i++) {
      ScorePage *currentPage = currentOperator->page[i];

      if (currentPage != NULL) {
	if (!(currentPage->sched_isDone)) {
	  if (!(currentPage->sched_visited)) {
	    findDeadLockedStreams_traverse_helper(currentPage,
						  &traversedStreams,
						  &foundCycles,
						  processorNode,
						  deadLockedCycles);
	  }
	}
      }
    }
    for (i = 0; i < currentOperator->segments; i++) {
      ScoreSegment *currentSegment = currentOperator->segment[i];

      if (currentSegment != NULL) {
	if (!(currentSegment->sched_isDone)) {
	  if (!(currentSegment->sched_visited)) {
	    findDeadLockedStreams_traverse_helper(currentSegment,
						  &traversedStreams,
						  &foundCycles,
						  processorNode,
						  deadLockedCycles);
	  }
	}
      }
    }
  }

  // go through the list of lists of ScoreStreams involved in cycles and
  // determine which one of them are full (as opposed to empty).
  // remove the ScoreStreams from the list of lists that are part of the
  // dependency cycle because they are empty.
  // also, remove any stream that are attached directly to stitch buffers
  // or the processor.
  forall_items(listItem, foundCycles) {
    list<ScoreStream *> *currentList = foundCycles.inf(listItem);
    list_item listItem2;
    char containedStitchOrProcessorStreams;

    containedStitchOrProcessorStreams = 0;
    listItem2 = currentList->first();
    while (listItem2 != nil_item) {
      ScoreStream *currentStream = currentList->inf(listItem2);
      list_item nextItem = currentList->succ(listItem2);

      // delete non-potentially-full streams, etc.
      // NOTE: We should never have to worry about done src/sink since
      //       it could not be in a loop otherwise!
      if (!(currentStream->sched_isPotentiallyFull)) {
        currentList->erase(listItem2);
      } else {
        char srcIsProcessor =  
          (currentStream->sched_srcFunc == STREAM_OPERATOR_TYPE);
        char sinkIsProcessor =  
          (currentStream->sched_snkFunc == STREAM_OPERATOR_TYPE);
        char srcIsStitch = 
          ((currentStream->sched_srcFunc == STREAM_SEGMENT_TYPE) &&
           ((ScoreSegmentStitch *) currentStream->sched_src)->sched_isStitch);
        char sinkIsStitch = 
          ((currentStream->sched_snkFunc == STREAM_SEGMENT_TYPE) &&
           ((ScoreSegmentStitch *) currentStream->sched_sink)->sched_isStitch);

        if (srcIsProcessor || sinkIsProcessor || srcIsStitch || sinkIsStitch) {
          currentList->erase(listItem2);
          containedStitchOrProcessorStreams = 1;
        }
      }

      listItem2 = nextItem;
    }

    // it is potentially possible that we got a loop of all potentially
    // empty streams. in this case, we have just removed all streams in the
    // cycle so we might as well just remove that cycle from consideration
    // all together.
    // FIX ME! SHOULDN'T NEED THIS!
    if (currentList->length() == 0) {
      if (containedStitchOrProcessorStreams) {
        cerr << "SCHEDERR: THE ONLY POTENTIALLY FULLY STREAMS FOUND IN THE " <<
          "BUFFERLOCK LOOP WERE CONNECTED TO STITCH BUFFERS AND PROCESSORS!" <<
          endl;
        exit(1);
      } else {
        if (VERBOSEDEBUG || DEBUG) {
  	  cerr << "SCHED: WHOOPS A DEPENDENCY CYCLE OF ALL EMPTY STREAMS! " <<
	    "LET'S GET RID OF THAT!" << endl;
        }

        foundCycles.erase(listItem);

        delete(currentList);
      }
    }
  }

  // find the minimum set of ScoreStreams that will solve the bufferlock
  // dependency cycle problem.
  // FIX ME! THERE SHOULD BE A BETTER WAY TO SEARCH FOR COMMON SCORESTREAMS
  //         AMONG THE VARIOUS LISTS.
  // FIX ME!!! THIS IS CERTAINLY NOT OPTIMAL!!!! SHOULD SEARCH FOR MINIMUM
  //           SET OF SCORESTREAMS TO BREAKUP ALL CYCLES!
  forall_items(listItem, foundCycles) {
    list<ScoreStream *> *currentList = foundCycles.inf(listItem);

    currentList->sort();
  }
  forall_items(listItem, foundCycles) {
    list<ScoreStream *> *currentList = foundCycles.inf(listItem);

    bufferLockedStreams->append(currentList->front());

    delete(currentList);
  }
  bufferLockedStreams->sort();
  bufferLockedStreams->unique();

  processorNode->setNumIO(0, 0);
}


///////////////////////////////////////////////////////////////////////////////
// ScoreSchedulerStatic::findDeadLockedStreams_traverse_helper:
//   Finds cycles in the buffer dependency graphs that include the given
//     start node (any intermediate cycles, for now, are not returned).
//   The resulting cycles (there may be more than one) are returned by
//     placing them on the given list of lists of ScoreStream*. The
//     the ScoreStreams are the streams that are part of the cycle.
//
// Parameters:
//   currentNode: the node to DFS. 
//   traversedStreams: the list of currently traversed streams.
//   foundCycles: the list of lists of streams in a buffer dependency cycle.
//   processorNode: the node used to represent the processor.
//   deadLockedCycles: cycles that are causing deadlock.
//
// Return value:
//   none. all return is done by placing things on the given list of lists of 
//     ScoreStreams.
///////////////////////////////////////////////////////////////////////////////
void ScoreSchedulerStatic::findDeadLockedStreams_traverse_helper(
  ScoreGraphNode *currentNode,
  list<ScoreStream *> *traversedStreams,
  list<list<ScoreStream *> *> *foundCycles,
  ScoreGraphNode *processorNode,
  list<list<ScoreStream *> *> *deadLockedCycles) {
  list<ScoreStream *> *copyOfTraversedStreams;
  unsigned int numInputs = currentNode->getInputs();
  unsigned int numOutputs = currentNode->getOutputs();
  unsigned int i;
  list_item listItem;


  // determine if this node has already been visited and is also the start
  // node. if so, then a cycle has been found!
  // also, if this node was visited, no further DFS should be performed.
  if (currentNode->sched_visited2) {
    char isDeadLockedCycle = 1;
    char hasFullStream = 0;

    // copy the list contents.
    // check to see if the traversed streams form a deadlocked cycle.
    // copy up to the point of the cycle.
    copyOfTraversedStreams = new list<ScoreStream *>;
    listItem = traversedStreams->first();
    while (listItem != nil_item) {
      ScoreStream *currentStream = traversedStreams->inf(listItem);
      ScoreGraphNode *srcNode;

      copyOfTraversedStreams->append(currentStream);
      
      if (currentStream->sched_isPotentiallyFull) {
	isDeadLockedCycle = 0;
	hasFullStream = 1;

	if (currentStream->sched_srcFunc == STREAM_OPERATOR_TYPE) {
	  srcNode = processorNode;
	} else {
	  srcNode = currentStream->sched_src;
	}
      } else {
	if (currentStream->sched_snkFunc == STREAM_OPERATOR_TYPE) {
	  isDeadLockedCycle = 0;

	  srcNode = processorNode;
	} else {
	  srcNode = currentStream->sched_sink;
	}
      }

      if (srcNode == currentNode) {
	break;
      }

      listItem = traversedStreams->succ(listItem);
    }

    if (!isDeadLockedCycle) {
      if (hasFullStream) {
	foundCycles->append(copyOfTraversedStreams);
      } else {
	delete(copyOfTraversedStreams);
      }
    } else {
      deadLockedCycles->append(copyOfTraversedStreams);
    }

    return;
  }
  
  // mark this node as visited.
  currentNode->sched_visited = 1;
  currentNode->sched_visited2 = 1;

  // traverse all dependent inputs and outputs.
  for (i = 0; i < numOutputs; i++) {
    char isDependentOnOutput = currentNode->sched_dependentOnOutputBuffer[i];

    if (isDependentOnOutput) {
      ScoreStream *dependentStream = currentNode->getSchedOutput(i);
      ScoreGraphNode *dependentNode = dependentStream->sched_sink;

      // make sure that the sink is not done. if it is then something is
      // actually wrong! (since we should have un-potentiallyFulled all input
      // streams of done pages/segments). but, for now, just ignore it!
      if (!(dependentStream->sched_sinkIsDone)) {
	// if the dependent node is on the processor, then substitute the
	// processor node in.
	if (dependentStream->sched_snkFunc == STREAM_OPERATOR_TYPE) {
	  dependentNode = processorNode;
	}
	
#if EXHAUSTIVEDEADLOCKSEARCH
	traversedStreams->push(dependentStream);
	findDeadLockedStreams_traverse_helper(
          dependentNode, traversedStreams, foundCycles,
	  processorNode, deadLockedCycles);
	traversedStreams->pop();
#else
	if (!(dependentNode->sched_visited && 
	      !(dependentNode->sched_visited2))) {
	  traversedStreams->push(dependentStream);
	  findDeadLockedStreams_traverse_helper(
            dependentNode, traversedStreams, foundCycles,
	    processorNode, deadLockedCycles);
	  traversedStreams->pop();
	}
#endif
      } else {
	if (VERBOSEDEBUG || DEBUG) {
	  cerr << "SCHED: Odd... while checking for deadlock, found a " <<
	    "stream who is marked potentially full but whose sink is done!" <<
	    endl;
	}

	// we will reset the dependent on output flag so that this does not
	// get counted more than once!
	currentNode->sched_dependentOnOutputBuffer[i] = 0;
      }
    }
  }
  for (i = 0; i < numInputs; i++) {
    char isDependentOnInput = currentNode->sched_dependentOnInputBuffer[i];

    if (isDependentOnInput) {
      ScoreStream *dependentStream = currentNode->getSchedInput(i);
      ScoreGraphNode *dependentNode = dependentStream->sched_src;

      // make sure that the source is not done. if it is and this
      // node is waiting on it for inputs, then, we are in deadlock!
      if (!(dependentStream->sched_srcIsDone)) {
	// if the dependent node is on the processor, then substitute the
	// processor node in.
	if (dependentStream->sched_srcFunc == STREAM_OPERATOR_TYPE) {
	  dependentNode = processorNode;
	}
	
#if EXHAUSTIVEDEADLOCKSEARCH
	traversedStreams->push(dependentStream);
	findDeadLockedStreams_traverse_helper(
          dependentNode, traversedStreams, foundCycles,
	  processorNode, deadLockedCycles);
	traversedStreams->pop();
#else
	if (!(dependentNode->sched_visited && 
	      !(dependentNode->sched_visited2))) {
	  traversedStreams->push(dependentStream);
	  findDeadLockedStreams_traverse_helper(
            dependentNode, traversedStreams, foundCycles,
	    processorNode, deadLockedCycles);
	  traversedStreams->pop();
	}
#endif
      } else {
	// make sure we are not on the processor node! the reason for this
	// is that we currently do not have knowledge of which processor
	// I/O are being read/written from the processor-side.
	// FIX ME! FIND A WAY TO FIGURE OUT WHAT THE PROCESSOR READ/WRITE
	//         REQUESTS ARE!
	if (currentNode != processorNode) {
	  copyOfTraversedStreams = new list<ScoreStream *>;
	  copyOfTraversedStreams->push(dependentStream);
	  
	  deadLockedCycles->append(copyOfTraversedStreams);

	  // we will reset the dependent on input flag so that this does not
	  // get counted more than once!
	  currentNode->sched_dependentOnInputBuffer[i] = 0;
	}
      }
    }
  }

  // unmark this node as visited.
  currentNode->sched_visited2 = 0;
}


///////////////////////////////////////////////////////////////////////////////
// ScoreSchedulerStatic::resolveBufferLockedStreams:
//   Given a set of buffer locked streams, attempts to resolve the
//     buffer lock.
//
// Parameters:
//   bufferLockedStreams: list of buffer locked streams.
//
// Return value:
//   none.
///////////////////////////////////////////////////////////////////////////////
void ScoreSchedulerStatic::resolveBufferLockedStreams(
  list<ScoreStream *> *bufferLockedStreams) {
  list_item listItem;


  // deal with each bufferlocked stream.
  forall_items(listItem, *bufferLockedStreams) {
    ScoreStream *currentStream = bufferLockedStreams->inf(listItem);
    ScoreGraphNode *currentSrc = currentStream->sched_src;
    ScoreGraphNode *currentSink = currentStream->sched_sink;
    int currentSrcNum = currentStream->sched_srcNum;
    ScoreStreamType *currentSrcType = currentSrc->outputType(currentSrcNum);
    ScoreCluster *currentSrcCluster = currentSrc->sched_parentCluster;
    ScoreCluster *currentSinkCluster = currentSink->sched_parentCluster;
    
    // make sure either end of the stream is not a stitch buffer!
    // FIX ME! IF THIS SHOULD HAPPEN, THEN WE SHOULD BE INCREASING THE
    //         SIZE OF THE STITCH BUFFER INSTEAD!
    if (((currentStream->sched_srcFunc == STREAM_SEGMENT_TYPE) &&
	 (((ScoreSegment *) currentSrc)->sched_isStitch)) ||
	((currentStream->sched_snkFunc == STREAM_SEGMENT_TYPE) &&
	 (((ScoreSegment *) currentSink)->sched_isStitch))) {
      cerr << "SCHED: FIX ME! WE SHOULD NOT BE SELECTING A STREAM NEXT TO A "
	   << "STITCH BUFFER FOR BUFFERLOCK RESOLUTION! INSTEAD, WE SHOULD BE "
	   << "EXPANDING THE BUFFER!" << endl;
    }

    // make sure either end of the stream is not the processor!
    // FIX ME! NEED TO FIGURE OUT WHAT TO DO IN GENERAL WITH STITCH BUFFERS
    //         AND PROCESSOR! PROCESSOR IO BUFFERING??
    if ((currentStream->sched_srcFunc == STREAM_OPERATOR_TYPE) &&
	(currentStream->sched_snkFunc == STREAM_OPERATOR_TYPE)) {
      cerr << "SCHED: FIX ME! WE DON'T KNOW WHAT TO DO WHEN STITCH BUFFERING "
	   << "AND SRC/SINK IS AN OPERATOR!" << endl;
      exit(1);
    }

    // determine if the stream is inter or intra cluster. act appropriately
    // to add in the stitch buffer.
    if (currentSrcCluster != currentSinkCluster) {
      ScoreStreamStitch *newStream = NULL;
      
      // attempt to get a spare stream stitch!
      if (SCORECUSTOMSTACK_LENGTH(spareStreamStitchList) > 0) {
	ScoreStream *tempStream;
	  
	SCORECUSTOMSTACK_POP(spareStreamStitchList, tempStream);
	newStream = (ScoreStreamStitch *) tempStream;
	
	newStream->recycle(currentStream->get_width(),
			   currentStream->get_fixed(),
			   currentStream->get_length(),
			   currentStream->get_type());
      } else {
	if (VERBOSEDEBUG || DEBUG) {
	  cout << "SCHED: RAN OUT OF SPARE STITCH STREAMS! " <<
	    "INSTANTIATING A NEW ONE!" << endl;
	}
	
	newStream = new ScoreStreamStitch(currentStream->get_width(),
					  currentStream->get_fixed(),
					  currentStream->get_length(),
					  currentStream->get_type());

	newStream->reset();
	newStream->sched_spareStreamStitchList = spareStreamStitchList;
      }

      newStream->producerClosed = currentStream->producerClosed;
      currentStream->producerClosed = 0;
      newStream->producerClosed_hw = currentStream->producerClosed_hw;
      currentStream->producerClosed_hw = 0;
      newStream->sched_isCrossCluster = currentStream->sched_isCrossCluster;

      currentSrc->unbindSchedOutput(currentSrcNum);
      currentSrc->bindSchedOutput(currentSrcNum, newStream, currentSrcType);

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
	
      newStitch->recycle(SCORE_DATASEGMENTBLOCK_LOADSIZE/(SCORE_ALIGN_BITWIDTH_TO_8(currentStream->get_width())/8),
			 currentStream->get_width(),
			 newStream, currentStream);

      // FIX ME! I ASSUME THAT THE PROCESS TO ASSOCIATE THIS STITCH
      //         BUFFER TO IS THE SAME AS THE ATTACHEDNODE.
      newStitch->sched_parentProcess = currentSrc->sched_parentProcess;
	      
      newStitch->sched_residentStart = 0;
      newStitch->sched_maxAddr = newStitch->length();
      newStitch->sched_residentLength = newStitch->length();

      // fix the output list of the source cluster.
      SCORECUSTOMLIST_REPLACE(currentSrcCluster->outputList, 
			      currentStream, newStream);
	      
      newStitch->sched_old_mode = newStitch->sched_mode =
        SCORE_CMB_SEQSRCSINK;

      // add the new stitch buffer to the stitch buffer list.
      SCORECUSTOMLIST_APPEND(stitchBufferList, newStitch);
      SCORECUSTOMLIST_APPEND(newStitch->sched_parentProcess->stitchBufferList, newStitch);
      newStitch->sched_parentProcess->numSegments++;
      SCORECUSTOMLIST_APPEND(addedBufferLockStitchBufferList, newStitch);

      // make sure the stitch buffer does not get cleaned up when empty!
      newStitch->sched_mustBeInDataFlow = 1;
    } else {
      ScoreStreamStitch *newStream = NULL;
      
      // attempt to get a spare stream stitch!
      if (SCORECUSTOMSTACK_LENGTH(spareStreamStitchList) > 0) {
	ScoreStream *tempStream;
	  
	SCORECUSTOMSTACK_POP(spareStreamStitchList, tempStream);
	newStream = (ScoreStreamStitch *) tempStream;
	
	newStream->recycle(currentStream->get_width(),
			   currentStream->get_fixed(),
			   currentStream->get_length(),
			   currentStream->get_type());
      } else {
	if (VERBOSEDEBUG || DEBUG) {
	  cout << "SCHED: RAN OUT OF SPARE STITCH STREAMS! " <<
	    "INSTANTIATING A NEW ONE!" << endl;
	}
	
	newStream = new ScoreStreamStitch(currentStream->get_width(),
					  currentStream->get_fixed(),
					  currentStream->get_length(),
					  currentStream->get_type());

	newStream->reset();
	newStream->sched_spareStreamStitchList = spareStreamStitchList;
      }
      
      newStream->producerClosed = currentStream->producerClosed;
      currentStream->producerClosed = 0;
      newStream->producerClosed_hw = currentStream->producerClosed_hw;
      currentStream->producerClosed_hw = 0;
      newStream->sched_isCrossCluster = currentStream->sched_isCrossCluster;

      currentSrc->unbindSchedOutput(currentSrcNum);
      currentSrc->bindSchedOutput(currentSrcNum, newStream, currentSrcType);

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
	
      newStitch->recycle(SCORE_DATASEGMENTBLOCK_LOADSIZE/(SCORE_ALIGN_BITWIDTH_TO_8(currentStream->get_width())/8),
			 currentStream->get_width(),
			 newStream, currentStream);

      // FIX ME! I ASSUME THAT THE PROCESS TO ASSOCIATE THIS STITCH
      //         BUFFER TO IS THE SAME AS THE ATTACHEDNODE.
      newStitch->sched_parentProcess = currentSrc->sched_parentProcess;

      newStitch->sched_residentStart = 0;
      newStitch->sched_maxAddr = newStitch->length();
      newStitch->sched_residentLength = newStitch->length();
	      
      // fix the input/output list of the cluster.
      SCORECUSTOMLIST_APPEND(currentSrcCluster->inputList, currentStream);
      SCORECUSTOMLIST_APPEND(currentSrcCluster->outputList, newStream);
	      
      newStitch->sched_old_mode = newStitch->sched_mode =
        SCORE_CMB_SEQSRCSINK;

      // add the new stitch buffer to the stitch buffer list.
      SCORECUSTOMLIST_APPEND(stitchBufferList, newStitch);
      SCORECUSTOMLIST_APPEND(newStitch->sched_parentProcess->stitchBufferList, newStitch);
      newStitch->sched_parentProcess->numSegments++;
      SCORECUSTOMLIST_APPEND(addedBufferLockStitchBufferList, newStitch);

      // make sure the stitch buffer does not get cleaned up when empty!
      newStitch->sched_mustBeInDataFlow = 1;

      // perform check to see if this would cause the cluster to no longer
      // be schedulable by itself!
      if (currentSrcCluster->getNumMemSegRequired() > numPhysicalCMB) {
	// FIX ME! IF THIS HAPPENS, WE SHOULD ACTUALLY SEE IF WE CAN BREAK
	//         THIS CLUSTER UP INTO MANAGEABLE CLUSTERS!
	cerr << "SCHED: FIX ME! Adding a stitch buffer to the cluster " <<
	  "caused the cluster to no longer be safely schedulable!" << endl;
	exit(1);
      }
    }
  }
}


///////////////////////////////////////////////////////////////////////////////
// ScoreSchedulerStatic::resolveDeadLockedCycles:
//   Given a set of dead locked cycles, attempts to resolve the
//     dead lock.
//
// Parameters:
//   deadLockedCycles: list of dead locked streams.
//
// Return value:
//   none.
///////////////////////////////////////////////////////////////////////////////
void ScoreSchedulerStatic::resolveDeadLockedCycles(
  list<list<ScoreStream *> *> *deadLockedCycles) {
  list_item listItem;
  int i;


  // print out the state of the scheduler so the user can diagnose the problem!
  printCurrentState();

  cerr <<
    "SCHED: ***** POTENTIAL DEADLOCK DETECTED! *****" <<
    endl;
  cerr <<
    "SCHED: ***** NUMBER OF DEADLOCK CYCLES: " <<
    deadLockedCycles->length() << endl;
  cerr << endl;

  i = 0;
  forall_items(listItem, *deadLockedCycles) {
    list<ScoreStream *> *currentList = deadLockedCycles->inf(listItem);
    list_item listItem2;
    
    cerr << "SCHED:    *** DEADLOCK CYCLE: " << i << endl;
    i++;

    forall_items(listItem2, *currentList) {
      ScoreStream *currentStream = currentList->inf(listItem2);

      cerr << "SCHED:      STREAM " << (unsigned int) currentStream << endl;
      cerr << "SCHED:        SRC: ";
      if (currentStream->sched_srcFunc == STREAM_OPERATOR_TYPE) {
	cerr << "OPERATOR";
      } else if (currentStream->sched_srcFunc == STREAM_PAGE_TYPE) {
	cerr << "PAGE";
      } else if (currentStream->sched_srcFunc == STREAM_SEGMENT_TYPE) {
	cerr << "SEGMENT";
      } else {
	cerr << "UNKNOWN";
      }
      if (!(currentStream->sched_srcIsDone)) {
	if (currentStream->sched_srcFunc != STREAM_OPERATOR_TYPE) {
	  cerr << "(" << (unsigned int) currentStream->sched_src << ")(" <<
	    currentStream->sched_srcNum << ")";
	}
	if (currentStream->sched_srcFunc == STREAM_PAGE_TYPE) {
	  cerr << " " << ((ScorePage *) currentStream->sched_src)->getSource();
	}
      } else {
	cerr << " (DONE!)" << endl;
      }
      cerr << endl;
      cerr << "SCHED:        SINK: ";
      if (currentStream->sched_snkFunc == STREAM_OPERATOR_TYPE) {
	cerr << "OPERATOR";
      } else if (currentStream->sched_snkFunc == STREAM_PAGE_TYPE) {
	cerr << "PAGE";
      } else if (currentStream->sched_snkFunc == STREAM_SEGMENT_TYPE) {
	cerr << "SEGMENT";
      } else {
	cerr << "UNKNOWN";
      }
      if (!(currentStream->sched_sinkIsDone)) {
	if (currentStream->sched_snkFunc != STREAM_OPERATOR_TYPE) {
	  cerr << "(" << (unsigned int) currentStream->sched_sink << ")(" <<
	    currentStream->sched_snkNum << ")";
	}
	if (currentStream->sched_snkFunc == STREAM_PAGE_TYPE) {
	  cerr << " " << ((ScorePage *) currentStream->sched_sink)->getSource();
	}
      } else {
	cerr << "SCHED: (DONE!)" << endl;
      }
      cerr << endl;
    }
  }

  exit(1);
}


///////////////////////////////////////////////////////////////////////////////
// ScoreSchedulerStatic::printCurrentState:
//   Prints out the current state of the scheduler.
//
// Parameters:
//   none.
//
// Return value:
//   none.
///////////////////////////////////////////////////////////////////////////////
void ScoreSchedulerStatic::printCurrentState() {
  unsigned int h, i, j, k;
    

  for (h = 0; h < SCORECUSTOMLIST_LENGTH(processList); h++) {
    ScoreProcess *currentProcess;

    SCORECUSTOMLIST_ITEMAT(processList, h, currentProcess);

    cerr << "SCHED: PROCESS ID ===> " << currentProcess->pid << endl;    
    for (i = 0; i < SCORECUSTOMLIST_LENGTH(currentProcess->operatorList);
	 i++) {
      ScoreOperatorInstance *opi;

      SCORECUSTOMLIST_ITEMAT(currentProcess->operatorList, i, opi);

      cerr << "SCHED: OPERATOR INSTANCE ===> " << 
	(unsigned int) opi << endl;

      cerr << "SCHED: PAGES===================" << endl;
      for (j = 0; j < opi->pages; j++) {
	if (opi->page[j] != NULL) {
	  cerr << "SCHED:    PAGE " << j << 
	    " (source=" << opi->page[j]->getSource() << ")" << 
	    " (state=" << opi->page[j]->sched_lastKnownState << ")" << 
	    "\t" << (unsigned int) opi->page[j] << endl;
	  
	  for (k = 0; k < (unsigned int) opi->page[j]->getInputs(); k++) {
	    cerr << "SCHED:       INPUT " << k << " srcFunc " << 
	      opi->page[j]->getSchedInput(k)->sched_srcFunc << " snkFunc " << 
	      opi->page[j]->getSchedInput(k)->sched_snkFunc << " ";
	    if (opi->page[j]->getSchedInput(k)->sched_src == NULL) {
	      cerr << "SRCISNULL ";
	    }
	    if (opi->page[j]->getSchedInput(k)->sched_sink == NULL) {
	      cerr << "SINKISNULL ";
	    }
	    cerr << "\t";
	    if (opi->page[j]->getSchedInput(k)->sched_isPotentiallyFull) {
	      cerr << "*FULL* ";
	    }
	    if (opi->page[j]->getSchedInput(k)->sched_isPotentiallyEmpty) {
	      cerr << "*EMPTY* ";
	    }
	    cerr << "\t NODECONSUMED " << opi->page[j]->getInputConsumption(k) << endl;
	    cerr << "\t PRODUCED " << STREAM_TOKENS_PRODUCED(opi->page[j]->getSchedInput(k)) << " CONSUMED " << STREAM_TOKENS_CONSUMED(opi->page[j]->getSchedInput(k)) << endl;
	    cerr << "\t"<< (unsigned int) opi->page[j]->getSchedInput(j);
	    if (opi->page[j]->getSchedInput(k)->sched_srcIsDone) {
	      cerr << " (DONE!)" << endl;
	    }
	    cerr << endl;
	  }
	  for (k = 0; k < (unsigned int) opi->page[j]->getOutputs(); k++) {
	    cerr << "SCHED:       OUTPUT " << k << " srcFunc " << 
	      opi->page[j]->getSchedOutput(k)->sched_srcFunc << " snkFunc " << 
	      opi->page[j]->getSchedOutput(k)->sched_snkFunc << " ";
	    if (opi->page[j]->getSchedOutput(k)->sched_src == NULL) {
	      cerr << "SRCISNULL ";
	    }
	    if (opi->page[j]->getSchedOutput(k)->sched_sink == NULL) {
	      cerr << "SINKISNULL ";
	    }
	    cerr << "\t";
	    if (opi->page[j]->getSchedOutput(k)->sched_isPotentiallyFull) {
	      cerr << "*FULL* ";
	    }
	    if (opi->page[j]->getSchedOutput(k)->sched_isPotentiallyEmpty) {
	      cerr << "*EMPTY* ";
	    }
	    cerr << "\t NODEPRODUCED " << opi->page[j]->getOutputProduction(k) << endl;
	    cerr << "\t PRODUCED " << STREAM_TOKENS_PRODUCED(opi->page[j]->getSchedOutput(k)) << " CONSUMED " << STREAM_TOKENS_CONSUMED(opi->page[j]->getSchedOutput(k)) << endl;
	    cerr << "\t"<< (unsigned int) opi->page[j]->getSchedOutput(k);
	    if (opi->page[j]->getSchedOutput(k)->sched_sinkIsDone) {
	      cerr << " (DONE!)" << endl;
	    }
	    cerr << endl;
	  }
	}
      }
      cerr << "SCHED: ========================" << endl;
      cerr << "SCHED: SEGMENTS===================" << endl;
      for (j = 0; j < opi->segments; j++) {
	if (opi->segment[j] != NULL) {
	  cerr << "SCHED:    SEGMENT " << j <<
	    "\t" << (unsigned int) opi->segment[j] << endl;
	  
	  for (k = 0; k < (unsigned int) opi->segment[j]->getInputs(); k++) {
	    cerr << "SCHED:       INPUT " << k << " srcFunc " << 
	      opi->segment[j]->getSchedInput(k)->sched_srcFunc << " snkFunc " << 
	      opi->segment[j]->getSchedInput(k)->sched_snkFunc << " ";
	    if (opi->segment[j]->getSchedInput(k)->sched_src == NULL) {
	      cerr << "SRCISNULL ";
	    }
	    if (opi->segment[j]->getSchedInput(k)->sched_sink == NULL) {
	      cerr << "SINKISNULL ";
	    }
	    cerr << "\t";
	    if (opi->segment[j]->getSchedInput(k)->sched_isPotentiallyFull) {
	      cerr << "*FULL* ";
	    }
	    if (opi->segment[j]->getSchedInput(k)->sched_isPotentiallyEmpty) {
	      cerr << "*EMPTY* ";
	    }
	    cerr << "\t NODECONSUMED " << opi->segment[j]->getInputConsumption(k) << endl;
	    cerr << "\t PRODUCED " << STREAM_TOKENS_PRODUCED(opi->segment[j]->getSchedInput(k)) << " CONSUMED " << STREAM_TOKENS_CONSUMED(opi->segment[j]->getSchedInput(k)) << endl;
	    cerr << "\t"<< (unsigned int) opi->segment[j]->getSchedInput(k);
	    if (opi->segment[j]->getSchedInput(k)->sched_srcIsDone) {
	      cerr << " (DONE!)" << endl;
	    }
	    cerr << endl;
	  }
	  for (k = 0; k < (unsigned int) opi->segment[j]->getOutputs(); k++) {
	    cerr << "SCHED:       OUTPUT " << k << " srcFunc " << 
	      opi->segment[j]->getSchedOutput(k)->sched_srcFunc << " snkFunc " << 
	      opi->segment[j]->getSchedOutput(k)->sched_snkFunc << " ";
	    if (opi->segment[j]->getSchedOutput(k)->sched_src == NULL) {
	      cerr << "SRCISNULL ";
	    }
	    if (opi->segment[j]->getSchedOutput(k)->sched_sink == NULL) {
	      cerr << "SINKISNULL ";
	    }
	    cerr << "\t";
	    if (opi->segment[j]->getSchedOutput(k)->sched_isPotentiallyFull) {
	      cerr << "*FULL* ";
	    }
	    if (opi->segment[j]->getSchedOutput(k)->sched_isPotentiallyEmpty) {
	      cerr << "*EMPTY* ";
	    }
	    cerr << "\t NODEPRODUCED " << opi->segment[j]->getOutputProduction(k) << endl;
	    cerr << "\t PRODUCED " << STREAM_TOKENS_PRODUCED(opi->segment[j]->getSchedOutput(k)) << " CONSUMED " << STREAM_TOKENS_CONSUMED(opi->segment[j]->getSchedOutput(k)) << endl;
	    cerr << "\t"<< (unsigned int) opi->segment[j]->getSchedOutput(k);
	    if (opi->segment[j]->getSchedOutput(k)->sched_sinkIsDone) {
	      cerr << " (DONE!)" << endl;
	    }
	    cerr << endl;
	  }
	}
      }
      cerr << "SCHED: ========================" << endl;
    }

    cerr << "SCHED: STITCH BUFFERS===================" << endl;
    for (i = 0; i < SCORECUSTOMLIST_LENGTH(currentProcess->stitchBufferList);
	 i++) {
      ScoreSegmentStitch *currentStitch;

      SCORECUSTOMLIST_ITEMAT(currentProcess->stitchBufferList, i, currentStitch);
      
      cerr << "SCHED:    STITCH " << i <<
	"\t" << (unsigned int) currentStitch << endl;
      
      for (j = 0; j < (unsigned int) currentStitch->getInputs(); j++) {
	cerr << "SCHED:       INPUT " << j << " srcFunc " << 
	  currentStitch->getSchedInput(j)->sched_srcFunc << " snkFunc " << 
	  currentStitch->getSchedInput(j)->sched_snkFunc << " ";
	if (currentStitch->getSchedInput(j)->sched_src == NULL) {
	  cerr << "SRCISNULL ";
	}
	if (currentStitch->getSchedInput(j)->sched_sink == NULL) {
	  cerr << "SINKISNULL ";
	}
	cerr << "\t";
	if (currentStitch->getSchedInput(j)->sched_isPotentiallyFull) {
	  cerr << "*FULL* ";
	}
	if (currentStitch->getSchedInput(j)->sched_isPotentiallyEmpty) {
	  cerr << "*EMPTY* ";
	}
	cerr << "\t NODECONSUMED " << currentStitch->getInputConsumption(j) << endl;
	cerr << "\t PRODUCED " << STREAM_TOKENS_PRODUCED(currentStitch->getSchedInput(j)) << " CONSUMED " << STREAM_TOKENS_CONSUMED(currentStitch->getSchedInput(j)) << endl;
	cerr << "\t"<< (unsigned int) currentStitch->getSchedInput(j);
	if (currentStitch->getSchedInput(j)->sched_srcIsDone) {
	  cerr << " (DONE!)" << endl;
	}
	cerr << endl;
      }
      for (j = 0; j < (unsigned int) currentStitch->getOutputs(); j++) {
	cerr << "SCHED:       OUTPUT " << j << " srcFunc " << 
	  currentStitch->getSchedOutput(j)->sched_srcFunc << " snkFunc " << 
	  currentStitch->getSchedOutput(j)->sched_snkFunc << " ";
	if (currentStitch->getSchedOutput(j)->sched_src == NULL) {
	  cerr << "SRCISNULL ";
	}
	if (currentStitch->getSchedOutput(j)->sched_sink == NULL) {
	  cerr << "SINKISNULL ";
	}
	cerr << "\t";
	if (currentStitch->getSchedOutput(j)->sched_isPotentiallyFull) {
	  cerr << "*FULL* ";
	}
	if (currentStitch->getSchedOutput(j)->sched_isPotentiallyEmpty) {
	  cerr << "*EMPTY* ";
	}
	cerr << "\t NODEPRODUCED " << currentStitch->getOutputProduction(j) << endl;
	cerr << "\t PRODUCED " << STREAM_TOKENS_PRODUCED(currentStitch->getSchedOutput(j)) << " CONSUMED " << STREAM_TOKENS_CONSUMED(currentStitch->getSchedOutput(j)) << endl;
	cerr << "\t"<< (unsigned int) currentStitch->getSchedOutput(j);
	if (currentStitch->getSchedOutput(j)->sched_sinkIsDone) {
	  cerr << " (DONE!)" << endl;
	}
	cerr << endl;
      }
    }
    cerr << "SCHED: ========================" << endl;
  }
}

///////////////////////////////////////////////////////////////////////////////
// ScoreSchedulerStatic::visualizeCurrentState:
//   Dumps current state as a frame in the visualFile
//
// Parameters:
//   none.
//
// Return value:
//   none.
///////////////////////////////////////////////////////////////////////////////
void ScoreSchedulerStatic::visualizeCurrentState() {
  
  if (visualFile == NULL)
    return;

  FILE *fp = NULL;

  if ((fp = fopen(visualFile, "ab")) == NULL) {
    cerr << "WARNING: visualFile could not be opened, the simulation will proceed without visualization\n";
    delete visualFile;
    visualFile = NULL;
  }

  unsigned int h, i, j, k;
  ScoreStateGraph stateGraph(currentTimeslice);

  if (VERBOSEDEBUG) {
    cerr << "++++++++++++stateGraph initialized:" << endl;
    stateGraph.print(stderr);
  }

  // (1) add all nodes
  // ------------------
    
  for (h = 0; h < SCORECUSTOMLIST_LENGTH(processList); h++) {
    ScoreProcess *currentProcess;

    SCORECUSTOMLIST_ITEMAT(processList, h, currentProcess);
    
    for (i = 0; i < SCORECUSTOMLIST_LENGTH(currentProcess->operatorList);
	 i++) {
      ScoreOperatorInstance *opi;

      SCORECUSTOMLIST_ITEMAT(currentProcess->operatorList, i, opi);

      for (j = 0; j < opi->pages; j++) {
	if (opi->page[j] != NULL)
	  stateGraph.addNode(currentProcess->pid, j, opi->page[j]);
      }
      for (j = 0; j < opi->segments; j++) {
	if (opi->segment[j] != NULL)
	  stateGraph.addNode(currentProcess->pid, j, opi->segment[j]);
      }
    }

    for (i = 0; i < SCORECUSTOMLIST_LENGTH(currentProcess->stitchBufferList);
	 i++) {
      ScoreSegmentStitch *currentStitch;

      SCORECUSTOMLIST_ITEMAT(currentProcess->stitchBufferList, i, currentStitch);

      stateGraph.addNode(currentProcess->pid, i, currentStitch);
    }
  }

  if (VERBOSEDEBUG) {
    cerr << "+++++++++stateGraph nodes added:" << endl;
    stateGraph.print(stderr);
  }

  // (2) add all edges
  // -----------------

  for (h = 0; h < SCORECUSTOMLIST_LENGTH(processList); h++) {
    ScoreProcess *currentProcess;

    SCORECUSTOMLIST_ITEMAT(processList, h, currentProcess);

    for (i = 0; i < SCORECUSTOMLIST_LENGTH(currentProcess->operatorList);
	 i++) {
      ScoreOperatorInstance *opi;

      SCORECUSTOMLIST_ITEMAT(currentProcess->operatorList, i, opi);
      
      for (j = 0; j < opi->pages; j++)
	if (opi->page[j] != NULL) {
	  for (k = 0; k < (unsigned int) opi->page[j]->getInputs(); k++)
	    stateGraph.addEdge(currentProcess->pid, opi->page[j]->getSchedInput(k));
	  
	  for (k = 0; k < (unsigned int) opi->page[j]->getOutputs(); k++)
	    //if (opi->page[j]->getSchedOutput(k)->snkFunc == STREAM_OPERATOR_TYPE)
	    stateGraph.addEdge(currentProcess->pid, opi->page[j]->getSchedOutput(k));
	}
      
      for (j = 0; j < opi->segments; j++)
	if (opi->segment[j] != NULL) {
	  for (k = 0; k < (unsigned int) opi->segment[j]->getInputs(); k++)
	    stateGraph.addEdge(currentProcess->pid, opi->segment[j]->getSchedInput(k));
	  for (k = 0; k < (unsigned int) opi->segment[j]->getOutputs(); k++)
	    //if (opi->segment[j]->getSchedOutput(k)->snkFunc == STREAM_OPERATOR_TYPE)
	    stateGraph.addEdge(currentProcess->pid, opi->segment[j]->getSchedOutput(k));
	}
    }

    for (i = 0; i < SCORECUSTOMLIST_LENGTH(currentProcess->stitchBufferList);
	 i++) {
      ScoreSegmentStitch *currentStitch;

      SCORECUSTOMLIST_ITEMAT(currentProcess->stitchBufferList, i, currentStitch);
      
      for (k = 0; k < (unsigned int) currentStitch->getInputs(); k++)
	stateGraph.addEdge(currentProcess->pid, currentStitch->getSchedInput(k));
	
      for (k = 0; k < (unsigned int) currentStitch->getOutputs(); k++)
	//if (currentStitch->getSchedOutput(k)->snkFunc == STREAM_OPERATOR_TYPE)
	stateGraph.addEdge(currentProcess->pid, currentStitch->getSchedOutput(k));
    }
  }


  // (3) mark edges if they are consumers or producer in current state
  // ------------------------------------------------------------------

  for (h = 0; h < SCORECUSTOMLIST_LENGTH(processList); h++) {
    ScoreProcess *currentProcess;

    SCORECUSTOMLIST_ITEMAT(processList, h, currentProcess);

    for (i = 0; i < SCORECUSTOMLIST_LENGTH(currentProcess->operatorList);
	 i++) {
      ScoreOperatorInstance *currentOperator;

      SCORECUSTOMLIST_ITEMAT(currentProcess->operatorList, i, currentOperator);
      
      for (j = 0; j < currentOperator->pages; j++) {
	ScorePage *currentPage = currentOperator->page[j];
	
	// ignore done pages.
	if (currentPage != NULL) {
	  unsigned int numInputs = currentPage->getInputs();
	  unsigned int numOutputs = currentPage->getOutputs();
	  int currentState = currentPage->sched_lastKnownState;
	  ScoreIOMaskType currentConsumed = currentPage->inputs_consumed(currentState);
	  ScoreIOMaskType currentProduced = currentPage->outputs_produced(currentState);
	  
	  for (k = 0; k < numInputs; k++) {
	    char isBeingConsumed = (currentConsumed >> k) & 1;

	    if (isBeingConsumed) {
	      SCORE_STREAM currentInput = currentPage->getSchedInput(k);
	      stateGraph.addEdgeStatus(currentInput, EDGE_STATUS_CONSUME);
	    }
	  }
	  for (k = 0; k < numOutputs; k++) {
	    char isBeingProduced = (currentProduced >> k) & 1;
	    
	    if (isBeingProduced) {
	      SCORE_STREAM currentOutput = currentPage->getSchedOutput(k);
	      stateGraph.addEdgeStatus(currentOutput, EDGE_STATUS_PRODUCE);
	    }
	  }
	}
      }
    }
  }


  if (VERBOSEDEBUG) {
    cerr << "stateGraph was built: " << endl;
    stateGraph.print(stderr);
  }

  // dump the created graph to disk
  stateGraph.write (fp);

  fflush(fp);
  fclose(fp);

}

#if GET_FEEDBACK
void ScoreSchedulerStatic::makeFeedback() {
  unsigned int i;

  for (unsigned int h = 0; h < SCORECUSTOMLIST_LENGTH(processList); h++) {
    ScoreProcess *currentProcess;
    
    SCORECUSTOMLIST_ITEMAT(processList, h, currentProcess);
    
    for (i = 0; i < SCORECUSTOMLIST_LENGTH(currentProcess->operatorList);
	 i++) {
      ScoreOperatorInstance *opi;
      
      SCORECUSTOMLIST_ITEMAT(currentProcess->operatorList, i, opi);
      ScoreGraphNode **graphNode;
      unsigned int maxGraphNodeCount;
      for (int counter = 0; counter < 2; counter ++) {
	if (counter == 0) { // process pages first
	  graphNode = (ScoreGraphNode**)opi->page;
	  maxGraphNodeCount = opi->pages;
	}
	else {
	  graphNode = (ScoreGraphNode**)opi->segment;
	  maxGraphNodeCount = opi->segments;
	}

	for (unsigned int j = 0; j < maxGraphNodeCount; j++) {
	  if (graphNode[j] != NULL) {
	    ScoreGraphNode *node = graphNode[j];
	    node->feedbackNode->recordConsumption(node->getConsumptionVector(),
						  node->getInputs());
	    node->feedbackNode->recordProduction(node->getProductionVector(),
						 node->getOutputs());
	    node->feedbackNode->recordFireCount(node->getFire());
	  }
	} // iterate through all nodes within a op instance
      } // first do pages, then segments
    } // go through all operators
  } // go through all processes
}
#endif

