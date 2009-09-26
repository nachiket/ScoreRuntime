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
// $Revision: 1.3 $
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

///////////////////////////////////////////////////////////////////////////////
// ScoreSchedulerRandom::addOperator:
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
int ScoreSchedulerRandom::addOperator(char *sharedObject, char *argbuf, pid_t pid) {
  construct_t construct;
  ScoreOperatorInstance *opi;
  ScoreProcess *process;
  graph operatorGraph;
  node_array<int> *componentNum;
  ScoreCluster **newClusters;
  list<ScoreCluster *> clusterList;
  list_item listItem, listItem2;
  char abortOperatorInsert;
  unsigned int i, j, numClusters;
#if ASPLOS2000
  // FIX ME! JUST FOR ASPLOS2000!
  list<ScorePage *> **fakeClusterSpecs = NULL;
  unsigned int numFakeClusters;
  list<ScorePage *> dummyPageList;
#endif


  // get a lock on the scheduler data mutex.
  pthread_mutex_lock(&schedulerDataMutex);

#if DOPROFILING
  startClock = threadCounter->ScoreThreadSchCounterRead();
#endif

  // try to get function pointer.
  {
    void *newHandle = dlopen(sharedObject, RTLD_LAZY);
    
    if (EXTRA_DEBUG)
      cerr << "<<<<<<<<<<<<dlOpen = " << newHandle << ">>>>>>>>>>>>>>>>\n";

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

  // Create the new clusters.
  // FIX ME! For now, we only optimize for clusters on a per operator
  //         basis, with no optimizations between operators. This means
  //         we do not capture feedback loops between operators.
#if ASPLOS2000
  // FIX ME! JUST FOR ASPLOS2000!
  numFakeClusters = 0;
#endif
  for (i = 0; i < opi->pages; i++) {
    opi->page[i]->sched_graphNode = operatorGraph.new_node();
#if ASPLOS2000
    // FIX ME! JUST FOR ASPLOS2000!
    if (opi->page[i]->group() != NO_GROUP) {
      if (((unsigned int) (opi->page[i]->group()+1)) > numFakeClusters) {
	numFakeClusters = opi->page[i]->group()+1;
      }
    }
#endif

    opi->page[i]->sched_parentOperator = opi;
  }
  for (i = 0; i < opi->segments; i++) {
    opi->segment[i]->sched_graphNode = operatorGraph.new_node();

    opi->segment[i]->sched_parentOperator = opi;
  }
#if ASPLOS2000
  // FIX ME! JUST FOR ASPLOS2000!
  if (numFakeClusters > 0) {
    fakeClusterSpecs = new (list<ScorePage *> *)[numFakeClusters];
    
    for (i = 0; i < numFakeClusters; i++) {
      fakeClusterSpecs[i] = new list<ScorePage *>;
    }
  }
#endif
  for (i = 0; i < opi->pages; i++) {
    ScorePage *currentPage = opi->page[i];
    int numOutputs = currentPage->getOutputs();
    int j;

#if ASPLOS2000
    // FIX ME! JUST FOR ASPLOS2000!
    if (numFakeClusters > 0) {
      if (opi->page[i]->group() != NO_GROUP) {
        fakeClusterSpecs[opi->page[i]->group()]->append(currentPage);
      }
    }
#endif

    for (j = 0; j < numOutputs; j++) {
      SCORE_STREAM outStream = currentPage->getSchedOutput(j);

      if (!(outStream->snkFunc == STREAM_OPERATOR_TYPE)) {
	ScoreGraphNode *sinkNode = outStream->sched_sink;

	// make sure we optimize only within this current operator!
	if (sinkNode->sched_parentOperator == opi) {
	  operatorGraph.new_edge(currentPage->sched_graphNode, 
				 sinkNode->sched_graphNode);
	}
      }
    }
  }
  for (i = 0; i < opi->segments; i++) {
    ScoreSegment *currentSegment = opi->segment[i];
    int numOutputs = currentSegment->getOutputs();
    int j;

    for (j = 0; j < numOutputs; j++) {
      SCORE_STREAM outStream = currentSegment->getSchedOutput(j);

      if (!(outStream->snkFunc == STREAM_OPERATOR_TYPE)) {
	ScoreGraphNode *sinkNode = outStream->sched_sink;
	
	// make sure we optimize only within this current operator!
	if (sinkNode->sched_parentOperator == opi) {
	  operatorGraph.new_edge(currentSegment->sched_graphNode, 
				 sinkNode->sched_graphNode);
	}
      }
    }
  }

  // run the SCC graph algorithm.
  componentNum = new node_array<int>(operatorGraph);
  numClusters = STRONG_COMPONENTS(operatorGraph, (*componentNum));

  // form the new clusters.
  newClusters = new ScoreCluster*[numClusters];
  for (i = 0; i < numClusters; i++) {
    newClusters[i] = new ScoreCluster();

#if ASPLOS2000
    if (numFakeClusters > 0) {
      newClusters[i]->clusterSpecs = 
	new (list<ScorePage *> *)[numFakeClusters];
      newClusters[i]->numClusterSpecNotDone =
	new unsigned int[numFakeClusters];
    
      for (j = 0; j < numFakeClusters; j++) {
	newClusters[i]->clusterSpecs[j] = NULL;
	newClusters[i]->numClusterSpecNotDone[j] = 0;
      }
    }
#endif
  }
  for (i = 0; i < opi->pages; i++) {
    ScorePage *currentPage = opi->page[i];
    int clusterNum = (*componentNum)[currentPage->sched_graphNode];

    newClusters[clusterNum]->addNode(currentPage);
  }
  for (i = 0; i < opi->segments; i++) {
    ScoreSegment *currentSegment = opi->segment[i];
    int clusterNum = (*componentNum)[currentSegment->sched_graphNode];

    newClusters[clusterNum]->addNode(currentSegment);
  }

  // Check the physical requirements of all clusters.
  // Extract the clusters which have excessive physical page/memory segment
  // requirements and redo them so that they do not require as much
  // resources.
  // FIX ME! Right now, we will just break them into 1-node clusters.
  //         However, this may not always be the best solution. It could
  //         actually make memory segment requirements worse!
  abortOperatorInsert = 0;
  for (i = 0; i < numClusters; i++) {
    if ((newClusters[i]->getNumPagesRequired() > numPhysicalCP) ||
	(newClusters[i]->getNumMemSegRequired() > numPhysicalCMB)) {
      unsigned int j;

      for (j = 0; j < SCORECUSTOMLIST_LENGTH(newClusters[i]->nodeList); j++) {
	ScoreGraphNode *currentNode;
	ScoreCluster *newSingleNodeCluster = new ScoreCluster();

	SCORECUSTOMLIST_ITEMAT(newClusters[i]->nodeList, j, currentNode);

	newSingleNodeCluster->addNode(currentNode);

#if ASPLOS2000
	if (numFakeClusters > 0) {
	  newSingleNodeCluster->clusterSpecs = 
	    new (list<ScorePage *> *)[numFakeClusters];
	  newSingleNodeCluster->numClusterSpecNotDone =
	    new unsigned int[numFakeClusters];
	  
	  for (j = 0; j < numFakeClusters; j++) {
	    newSingleNodeCluster->clusterSpecs[j] = NULL;
	    newSingleNodeCluster->numClusterSpecNotDone[j] = 0;
	  }
	}
#endif
	// make sure the new cluster is valid. if not, then give up!
	// we currently do not know how to handle this condition!
	// if it is valid, put it on the cluster list.
	if ((newSingleNodeCluster->getNumPagesRequired() > numPhysicalCP) ||
	    (newSingleNodeCluster->getNumMemSegRequired() > numPhysicalCMB)) {
	  if (!abortOperatorInsert) {
	    cerr << "SCHEDERR: Insufficient physical resources! (operator=" <<
	      sharedObject << ")" << endl;

	    abortOperatorInsert = 1;
	  }

	  delete newSingleNodeCluster;
	} else {
	  clusterList.append(newSingleNodeCluster);
	}
      }

      delete newClusters[i];
      newClusters[i] = NULL;
    } else {
      clusterList.append(newClusters[i]);
    }
  }

#if ASPLOS2000
  // FIX ME! JUST FOR ASPLOS2000!
  for (i = 0; i < numFakeClusters; i++) {
    list<ScorePage *> *fakeSpec;
    ScorePage *basePage;
    ScoreCluster *baseCluster;

    fakeSpec = fakeClusterSpecs[i];

    if (fakeSpec->length() > 1) {
      basePage = fakeSpec->pop();
      baseCluster = basePage->sched_parentCluster;

      forall_items(listItem, *fakeSpec) {
	ScorePage *currentPage = fakeSpec->inf(listItem);
	ScoreCluster *currentCluster = currentPage->sched_parentCluster;
	
	if (currentCluster != baseCluster) {
	  unsigned int j;

	  for (j = 0; j < SCORECUSTOMLIST_LENGTH(currentCluster->nodeList);
	       j++) {
	    ScoreGraphNode *currentNode;

            SCORECUSTOMLIST_ITEMAT(currentCluster->nodeList, j, currentNode);
	    
	    baseCluster->addNode(currentNode);
	  }
	  
	  clusterList.remove(currentCluster);
	  
	  delete(currentCluster);
	}
      }

      fakeSpec->push(basePage);
    }
  }
  // pad with dummy pages and register the pages according to fakeSpec.
  for (i = 0; i < numFakeClusters; i++) {
    list<ScorePage *> *fakeSpec;
    ScorePage *basePage;
    ScoreCluster *baseCluster;
    list<ScorePage *> *clusterSpec;

    fakeSpec = fakeClusterSpecs[i];

    if (fakeSpec->length() > 0) {
      unsigned int numDummyPagesToAdd;

      if (((unsigned int) fakeSpec->length()) < SCORE_NUM_PAGENODES_IN_CP) {
	numDummyPagesToAdd = 
	  SCORE_NUM_PAGENODES_IN_CP - fakeSpec->length();
      } else { 
	numDummyPagesToAdd = 0;
      }

      basePage = fakeSpec->front();
      baseCluster = basePage->sched_parentCluster;

      if ((baseCluster->clusterSpecs)[i] == NULL) {
	(baseCluster->clusterSpecs)[i] = new list<ScorePage *>;
      }

      clusterSpec = baseCluster->clusterSpecs[i];

      forall_items(listItem, *fakeSpec) {
	ScorePage *currentPage = fakeSpec->inf(listItem);

	clusterSpec->append(currentPage);
      }

      for (j = 0; j < numDummyPagesToAdd; j++) {
	ScorePage *dummyPage = new ScoreDummyDonePage();

	dummyPage->setGroup(i);
	dummyPageList.append(dummyPage);
	baseCluster->addNode(dummyPage);
	clusterSpec->append(dummyPage);

	dummyPage->sched_isDone = 0;
	dummyPage->sched_isResident = 0;
	dummyPage->sched_isScheduled = 0;
	dummyPage->sched_residentLoc = 0;
	dummyPage->sched_fifoBuffer = malloc(SCORE_PAGEFIFO_SIZE);
	dummyPage->sched_lastKnownState = 0;
	dummyPage->sched_potentiallyDidNotFireLastResident = 0;
      }

      baseCluster->numClusterSpecNotDone[i] = fakeSpec->length() +
	numDummyPagesToAdd;
    }
  }

  if (numFakeClusters > 0) {
    for (i = 0; i < numFakeClusters; i++) {
      delete(fakeClusterSpecs[i]);
    }

    delete(fakeClusterSpecs);
  }

  forall_items(listItem, clusterList) {
    ScoreCluster *currentCluster = clusterList.inf(listItem);

    if ((currentCluster->getNumPagesRequired() > numPhysicalCP) ||
	(currentCluster->getNumMemSegRequired() > numPhysicalCMB)) {
      cerr << "FOR ASPLOS2000, WE CANNOT AFFORD TO BREAK CLUSTERS INTO " <<
	"SINGLE NODE CLUSTERS!" << endl;
      abortOperatorInsert = 1;
    }
  }
#endif

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
    process->addOperator(opi);

#if ASPLOS2000
    forall_items(listItem, dummyPageList) {
      ScorePage *dummyPage = dummyPageList.inf(listItem);

      dummyPage->sched_parentOperator = opi;
      dummyPage->sched_parentProcess = process;
      SCORECUSTOMLIST_APPEND(process->nodeList, dummyPage);
    }

    opi->sched_livePages = opi->sched_livePages + dummyPageList.length();
    process->numPages = process->numPages + dummyPageList.length();
#endif

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
    SCORECUSTOMLIST_CLEAR(clusterListToAddProcessorStitch);
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
	  SCORECUSTOMLIST_APPEND(clusterListToAddProcessorStitch, currentPage->sched_parentCluster);
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
	  SCORECUSTOMLIST_APPEND(clusterListToAddProcessorStitch, currentPage->sched_parentCluster);
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
	  SCORECUSTOMLIST_APPEND(clusterListToAddProcessorStitch, currentSegment->sched_parentCluster);
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
	  SCORECUSTOMLIST_APPEND(clusterListToAddProcessorStitch, currentSegment->sched_parentCluster);
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

    // add the new clusters to the process and waiting list.
    // FIXME! FOR NOW, WE ARE NOT GOING TO OPTIMIZE ALREADY ENTERED CLUSTERS!
    {
      if (VERBOSEDEBUG || DEBUG || PRINTSTATE) {
	cerr << "SCHED: CLUSTERS========================" << endl;
	forall_items(listItem, clusterList) {
	  ScoreCluster *currentCluster = clusterList.inf(listItem);

	  cerr << "SCHED:    CLUSTER " << i << " (" << 
	    (unsigned int) currentCluster << ")" << endl;
	  for (i = 0; i < SCORECUSTOMLIST_LENGTH(currentCluster->nodeList);
	       i++) {
	    ScoreGraphNode *currentNode;

	    SCORECUSTOMLIST_ITEMAT(currentCluster->nodeList, i, currentNode);

	    cerr << "SCHED:       ";
	    if (currentNode->isPage()) {
	      cerr << "PAGE";
	    } else if (currentNode->isSegment()) {
	      cerr << "SEGMENT";
	    } else {
	      cerr << "UNKNOWN";
	    }
	    cerr << " (" << (unsigned int) currentNode << ")" << endl;
	  }
	}
	cerr << "SCHED: ================================" << endl;
      }

      // now, actually insert cluster into the process and the waiting
      // cluster list (with priorities based on the topological sort).
      forall_items(listItem, clusterList) {
	ScoreCluster *currentCluster = clusterList.inf(listItem);
	char isOnHead = 0;

	process->addCluster(currentCluster);
      
	// check to see if all of the old head list clusters should still
	// be on the head list.
	{
	  list<ScoreCluster *> checkClusterList;
	  
	  for (i = 0; i < SCORECUSTOMLIST_LENGTH(headClusterList); i++) {
	    ScoreCluster *headCluster;

	    SCORECUSTOMLIST_ITEMAT(headClusterList, i, headCluster);

	    headCluster->visited = 0;
	  }

	  for (i = 0; i < SCORECUSTOMLIST_LENGTH(currentCluster->outputList); 
	       i++) {
	    ScoreStream *currentOutput;

	    SCORECUSTOMLIST_ITEMAT(currentCluster->outputList, i, currentOutput);

	    if (!(currentOutput->sched_sinkIsDone) &&
		(currentOutput->sched_snkFunc == STREAM_SEGMENT_TYPE) &&
		((ScoreSegment *) 
		 (currentOutput->sched_sink))->sched_isStitch) {
	      ScoreSegmentStitch *currentStitch =
		(ScoreSegmentStitch *) currentOutput->sched_sink;
	      
	      currentOutput = currentStitch->getSchedOutStream();
	    }
	    
	    if (!(currentOutput->sched_sinkIsDone) &&
		(currentOutput->sched_snkFunc != STREAM_OPERATOR_TYPE)) {
	      ScoreCluster *outputCluster = 
		currentOutput->sched_sink->sched_parentCluster;
	      
	      if (outputCluster->isHead && !(outputCluster->visited)) {
		outputCluster->visited = 1;
		checkClusterList.append(outputCluster);
	      }
	    }
	  }

	  forall_items(listItem2, checkClusterList) {
	    ScoreCluster *checkCluster = checkClusterList.inf(listItem2);
	    char stillOnHead = 0;

	    for (i = 0; i < SCORECUSTOMLIST_LENGTH(checkCluster->inputList);
		 i++) {
	      ScoreStream *checkInput;

              SCORECUSTOMLIST_ITEMAT(checkCluster->inputList, i, checkInput);

	      if (!(checkInput->sched_srcIsDone) &&
		  (checkInput->sched_srcFunc == STREAM_SEGMENT_TYPE) &&
		  ((ScoreSegment *) (checkInput->sched_src))->sched_isStitch) {
		ScoreSegmentStitch *checkStitch = 
		  (ScoreSegmentStitch *) (checkInput->sched_src);

		checkInput = checkStitch->getSchedInStream();
	      }

	      if (checkInput->sched_srcFunc == STREAM_OPERATOR_TYPE) {
		stillOnHead = 1;
	      }
	    }

	    if (!stillOnHead) {
	      SCORECUSTOMLIST_REMOVE(headClusterList, checkCluster);
	      checkCluster->isHead = 0;
	    }
	  }
	}

        if (SCORECUSTOMLIST_LENGTH(currentCluster->inputList) == 0) {
          isOnHead = 1;
        } else {
	  for (i = 0; i < SCORECUSTOMLIST_LENGTH(currentCluster->inputList);
	       i++) {
            ScoreStream *currentInput;

            SCORECUSTOMLIST_ITEMAT(currentCluster->inputList, i, currentInput);

	    if (!(currentInput->sched_srcIsDone) &&
		(currentInput->sched_srcFunc == STREAM_SEGMENT_TYPE) &&
		((ScoreSegment *) (currentInput->sched_src))->sched_isStitch) {
	      ScoreSegmentStitch *currentStitch =
		(ScoreSegmentStitch *) (currentInput->sched_src);

	      currentInput = currentStitch->getSchedInStream();
	    }

	    if (currentInput->sched_srcIsDone ||
		(currentInput->sched_srcFunc == STREAM_OPERATOR_TYPE)) {
              isOnHead = 1;
            }
          }
        }

	SCORECUSTOMLIST_APPEND(waitingClusterList, currentCluster);

	if (isOnHead) {
	  SCORECUSTOMLIST_APPEND(headClusterList, currentCluster);
	  currentCluster->isHead = 1;
	}
      }
    }

#if 1
    // modify the clusters that output to the cpu by adding a stitch buffer
    // between cpu and them
    for (unsigned int listIndex = 0;
	 listIndex < SCORECUSTOMLIST_LENGTH(clusterListToAddProcessorStitch);
	 listIndex ++) {
      ScoreCluster *currentCluster;
      SCORECUSTOMLIST_ITEMAT(clusterListToAddProcessorStitch, listIndex, currentCluster);
      if (!currentCluster->sched_stitchAdded) {
	for (unsigned int i = 0;
	     i < SCORECUSTOMLIST_LENGTH(currentCluster->outputList); i++) {
	  SCORE_STREAM currentStream;
	  SCORECUSTOMLIST_ITEMAT(currentCluster->outputList, i, currentStream);
	  if (currentStream->snkFunc == STREAM_OPERATOR_TYPE) {
	    ScoreSegmentStitch *stitch = 
	      insertStitchBufferOnOutput(currentCluster, i, currentStream);
	    // set this, so that the stitch is not cleaned up when empty
	    stitch->sched_mustBeInDataFlow = 1;
	    currentCluster->addNode(stitch);
	  }
	}

	for (unsigned int i = 0;
	     i < SCORECUSTOMLIST_LENGTH(currentCluster->inputList); i++) {
	  SCORE_STREAM currentStream;
	  SCORECUSTOMLIST_ITEMAT(currentCluster->inputList, i, currentStream);
	  if (currentStream->srcFunc == STREAM_OPERATOR_TYPE) {
	    ScoreSegmentStitch *stitch = 
	      insertStitchBufferOnInput(currentStream, currentCluster, i);
	    // set this, so that the stitch is not cleaned up when empty
	    stitch->sched_mustBeInDataFlow = 1;
	    currentCluster->addNode(stitch);
	  }
	}
	currentCluster->sched_stitchAdded = 1;
      }
    }
#endif
 
    // clean up.
    delete componentNum;
    componentNum = NULL;
    delete newClusters;
    newClusters = NULL;

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
  } else {
    void *oldHandle = opi->sched_handle;

    // clean up.
    forall_items(listItem, clusterList) {
      ScoreCluster *mynode = clusterList.inf(listItem);

      delete mynode;
    }
    delete componentNum;
    componentNum = NULL;
    delete newClusters;
    newClusters = NULL;
    delete opi;
    opi = NULL;
    dlclose(oldHandle);

    // release the scheduler data mutex.
    pthread_mutex_unlock(&schedulerDataMutex);

    return(-1);
  }
}
