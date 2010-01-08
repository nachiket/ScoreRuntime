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
// $Revision: 1.46 $
//
//////////////////////////////////////////////////////////////////////////////

#include <iostream>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <values.h>
#include <dlfcn.h>
#include "LEDA/core/list.h"
#include "LEDA/core/b_stack.h"
#include "LEDA/graph/graph.h"
//#include "Kenel/graph/basic_graph_alg.h"
//#include "Kernel/graph/graph_alg.h"
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
#include "ScoreSchedulerDynamic.h"
#include "ScoreRuntime.h"
#include "ScoreSimulator.h"
#include "ScoreConfig.h"
#include "ScoreStateGraph.h"
#include "ScoreDummyDonePage.h"
#include "ScoreCustomStack.h"
#include "ScoreCustomList.h"
#include "ScoreCustomQueue.h"
#include "ScoreCustomLinkedList.h"

using leda::graph;
using leda::node_array;
using leda::edge_array;

#include "ScoreProfiler.h"

#if SCHEDULECLUSTERS_FOR_SCHEDTIME && DOSCHEDULE_FOR_SCHEDTIME
#error "SCHEDULECLUSTERS_FOR_SCHEDTIME && DOSCHEDULE_FOR_SCHEDTIME can not be both enabled";
#endif

#if DOPROFILING && DOPROFILING_SCHEDULECLUSTERS
#error "DOPROFILING && DOPROFILING_SCHEDULECLUSTERS can not be both enabled";
#endif

#if (SCHEDULECLUSTERS_FOR_SCHEDTIME || DOSCHEDULE_FOR_SCHEDTIME) && !DOPROFILING
#error "(SCHEDULECLUSTERS_FOR_SCHEDTIME || DOSCHEDULE_FOR_SCHEDTIME) && !DOPROFILING failed"
#endif

#if GET_FEEDBACK
#include "ScoreFeedbackGraph.h"
extern ScoreFeedbackMode gFeedbackMode;
extern FILE *schedTemplateFile;
void makeSchedTemplate(FILE *f, ScoreFeedbackGraph *feedbackGraph);
#endif




extern bool outputNetlist;
extern bool uniqNetlistRes;
extern char* netlistDirName;


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


#if DOPROFILING || KEEPRECONFIGSTATISTICS
#include <asm/msr.h>

unsigned long long startClock, endClock, diffClock;

#undef DECLARE
#define DECLARE(__a__)   PROF_ ## __a__,

enum {
#include "do_prof_categories.inc"
};

enum {
#include "aux_stat_categories.inc"
};

enum {
#include "reconfig_stat_prof.inc"
};

enum {
#include "sched_clusters_stat_prof.inc"
};

#undef DECLARE
#define DECLARE(__a__)     # __a__ ,

const char *do_prof_cat_str[] = {
#include "do_prof_categories.inc"
};

const size_t do_prof_cat_count = 
   sizeof (do_prof_cat_str) / sizeof (do_prof_cat_str[0]);

const char *aux_stat_cat_str[] = {
#include "aux_stat_categories.inc"
};

const size_t aux_stat_cat_count = 
   sizeof (aux_stat_cat_str) / sizeof (aux_stat_cat_str[0]);

const char *reconfig_stat_prof_str[] = {
#include "reconfig_stat_prof.inc"
};

const size_t reconfig_stat_prof_count = 
   sizeof (reconfig_stat_prof_str) / sizeof (reconfig_stat_prof_str[0]);

const char *sched_clusters_stat_prof_str[] = {
#include "sched_clusters_stat_prof.inc"
};

const size_t sched_clusters_stat_prof_count = 
   sizeof (sched_clusters_stat_prof_str) / 
     sizeof (sched_clusters_stat_prof_str[0]);

#endif


// a pointer to the visualFile where state graphs are written
extern char *visualFile;

// Added by Nachiket.. LEDA is bizarre
using leda::b_stack;
using leda::edge;

static void scc_dfs(const graph& G, node v, node_array<int>& compnum,
                                            b_stack<node>& unfinished,
                                            b_stack<int>& roots,
                                            int& count1, int& count2 )
{ int cv = --count1;
  compnum[v] = cv;
  unfinished.push(v);
  roots.push(cv);

  edge e;
  forall_out_edges(e,v)
  { node w =G.target(e);
    int cw = compnum[w];
    if (cw == -1)
      scc_dfs(G,w,compnum,unfinished,roots,count1,count2);
    else
      if (cw < -1)
         while (roots.top() < cw)  roots.pop();
   }

  if (roots.top() == cv)
  { node u;
    do { u = unfinished.pop(); // u is a node of the scc with root v
         compnum[u] = count2;
        } while (v != u);
    roots.pop();
    count2++;
   }
}




int STRONG_COMPONENTS(const graph& G, node_array<int>& compnum)
{
  int n = G.number_of_nodes();

  b_stack<int>  roots(n);
  b_stack<node> unfinished(n);

  int count1 = -1;
  int count2 = 0;

  node v;
  forall_nodes(v,G) compnum[v] = -1;
    forall_nodes(v,G)
      if (compnum[v] == -1)
        scc_dfs(G,v,compnum,unfinished,roots,count1,count2);
        return count2;
}


///////////////////////////////////////////////////////////////////////////////
// ScoreSchedulerDynamic::ScoreSchedulerDynamic:
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
ScoreSchedulerDynamic::ScoreSchedulerDynamic(char exitOnIdle, char noDeadlockDetection,
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
    new ScoreCustomLinkedList<unsigned int>
    (SCORE_SCHEDULERUNUSEDPHYSICALCMBS_BOUND);
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
  
#if FRONTIERLIST_USEPRIORITY
  frontierClusterList = 
    new ScoreCustomPriorityList<ScoreCluster *>
    (SCORE_SCHEDULERFRONTIERCLUSTERLIST_BOUND);
  reprioritizeClusterArray = 
    new (ScoreCluster *)[SCORE_SCHEDULERFRONTIERCLUSTERLIST_BOUND+1];
#else
  frontierClusterList = 
    new ScoreCustomLinkedList<ScoreCluster *>
    (SCORE_SCHEDULERFRONTIERCLUSTERLIST_BOUND);
#endif

  // fill up the spare segment stitch and stream stitch lists.
  // NOTE: SEGMENTS ARE INITIALIZED TO POTENTIALLY ACCOMODATE 8 BIT TOKENS!
  spareSegmentStitchList = 
    new ScoreCustomStack<ScoreSegmentStitch *>
    (SCORE_SPARESEGMENTSTITCHLIST_BOUND);
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
      new ScoreStreamStitch(64 /* FIX ME! */, 0,
			    ARRAY_FIFO_SIZE, 
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
    new ScoreCustomLinkedList<ScoreCluster *>(SCORE_SCHEDULERWAITINGCLUSTERLIST_BOUND);
  residentClusterList =
    new ScoreCustomLinkedList<ScoreCluster *>(SCORE_SCHEDULERRESIDENTCLUSTERLIST_BOUND);
  headClusterList =
    new ScoreCustomList<ScoreCluster *>(SCORE_SCHEDULERHEADCLUSTERLIST_BOUND);

  processorIStreamList =
    new ScoreCustomList<ScoreStream *>(SCORE_SCHEDULERPROCESSORISTREAMLIST_BOUND);
  processorOStreamList =
    new ScoreCustomList<ScoreStream *>(SCORE_SCHEDULERPROCESSOROSTREAMLIST_BOUND);

  doneNodeList = 
    new ScoreCustomList<ScoreGraphNode *>(SCORE_SCHEDULERDONENODELIST_BOUND);
  freeableClusterList =
    new ScoreCustomList<ScoreCluster *>(SCORE_SCHEDULERFREEABLECLUSTERLIST_BOUND);
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

  numFreePageTrial =
    new ScoreCustomStack<int>(SCORE_SCHEDULERNUMFREEPAGETRIAL_BOUND);
  numFreeMemSegTrial =
    new ScoreCustomStack<int>(SCORE_SCHEDULERNUMFREEMEMSEGTRIAL_BOUND);
  traversalTrial =
    new ScoreCustomStack<int>(SCORE_SCHEDULERTRAVERSALTRIAL_BOUND);
  scheduledClusterTrial =
    new ScoreCustomStack<ScoreCluster *>(SCORE_SCHEDULERSCHEDULEDCLUSTERTRIAL_BOUND);
  frontierClusterTrial =
    new ScoreCustomStack<ScoreCluster *>(SCORE_SCHEDULERFRONTIERCLUSTERTRIAL_BOUND);

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


#if DOPROFILING
  crit_loop_prof = new ScoreProfiler(do_prof_cat_count, do_prof_cat_str,
				     false,
				     "DoSchedule");
  aux_stat_prof = new ScoreProfiler(aux_stat_cat_count, aux_stat_cat_str);
#endif

#if DOPROFILING_SCHEDULECLUSTERS

  sched_clusters_stat_prof = new ScoreProfiler(sched_clusters_stat_prof_count,
					       sched_clusters_stat_prof_str,
					       true, /* add per item acct */
					       "ScheduleClusters");
#endif


#if KEEPRECONFIGSTATISTICS
  reconfig_stat_prof = new ScoreProfiler(reconfig_stat_prof_count,
					 reconfig_stat_prof_str);
#endif
}


///////////////////////////////////////////////////////////////////////////////
// ScoreSchedulerDynamic::~ScoreSchedulerDynamic:
//   Destructor for the scheduler.
//   Cleans up all internal structures.
//
// Parameters: None.
//
// Return value: None.
///////////////////////////////////////////////////////////////////////////////
ScoreSchedulerDynamic::~ScoreSchedulerDynamic() {
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

  delete(frontierClusterList);
#if FRONTIERLIST_USEPRIORITY
  delete(reprioritizeClusterArray);
#endif
  delete(waitingClusterList);
  delete(residentClusterList);
  delete(headClusterList);

  delete(processorIStreamList);
  delete(processorOStreamList);

  delete(doneNodeList);
  delete(freeableClusterList);
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

  delete(numFreePageTrial);
  delete(numFreeMemSegTrial);
  delete(traversalTrial);
  delete(scheduledClusterTrial);

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
}




///////////////////////////////////////////////////////////////////////////////
// ScoreSchedulerDynamic::addOperator:
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
int ScoreSchedulerDynamic::addOperator(char *sharedObject, char *argbuf, pid_t pid) {
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

  list<ScoreStream*> *expandStreamList = 0;


  // get a lock on the scheduler data mutex.
  pthread_mutex_lock(&schedulerDataMutex);

#if DOPROFILING
  startClock = threadCounter->read_tsc();
#endif

  // try to get function pointer.
  {
    void *newHandle = dlopen(sharedObject, RTLD_LAZY);

    if (newHandle != NULL) {
      construct = (construct_t) dlsym(newHandle, SCORE_CONSTRUCT_NAME);
      char *error = dlerror();

      if (error == NULL) {
 	// obtain the list of streams that need to expand
	list<ScoreStream*> **tmp =
	  (list<ScoreStream*>**) dlsym(newHandle, "streamsToExpand");
	char *expandListError = dlerror();
	if (expandListError == NULL) {
	  expandStreamList = *tmp;
	  
	  //ScoreStream::streamsToExpand->clear();
	  
	  // instantiate an instance of the operator.
	  opi = (*construct)(argbuf);
	  
	  // store the handle.
	  opi->sched_handle = newHandle;
	} else {
	  cerr << "could not find expand stream list in " <<
	    sharedObject << " (dlerror = " << expandListError << ")\n";
	  return -1;
	}
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
	(long) opi->page[i] << endl;
      
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
	cerr << " " << (long) opi->page[i]->getSchedInput(j) << endl;
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
	cerr << " " << (long) opi->page[i]->getSchedOutput(j) << endl;
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
	cerr << " " << (long) opi->segment[i]->getSchedInput(j)
	     << endl;
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
	cerr << (long) opi->segment[i]->getSchedOutput(j) << endl;
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
    fakeClusterSpecs = new list<ScorePage *> *[numFakeClusters];
    
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
	new list<ScorePage *> *[numFakeClusters];
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
	    new list<ScorePage *> *[numFakeClusters];
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
	    
	    cerr << "SCHEDERR: The cluster that caused the problem contains:\n";
	    for (unsigned int listIndex = 0; 
		 listIndex < SCORECUSTOMLIST_LENGTH(newSingleNodeCluster->nodeList);
		 listIndex ++) {
	      cerr << "SCHEDERR: ";
	      ScoreGraphNode *node;
	      SCORECUSTOMLIST_ITEMAT(newSingleNodeCluster->nodeList, listIndex,
				     node);
	      if (node->isPage()) {
		ScorePage *page = (ScorePage*) node;
		cerr << "Page: \'" << page->getSource() << "\' numInputs = " <<
		  page->getInputs() << " numOutputs = " <<
		  page->getOutputs() << endl;
	      } else {
		ScoreSegment *segment = (ScoreSegment *) node;
		cerr << "Segment: numInputs = " << segment->getInputs() <<
		  " numOutputs = " << segment->getOutputs() << endl;
	      }
	    }
	    cerr << "SCHEDERR: The cluster requires " << 
	      newSingleNodeCluster->getNumPagesRequired() << " CPs and " <<
	      newSingleNodeCluster->getNumMemSegRequired() << " CMBs" << endl;
	    cerr << "SCHEDERR: Array size: " << numPhysicalCP << " CPs and " <<
	      numPhysicalCMB << " CMBs" << endl;

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

#if GET_FEEDBACK
    if (schedTemplateFile) {
      makeSchedTemplate(schedTemplateFile, process->feedbackGraph);
      fclose(schedTemplateFile);
      exit(0);
    }
#endif

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
      ScoreIOMaskType currentConsumed = 
	currentPage->inputs_consumed(currentState);
      ScoreIOMaskType currentProduced = 
	currentPage->outputs_produced(currentState);
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
	  (SCORE_DATASEGMENTBLOCK_LOADSIZE /
	   (SCORE_ALIGN_BITWIDTH_TO_8(currentSegment->width())/8))) {
	currentSegment->sched_maxAddr = 
          SCORE_DATASEGMENTBLOCK_LOADSIZE / 
	  (SCORE_ALIGN_BITWIDTH_TO_8(currentSegment->width())/8);
	currentSegment->sched_residentLength = 
          SCORE_DATASEGMENTBLOCK_LOADSIZE /
	  (SCORE_ALIGN_BITWIDTH_TO_8(currentSegment->width())/8);
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

    // add the new clusters to the process and waiting list.
    // FIXME! FOR NOW, WE ARE NOT GOING TO OPTIMIZE ALREADY ENTERED CLUSTERS!
    {
      if (VERBOSEDEBUG || DEBUG || PRINTSTATE) {
	cerr << "SCHED: CLUSTERS========================" << endl;
	forall_items(listItem, clusterList) {
	  ScoreCluster *currentCluster = clusterList.inf(listItem);

	  cerr << "SCHED:    CLUSTER " << i << " (" << 
	    (long) currentCluster << ")" << endl;
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
	    cerr << " (" << (long) currentNode << ")" << endl;
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

	    SCORECUSTOMLIST_ITEMAT(currentCluster->outputList, i,
				   currentOutput);

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

	SCORECUSTOMLINKEDLIST_APPEND(waitingClusterList, currentCluster, 
				     currentCluster->clusterWaitingListItem);

	if (isOnHead) {
	  SCORECUSTOMLIST_APPEND(headClusterList, currentCluster);
	  currentCluster->isHead = 1;
	}
      }
    }

    if (expandStreamList->size() > 0) {
      resolveBufferLockedStreams(expandStreamList);
    }
    
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
    endClock = threadCounter->read_tsc();
    diffClock = endClock - startClock;
    cerr << "   addOperator() ==> " << diffClock << " cycle(s)" << endl;
#endif

    // release the scheduler data mutex.
    pthread_mutex_unlock(&schedulerDataMutex);

    return(0);
  } else {
    void *oldHandle = opi->sched_handle;

    // clean up.
    forall_items(listItem, clusterList) {
      ScoreCluster *node = clusterList.inf(listItem);

      delete node;
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


///////////////////////////////////////////////////////////////////////////////
// ScoreSchedulerDynamic::doSchedule:
//   Perform scheduling of the array.
//
// Parameters: none.
//
// Return value: none.
///////////////////////////////////////////////////////////////////////////////
void ScoreSchedulerDynamic::doSchedule() {
  unsigned int i;
  unsigned long long doScheduleDiffClock;

#if DOPROFILING
  unsigned int reconfigTimeStart, reconfigTimeEnd = 0;
#endif

#if SCHED_VIRT_TIME_BREAKDOWN
static unsigned int
  doSchedule_begin_schedulerVirtualTime,
  beforeReconfig_schedulerVirtualTime,
  afterReconfig_schedulerVirtualTime;
#endif

  // get a lock on the scheduler data mutex.
  pthread_mutex_lock(&schedulerDataMutex);

  if (VISUALIZE_STATE)
    visualizeCurrentState(currentTimeslice);

  currentTimeslice++;

  if (VERBOSEDEBUG || DEBUG || DOPROFILING || PRINTSTATE) {
    if (!isPseudoIdeal) {
      cerr << "-----------------------------------------------------------\n";
      cerr << "SCHED: begin doSchedule: TS: "
	   << currentTimeslice << endl;
      cerr << "SCHED: TIME: " << schedulerVirtualTime 
	   << "( + "
	   << (schedulerVirtualTime - afterReconfig_schedulerVirtualTime)
	   << " exec time)" << endl;
      doSchedule_begin_schedulerVirtualTime = schedulerVirtualTime;
    } else {
      if ((schedulerVirtualTime % 100000) == 0) {
        cerr << "SCHED: SCHEDULER VIRTUAL TIME: " << schedulerVirtualTime
	     << endl;
      }
    }
  }


  // check to see if the scheduler was reawakened (simulation only).
  if (isReawakening) {
    if (VERBOSEDEBUG || DEBUG) {
      cerr << "SCHED: SCHEDULER REAWOKEN AT CYCLE = " 
	   << schedulerVirtualTime << endl;
    }
    lastReawakenTime = schedulerVirtualTime;

    isReawakening = 0;
  }
  
  // get the current status of the array.
  getCurrentStatus();

  // gather status info for the array and store in scheduler data structures.
  gatherStatusInfo();

  // find the done pages/segments.
  findDonePagesSegments();

  if (VERBOSEDEBUG || DEBUG || PRINTSTATE) {
    printGraphNodeCustomList("NUMBER OF DONE NODES", doneNodeList);
  }

  // find any memory segments that have faulted on their address.
  findFaultedMemSeg();

  if (VERBOSEDEBUG || DEBUG || PRINTSTATE) {
    printFaultedMemSeg(faultedMemSegList);
  }

  // find the freeable clusters.
  findFreeableClusters();

  if (VERBOSEDEBUG || DEBUG || PRINTSTATE) {
    printClusterCustomList("NUMBER OF FREEABLE CLUSTERS:",
			   freeableClusterList);
  }

  // deal with any potential bufferlock.

  if (!doNoDeadlockDetection) {
    dealWithDeadLock();
  }

  if (VERBOSEDEBUG || DEBUG || PRINTSTATE) {
    printSchedStateBeforeScheduleClusters(headClusterList,
					  frontierClusterList,
					  waitingClusterList);
  }

  // perform cluster scheduling.
  scheduleClusters();

  if (VERBOSEDEBUG || DEBUG || PRINTSTATE) {
    printSchedStateAfterScheduleClusters(frontierClusterList,
					 waitingClusterList,
					 scheduledPageList,
					 scheduledMemSegList,
					 removedPageList,
					 removedMemSegList);
  }

  // place the scheduled pages/memory segments.
  performPlacement();

  // --- compute the overhead of scheduling operations -
  //     store it in doScheduleDiffClock

#if !NO_OVERHEAD
 #if SCHEDULECLUSTERS_FOR_SCHEDTIME
  doScheduleDiffClock = crit_loop_prof->lastVal(PROF_scheduleClusters);
 #elif DOSCHEDULE_FOR_SCHEDTIME
  doScheduleDiffClock = crit_loop_prof->aggregate();
 #else
  // FIX ME! THIS IS A TEMPORARY WORKAROUND FOR SUBSTITUTING IN A SCHEDULER
  //         DECISION TIME! THIS IS NECESSARILY INCORRECT!
  doScheduleDiffClock = SCORE_FAKE_SCHEDULER_TIME;
 #endif
#else /* NO_OVERHEAD */
  doScheduleDiffClock = 0;
#endif


#if PARALLEL_TIME
  advanceSimulatorTimeOffset(doScheduleDiffClock);
#endif

#if SCHED_VIRT_TIME_BREAKDOWN

#if SCHED_VIRT_TIME_BREAKDOWN_VERBOSE
  cerr << "SCHED: TIME BEFORE RECONFIG: " << schedulerVirtualTime 
#if !PARALLEL_TIME
       << "( +-> "
#else  
       << "( + " 
#endif
       << doScheduleDiffClock << " sched ovhd)" << endl;
#endif  

  beforeReconfig_schedulerVirtualTime = schedulerVirtualTime;
#endif

#if DOPROFILING

#if PARALLEL_TIME
 reconfigTimeStart = schedulerVirtualTime;
#else 
 reconfigTimeStart = schedulerVirtualTime + doScheduleDiffClock;
#endif

#endif

  // issue the commands necessary to reconfigure the array.
  issueReconfigCommands(doScheduleDiffClock);


#if DOPROFILING
 reconfigTimeEnd = schedulerVirtualTime;
#endif


#if SCHED_VIRT_TIME_BREAKDOWN

#if SCHED_VIRT_TIME_BREAKDOWN_VERBOSE
  cerr << "SCHED: TIME AFTER RECONFIG: " << schedulerVirtualTime 
       << " ( + " 
#if PARALLEL_TIME
       << (schedulerVirtualTime - beforeReconfig_schedulerVirtualTime)
#else 
       << (schedulerVirtualTime - 
	   (beforeReconfig_schedulerVirtualTime + doScheduleDiffClock))
#endif
       << " reconf ovhd) " << endl;

  if (0) {
    cerr << "PARALLEL_TIME = " << PARALLEL_TIME << endl;
    cerr << "schedulerVirtualTime = " << schedulerVirtualTime << endl;
    cerr << "beforeReconfig..     = " <<
      beforeReconfig_schedulerVirtualTime << endl;
    cerr << "doScheduleDiffClock  = " << doScheduleDiffClock << endl; 
  }

#endif
  
  afterReconfig_schedulerVirtualTime = schedulerVirtualTime;
#endif

  if (VERBOSEDEBUG || DEBUG || PRINTSTATE) {
    printStitchCustomList("NUMBER OF EMPTY STITCH SEGMENTS", emptyStitchList);
  }

  // performs any cleanup necessary (i.e. synchronize memory segments with
  // the user process and deleting any necessary processes or clusters).
  performCleanup();

  // this is used for detailed testing... prints out the view of the world
  // so far and the status of the pages/segments/streams.
  if (VISUALIZE_STATE) {
    // second arg: output netlist at this point
    visualizeCurrentState(currentTimeslice-1, true); 
  }

  if (VERBOSEDEBUG || PRINTSTATE) {
    printCurrentState();
    printArrayState(arrayCP, numPhysicalCP, arrayCMB, numPhysicalCMB);
  }

  // if there is no more work to do, then do not send a timeslice request.
  // otherwise, inform simulator that it can execute up to next timeslice.
  if ((SCORECUSTOMLIST_LENGTH(processList) == 0) &&
      (SCORECUSTOMLIST_LENGTH(stitchBufferList) == 0)) {
    isIdle = 1;

#if RECONFIG_ACCT || GET_FEEDBACK
    {
      ScoreSyncEvent *ev = new ScoreSyncEvent;
      ev->command = SCORE_EVENT_IDLE;
      ev->currentTime = schedulerVirtualTime;

      STREAM_WRITE_RT(toSimulator, ev);

      STREAM_READ_RT(fromSimulator,ev);
    }
#endif

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

#if DOPROFILING
  reconfigTimeEnd -= reconfigTimeStart;

  aux_stat_prof->addSample(currentTimeslice, PROF_reconfigTime,
			   reconfigTimeEnd, DOPROFILING_VERBOSE);

  unsigned int usedCPs = numPhysicalCP - currentNumFreeCPs;
  unsigned int usedCMBs = numPhysicalCMB - currentNumFreeCMBs;

  if (usedCPs || usedCMBs) {
    
    aux_stat_prof->addSample(currentTimeslice, PROF_usedCPs,
			     usedCPs, DOPROFILING_VERBOSE);
    aux_stat_prof->addSample(currentTimeslice, PROF_usedCMBs, 
			     usedCMBs, DOPROFILING_VERBOSE);
  }

#endif

  if (VERBOSEDEBUG || DEBUG || DOPROFILING || PRINTSTATE) {
    if (!isPseudoIdeal) {
      cerr << "SCHED: Ending doSchedule()" << endl;
      cerr << "-----------------------------------------------------------\n";
    }
  }

#if DOPROFILING
  if (isIdle) {
    crit_loop_prof->finishTS(DOPROFILING_VERBOSE);
    aux_stat_prof->finishTS(DOPROFILING_VERBOSE);

    cerr << "SCHED: =================================================" << endl;
    crit_loop_prof->print(cerr);
    cerr << "SCHED: =================================================" << endl;
    aux_stat_prof->print(cerr, false);
    cerr << "SCHED: =================================================" << endl;
  }
#endif

#if DOPROFILING_SCHEDULECLUSTERS
  if (isIdle) {
    cerr << "SCHED: =================================================" << endl;
    sched_clusters_stat_prof->finishTS(DOPROFILING_VERBOSE);
    sched_clusters_stat_prof->print(cerr);
    cerr << "SCHED: =================================================" << endl;
  }
#endif

#if KEEPRECONFIGSTATISTICS
  if (isIdle) {
    cerr << "SCHED: =================================================" << endl;
    reconfig_stat_prof->print(cerr,
			      false, // do not summarize
			      true); // print total only
    cerr << "SCHED: =================================================" << endl;
  }
#endif

  // if we are idle and are to exit when idle, then do so!
  if (isIdle && doExitOnIdle) {
    cerr << "SCHED: EXITING ON IDLE AS REQUESTED!" << endl;
    simulator->printStats();
    pthread_mutex_unlock(&schedulerDataMutex);
    exit(0);
  }

  // release the lock on the scheduler data mutex.
  pthread_mutex_unlock(&schedulerDataMutex);
}




///////////////////////////////////////////////////////////////////////////////
// ScoreSchedulerDynamic::getCurrentTimeslice:
//   Get the current timeslice.
//
// Parameters: none.
//
// Return value: 
//   the current timeslice.
///////////////////////////////////////////////////////////////////////////////
unsigned int ScoreSchedulerDynamic::getCurrentTimeslice() {
  return(currentTimeslice);
}




///////////////////////////////////////////////////////////////////////////////
// ScoreSchedulerDynamic::getCurrentStatus:
//   Get the current status of the physical array.
//
// Parameters: none.
//
// Return value: none.
///////////////////////////////////////////////////////////////////////////////
void ScoreSchedulerDynamic::getCurrentStatus() {
  unsigned int i;
  char *cpMask = NULL;
  char *cmbMask = NULL;

  if (VERBOSEDEBUG || DEBUG) {
    cerr << "SCHED:    getCurrentStatus()" << endl;
  }

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

}




///////////////////////////////////////////////////////////////////////////////
// ScoreSchedulerDynamic::gatherStatusInfo:
//   Given the array status, stores current array status info into scheduler
//     data structures.
//
// Parameters: none.
//
// Return value: none.
///////////////////////////////////////////////////////////////////////////////
void ScoreSchedulerDynamic::gatherStatusInfo() {
  unsigned int i, j;

  if (VERBOSEDEBUG || DEBUG) {
    cerr << "SCHED:    gatherStatusInfo()" << endl;
  }

#if DOPROFILING
  startClock = threadCounter->read_tsc();
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

#if DOPROFILING
  endClock = threadCounter->read_tsc();
  crit_loop_prof->addSample(currentTimeslice, PROF_gatherStatusInfo,
			    endClock - startClock, DOPROFILING_VERBOSE);
#endif
}




///////////////////////////////////////////////////////////////////////////////
// ScoreSchedulerDynamic::findDonePagesSegments:
//   Look at the physical array status and determine which pages/segments are 
//     done. Place all done pages on the done page list.
//
// Parameters: none.
//
// Return value: none.
///////////////////////////////////////////////////////////////////////////////
void ScoreSchedulerDynamic::findDonePagesSegments() {
  unsigned int i, j;

  if (VERBOSEDEBUG || DEBUG) {
    cerr << "SCHED:    findDonePagesSegments()" << endl;
  }

#if DOPROFILING
  startClock = threadCounter->read_tsc();
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
	cerr << "SCHED: EXPLICIT DONE PAGE: " << (long) donePage << 
	  endl;
      }

      // set the done flag on the page.
      // make sure it is not already done!
      if (donePage->sched_isDone) {
#if ASPLOS2000
#else
	cerr << "SCHEDERR: Page at physical location " << i << 
	  " has already " << "signalled done!" << endl;

	return;
#endif
      } else {
#if ASPLOS2000
	if (donePage->group() != NO_GROUP) {
	  ScoreCluster *donePageCluster = donePage->sched_parentCluster;
	  unsigned int donePageGroup = donePage->group();
	  
	  (donePageCluster->numClusterSpecNotDone)[donePageGroup]--;
	}
#endif

	donePage->sched_isDone = 1;

#if ASPLOS2000
	// register the done page with its I/O.
	for (j = 0; j < numInputs; j++) {
	  SCORE_STREAM attachedStream = donePage->getSchedInput(j);
	  
	  attachedStream->sched_sinkIsDone = 1;
	}
	for (j = 0; j < numOutputs; j++) {
	  SCORE_STREAM attachedStream = donePage->getSchedOutput(j);
	  
	  attachedStream->sched_srcIsDone = 1;
	}
#else
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
#endif
	
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
	  (long) doneSegment << endl;
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

#if ASPLOS2000
	// register the segment page with its I/O.
	for (j = 0; j < numInputs; j++) {
	  SCORE_STREAM attachedStream = doneSegment->getSchedInput(j);
	  
	  attachedStream->sched_sinkIsDone = 1;
	}
	for (j = 0; j < numOutputs; j++) {
	  SCORE_STREAM attachedStream = doneSegment->getSchedOutput(j);
	  
	  attachedStream->sched_srcIsDone = 1;
	}
#else
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
#endif
	
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
	    (long) doneSegment << endl;
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
		  (long) attachedNode << endl;
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
	      
#if ASPLOS2000
	      // register the attached node with its I/O.
	      for (j = 0; j < attachedNodeNumInputs; j++) {
		SCORE_STREAM attachedNodeStream = 
		  attachedNode->getSchedInput(j);
		
		attachedNodeStream->sched_sinkIsDone = 1;
	      }
	      for (j = 0; j < attachedNodeNumOutputs; j++) {
		SCORE_STREAM attachedNodeStream = 
		  attachedNode->getSchedOutput(j);
		
		attachedNodeStream->sched_srcIsDone = 1;
	      }
#else
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
#endif
	      
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
		  (long) attachedNode << endl;
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
	      
#if ASPLOS2000
	      // register the attached node with its I/O.
	      for (j = 0; j < attachedNodeNumInputs; j++) {
		SCORE_STREAM attachedNodeStream = 
		  attachedNode->getSchedInput(j);
		
		attachedNodeStream->sched_sinkIsDone = 1;
	      }
	      for (j = 0; j < attachedNodeNumOutputs; j++) {
		SCORE_STREAM attachedNodeStream = 
		  attachedNode->getSchedOutput(j);
		
		attachedNodeStream->sched_srcIsDone = 1;
	      }
#else
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
#endif
	      
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

#if ASPLOS2000
  i = 0;
  while (i < SCORECUSTOMLIST_LENGTH(doneNodeList)) {
    ScoreGraphNode *currentNode;

    SCORECUSTOMLIST_ITEMAT(doneNodeList, i, currentNode);

    if (currentNode->isPage()) {
      ScorePage *currentPage = (ScorePage *) currentNode;
      
      if (currentPage->group() != NO_GROUP) {
	ScoreCluster *currentCluster = currentNode->sched_parentCluster;
	unsigned int currentPageGroup = currentPage->group();

	if ((currentCluster->numClusterSpecNotDone)[currentPageGroup] == 0) {
	  list<ScorePage *> *clusterSpec = 
	    currentCluster->clusterSpecs[currentPageGroup];

	  if (clusterSpec != NULL) {
	    while (!(clusterSpec->empty())) {
	      ScorePage *groupPage = clusterSpec->pop();
	      
	      if (groupPage != currentNode) {
                char foundPage = 0;

                for (j = 0; j < SCORECUSTOMLIST_LENGTH(doneNodeList); j++) {
                  ScoreGraphNode *searchPage;

                  SCORECUSTOMLIST_ITEMAT(doneNodeList, j, searchPage);

                  if (searchPage == groupPage) {
                    foundPage = 1;
                    break;
                  }
                }

		if (!foundPage) {
		  SCORECUSTOMLIST_APPEND(doneNodeList, groupPage);
		}
	      }
	    }

	    delete(clusterSpec);
	    currentCluster->clusterSpecs[currentPageGroup] = NULL;
	  }
	} else {
	  SCORECUSTOMLIST_REMOVEITEMAT(doneNodeList, i);

	  // we do not want to increment the i index.
	  continue;
	}
      }
    }

    i++;
  }
  for (i = 0; i < SCORECUSTOMLIST_LENGTH(doneNodeList); i++) {
    ScoreGraphNode *currentNode;

    SCORECUSTOMLIST_ITEMAT(doneNodeList, i, currentNode);
    unsigned int numInputs = (unsigned int) currentNode->getInputs();
    unsigned int numOutputs = (unsigned int) currentNode->getOutputs();

    // register the attached node with its I/O.
    for (j = 0; j < numInputs; j++) {
      SCORE_STREAM currentStream = currentNode->getSchedInput(j);
      
      currentStream->sched_sinkIsDone = 1;
      // NOTE: DO NOT USE STREAM_UNBIND_SINK SINCE THIS DESTROYS 
      //       SNKFUNC WHICH IS USED TO DETERMINE WHEN TO GC THE 
      //       STREAM!
      currentStream->sched_sink = NULL;
      currentStream->sched_snkNum = -1;
      
      // if the sink of a stream becomes done, that stream can 
      // then no longer be full!
      currentStream->sched_isPotentiallyFull = 0;
    }
    for (j = 0; j < numOutputs; j++) {
      SCORE_STREAM currentStream = currentNode->getSchedOutput(j);
      
      currentStream->sched_srcIsDone = 1;
      // NOTE: DO NOT USE STREAM_UNBIND_SRC SINCE THIS DESTROYS 
      //       SRCFUNC WHICH IS USED TO DETERMINE WHEN TO GC THE 
      //       STREAM!
      currentStream->sched_src = NULL;
      currentStream->sched_srcNum = -1;
    }
  }
#endif

#if DOPROFILING
  endClock = threadCounter->read_tsc();
  crit_loop_prof->addSample(currentTimeslice, PROF_findDonePagesSegments,
			    endClock - startClock, DOPROFILING_VERBOSE);
#endif
}


///////////////////////////////////////////////////////////////////////////////
// ScoreSchedulerDynamic::checkImplicitDonePagesSegments:
//   Check to see if a page/segment is implicitly done. (not explicitly
//     signalling done).
//
// Parameters:
//   currentNode: node to check.
//
// Return value: 0 if false; 1 if true.
///////////////////////////////////////////////////////////////////////////////
int ScoreSchedulerDynamic::checkImplicitDonePagesSegments(ScoreGraphNode 
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
// ScoreSchedulerDynamic::findFaultedMemSeg:
//   Look at the physical array status and determine which memory segments
//     have faulted on their address. Place these memory segments on the
//     faulted segment list.
//
// Parameters: none.
//
// Return value: none.
///////////////////////////////////////////////////////////////////////////////
void ScoreSchedulerDynamic::findFaultedMemSeg() {
  unsigned int i;
  SCORECUSTOMLINKEDLISTITEM listItem;
  
  if (VERBOSEDEBUG || DEBUG) {
    cerr << "SCHED:    findFaultedMemSeg()" << endl;
  }

#if DOPROFILING
  startClock = threadCounter->read_tsc();
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
      ScoreProcess *parentProcess = faultedSegment->sched_parentProcess;

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
	  
	  // FIX ME! SHOULD PROBABLY HAVE A BETTER POLICY!
	  // mark any resident clusters from the parent process of the faulted
	  //   segment as unfreeable in this timeslice.
	  SCORECUSTOMLINKEDLIST_HEAD(residentClusterList, listItem);
	  while (listItem != SCORECUSTOMLINKEDLIST_NULL) {
	    ScoreCluster *currentCluster;

	    SCORECUSTOMLINKEDLIST_ITEMAT(residentClusterList, 
					 listItem, currentCluster);
	    
	    if (currentCluster->parentProcess == parentProcess) {
	      currentCluster->shouldNotBeFreed = 1;
	    }

	    SCORECUSTOMLINKEDLIST_GOTONEXT(residentClusterList, listItem);
	  }
	} else {
	  cerr << "SCHEDERR: Segmentation fault at physical CMB location " <<
	    i << "! (addr=" << cmbStatus[i].faultedAddr << ") " << 
	    "(length=" << arrayCMB[i].active->length() << ")" << endl;
	  arrayCMB[i].active->print(stderr);

	  // FIX ME! WOULD LIKE TO BE ABLE TO KILL THE PROCESS WITHOUT KILLING
	  // THE RUNTIME SYSTEM! THIS IS ALL I CAN DO FOR NOW SO THAT THE
	  // USER SEES THE SEGFAULT!
	  exit(1);
	}
      }
    }
  }

#if DOPROFILING
  endClock = threadCounter->read_tsc();
  crit_loop_prof->addSample(currentTimeslice, PROF_findFaultedMemSeg,
			    endClock - startClock, DOPROFILING_VERBOSE);
#endif
}


///////////////////////////////////////////////////////////////////////////////
// ScoreSchedulerDynamic::findFreeableClusters:
//   Look at the physical array status and determine which clusters are
//     freeable. Place all freeable clusters on the freeable cluster list.
//
// Parameters: none.
//
// Return value: none.
///////////////////////////////////////////////////////////////////////////////
void ScoreSchedulerDynamic::findFreeableClusters() {
  SCORECUSTOMLINKEDLISTITEM listItem;
  unsigned int i;

  if (VERBOSEDEBUG || DEBUG) {
    cerr << "SCHED:    findFreeableClusters()" << endl;
  }

#if DOPROFILING
  startClock = threadCounter->read_tsc();
#endif

  // if there are no waiting clusters, then nothing should be freeable!
#if FRONTIERLIST_USEPRIORITY
  if (!(SCORECUSTOMPRIORITYLIST_ISEMPTY(frontierClusterList)) || 
      !(SCORECUSTOMLINKEDLIST_ISEMPTY(waitingClusterList))) {
#else
  if (!(SCORECUSTOMLINKEDLIST_ISEMPTY(frontierClusterList)) || 
      !(SCORECUSTOMLINKEDLIST_ISEMPTY(waitingClusterList))) {
#endif
    // for all the clusters on the resident cluster list, check all of their
    // pages to see if they are freeable (i.e. stalled beyond the threshold).
    // if so, then put the cluster on the freeable cluster list.
    SCORECUSTOMLINKEDLIST_HEAD(residentClusterList, listItem);
    while (listItem != SCORECUSTOMLINKEDLIST_NULL) {
      ScoreCluster *currentCluster;
      unsigned int numNodes = 0;
      unsigned int numNodesStitch = 0;
      unsigned int numNodesFreeable = 0;
      unsigned int numNodesStitchFreeable = 0;
      
      SCORECUSTOMLINKEDLIST_ITEMAT(residentClusterList, 
				   listItem, currentCluster);

      // make sure we are allowed to free this cluster.
      if (!(currentCluster->shouldNotBeFreed)) {
	// go through all nodes on the cluster and check to see if they are
	// all freeable.
	for (i = 0; i < SCORECUSTOMLIST_LENGTH(currentCluster->nodeList);
	     i++) {
	  ScoreGraphNode *currentNode;
	  unsigned int currentNodeLoc;
	  
	  SCORECUSTOMLIST_ITEMAT(currentCluster->nodeList, i, currentNode);
          currentNodeLoc = currentNode->sched_residentLoc;

	  // don't count done nodes!
	  if (!(currentNode->sched_isDone)) {
	    if (currentNode->isPage()) {
	      unsigned int currentPageStallCount =
		cpStatus[currentNodeLoc].stallCount;
	    
	      numNodes++;

	      if (currentPageStallCount >= SCORE_STALL_THRESHOLD) {
		numNodesFreeable++;
	      }
	    } else {
	      unsigned int currentSegmentStallCount =
		cmbStatus[currentNodeLoc].stallCount;

	      if (((ScoreSegment *) currentNode)->sched_isStitch) {
		numNodesStitch++;

		if (currentSegmentStallCount >= SCORE_STALL_THRESHOLD) {
		  numNodesStitchFreeable++;
		}
	      } else {
		numNodes++;

		if (currentSegmentStallCount >= SCORE_STALL_THRESHOLD) {
		  numNodesFreeable++;
		}
	      }
	    }
	  }
	}
	
        // if we want to consider stitch buffers in a cluster with
        // non-stitch buffer nodes (or if there is simply no other
        // nodes in the cluster), then add the stitch buffer counts to
        // the main counts.
        if (!noCareStitchBufferInClusters ||
            (numNodes == numNodesStitch)) {
          numNodesFreeable = numNodesFreeable + numNodesStitchFreeable;
        }

	// if the ratio of nodes in a cluster that are freeable (i.e. stalled)
	// is greater than or equal to SCORE_CLUSTERFREEABLE_RATIO, then
	// place the cluster on the freeable list.
	if ((((float) numNodesFreeable)/((float) numNodes)) >=
	    SCORE_CLUSTERFREEABLE_RATIO) {
	  currentCluster->isFreeable = 1;
	  SCORECUSTOMLIST_APPEND(freeableClusterList, currentCluster);
	}
      } else {
	// unflag this cluster as unfreeable.
	currentCluster->shouldNotBeFreed = 0;
      }

      SCORECUSTOMLINKEDLIST_GOTONEXT(residentClusterList, listItem);
    }
  }

#if DOPROFILING
  endClock = threadCounter->read_tsc();
  crit_loop_prof->addSample(currentTimeslice, PROF_findFreeableClusters, 
			    endClock - startClock, DOPROFILING_VERBOSE);
#endif
}


///////////////////////////////////////////////////////////////////////////////
// ScoreSchedulerDynamic::dealWithDeadLock:
//   Determine if any processes have deadlocked. If so, deal with it!
//
// Parameters: none.
//
// Return value: none.
///////////////////////////////////////////////////////////////////////////////
void ScoreSchedulerDynamic::dealWithDeadLock() {
  unsigned int i;

  if (VERBOSEDEBUG || DEBUG) {
    cerr << "SCHED:    dealWithDeadLock()" << endl;
  }

#if DOPROFILING
  startClock = threadCounter->read_tsc();
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
    cerr << "After finddeadlock" << endl;

    if (deadLockedCycles.length() != 0) {
      cerr << "inside deadlockedcycle" << endl;
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
      cerr << "inside bufferlockedstreams" << endl;
      if (VERBOSEDEBUG || DEBUG || EXTRA_DEBUG) {
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

#if DOPROFILING
  endClock = threadCounter->read_tsc();
  crit_loop_prof->addSample(currentTimeslice, PROF_dealWithDeadLock,
			    endClock - startClock, DOPROFILING_VERBOSE);
#endif
}


///////////////////////////////////////////////////////////////////////////////
// ScoreSchedulerDynamic::scheduleClusters:
//   Given the list of done pages/segments, freeable clusters, 
//     ready clusters, and waiting clusters, decide which clusters should 
//     remain on the array and which clusters should be removed from the 
//     array. It will fill in the following lists with the result: 
//     scheduledPageList, scheduledMemSegList, removedPageList, 
//     removedMemSegList. The appropriate clusters will be moved to/from 
//     frontierClusterList, waitingClusterList, scheduledClusterList, 
//     removedClusterList, residentClusterList.
//
// Parameters: none.
//
// Return value: none.
///////////////////////////////////////////////////////////////////////////////
void ScoreSchedulerDynamic::scheduleClusters() {

  if (VERBOSEDEBUG || DEBUG) {
    cerr << "SCHED:    scheduleClusters()" << endl;
  }

  int numFreePage, numFreeMemSeg;
  unsigned int traversal;
  SCORECUSTOMLINKEDLISTITEM listItem;
  char forceCompleteReschedule = 0;
  unsigned int i, j;

#if DOPROFILING
  startClock = threadCounter->read_tsc();
#endif

#if DOPROFILING_SCHEDULECLUSTERS
  if (DOPROFILING_VERBOSE) {
    cerr << "===========> SCHEDULECLUSTERS START TIMESLICE: " << 
      currentTimeslice << endl;
  }

  statDataType current_scheduleClusters_performTrials = 0,
    current_scheduleClusters_performTrials_reprioritize = 0;

  unsigned int profilerItemCount = 
    SCORECUSTOMLIST_LENGTH(doneNodeList);
  startClock = threadCounter->read_tsc();
#endif

  // FIX ME! WE MIGHT WANT TO TRY TO GIVE SOME PRIORITY TO CLUSTERS CONNECTED
  //         TO ADDED STITCH BUFFERS??

  // get the number of free pages and memory segments there currently are.
  numFreePage = currentNumFreeCPs;
  numFreeMemSeg = currentNumFreeCMBs;

  traversal = currentTraversal;

  // FIX ME! Need to deal with how faulted CMBs interact with the schedule.
  // FIX ME! SHOULD PROVIDE SOME KIND OF FAIRNESS SCHEDULING BETWEEN
  //         DIFFERENT PROCESSES!
  // FIX ME! WHEN WE PUT IN A STITCH BUFFER TO SOLVE BUFFERLOCK, WE SHOULD
  //         TRY TO MAKE SURE THE SOURCE OF THE STITCH BUFFER DATA IS
  //         GOING TO BE SCHEDULED (TO GUARANTEE THAT THE BUFFERLOCK WILL
  //         BE SOLVED!

  // determine the number of free pages and memory segments we will get if
  // we remove all done pages/segments.
  // NOTE: We will do some of the cleanup here to avoid extra work being
  //       done:
  //       - reduce the numPages/numSegments count in parent cluster.
  //       - remove the node from the node list of the parent cluster.
  //       - remove the stream I/O attached to the done node from the
  //         cluster stream I/O.
  //       - if the cluster has now become empty, then remove it from
  //         the resident cluster list, freeable cluster list, and waiting
  //         cluster list.
  for (i = 0; i < SCORECUSTOMLIST_LENGTH(doneNodeList); i++) {
    ScoreGraphNode *currentNode;
    ScoreCluster *parentCluster;
    unsigned int numInputs;
    unsigned int numOutputs;

    SCORECUSTOMLIST_ITEMAT(doneNodeList, i, currentNode);
    parentCluster = currentNode->sched_parentCluster;
    numInputs = currentNode->getInputs();
    numOutputs = currentNode->getOutputs();

    if (currentNode->sched_isResident) {
      if (currentNode->isPage()) {
	// increase the number of free pages by 1.
	numFreePage++;
      } else {
	// increase the number of free segments by 1.
	numFreeMemSeg++;
      }

      // mark the node not isScheduled.
      currentNode->sched_isScheduled = 0;
    }

    // do partial cleanup.
    if (!(currentNode->isSegment() && 
	  ((ScoreSegment *) currentNode)->sched_isStitch)) {
      parentCluster->removeNode(currentNode);

      for (j = 0; j < numOutputs; j++) {
        ScoreStream *attachedStream = currentNode->getSchedOutput(j);
	ScoreGraphNode *attachedNode = attachedStream->sched_sink;

	if (attachedStream->sched_isCrossCluster) {
	  if (attachedStream->sched_sinkIsDone) {
	    // do nothing!
	  } else if (attachedStream->sched_snkFunc == STREAM_OPERATOR_TYPE) {
	    // do nothing!
	  } else if (attachedStream->sched_snkFunc == STREAM_SEGMENT_TYPE) {
	    ScoreSegment *attachedSegment = (ScoreSegment *) attachedNode;
	      
	    if (attachedSegment->sched_isStitch) {
    	      ScoreSegmentStitch *attachedStitch = 
		(ScoreSegmentStitch *) attachedSegment;
	      
	      attachedStream = attachedStitch->getSchedOutStream();
	      attachedNode = attachedStream->sched_sink;
	    }
	  }

	  if (attachedStream->sched_sinkIsDone) {
	    // do nothing!
	  } else if (attachedStream->sched_snkFunc == STREAM_OPERATOR_TYPE) {
	    // do nothing!
	  } else {
	    ScoreCluster *attachedCluster = 
	      attachedNode->sched_parentCluster;

	    if (!(attachedCluster->isHead)) {
	      SCORECUSTOMLIST_APPEND(headClusterList, attachedCluster);
  	      attachedCluster->isHead = 1;
	    }
	  }
	}
      }

      // look at any of the IO coming from or going to the done node.
      // if that IO is cross cluster, and it is attached to a stitch buffer
      // that is not already done, then figure out whether or not it should
      // be unscheduled. if so, unschedule the stitch buffer.
      // if the output stitch buffer is a sched_isOnlyFeedProcessor stitch
      // buffer which is not done, then move the stitch buffer completely
      // within the cluster.
      for (j = 0; j < numInputs; j++) {
	SCORE_STREAM attachedStream = currentNode->getSchedInput(j);
	ScoreGraphNode *attachedNode = attachedStream->sched_src;
	ScoreSegment *attachedSegment = (ScoreSegment *) attachedNode;
      
	if (attachedStream->sched_isCrossCluster) {
	  if (attachedStream->sched_srcIsDone) {
	    // do nothing!
	  } else if ((attachedStream->sched_srcFunc == STREAM_SEGMENT_TYPE) &&
	    attachedSegment->sched_isStitch) {
	    ScoreSegmentStitch *attachedStitch = 
	      (ScoreSegmentStitch *) attachedSegment;
	    SCORE_STREAM stitchSrcStream = attachedStitch->getSchedInStream();
	    ScoreGraphNode *stitchSrcNode = stitchSrcStream->sched_src;

	    if (attachedStitch->sched_isScheduled) {
	      if (stitchSrcStream->sched_srcIsDone ||
		  (stitchSrcStream->sched_srcFunc == STREAM_OPERATOR_TYPE) ||
		  !(stitchSrcNode->sched_isScheduled)) {
		numFreeMemSeg++;
	      
		// mark the attached stitch buffer unscheduled.
		attachedStitch->sched_isScheduled = 0;
	      }
	    }
	  }
	}
      }
      for (j = 0; j < numOutputs; j++) {
	SCORE_STREAM attachedStream = currentNode->getSchedOutput(j);
	ScoreGraphNode *attachedNode = attachedStream->sched_sink;
	ScoreSegment *attachedSegment = (ScoreSegment *) attachedNode;
      
	if (attachedStream->sched_isCrossCluster) {
	  if (attachedStream->sched_sinkIsDone) {
	    // do nothing!
	  } else if ((attachedStream->sched_snkFunc == STREAM_SEGMENT_TYPE) &&
	    attachedSegment->sched_isStitch) {
	    ScoreSegmentStitch *attachedStitch = 
	      (ScoreSegmentStitch *) attachedSegment;
	    SCORE_STREAM stitchSinkStream = 
	      attachedStitch->getSchedOutStream();
	    ScoreGraphNode *stitchSinkNode = stitchSinkStream->sched_sink;

	    // check to see if this stitch buffer should be marked
	    // sched_isOnlyFeedProcessor.
	    if (!(attachedStitch->sched_isOnlyFeedProcessor)) {
	      SCORE_STREAM outStream = 
		attachedStitch->getSchedOutput(SCORE_CMB_STITCH_DATAR_OUTNUM);

	      if (outStream->sched_snkFunc == STREAM_OPERATOR_TYPE) {
		attachedStitch->sched_isOnlyFeedProcessor = 1;
	      }
	    }

	    if (attachedStitch->sched_isScheduled &&
		!(attachedStitch->sched_isOnlyFeedProcessor)) {
	      if (stitchSinkStream->sched_sinkIsDone ||
		  (stitchSinkStream->sched_snkFunc == STREAM_OPERATOR_TYPE) ||
		  !(stitchSinkNode->sched_isScheduled)) {
		numFreeMemSeg++;
	      
		// mark the attached stitch buffer unscheduled.
		attachedStitch->sched_isScheduled = 0;
	      }
	    } else if (attachedStitch->sched_isScheduled &&
		       attachedStitch->sched_isOnlyFeedProcessor) {
	      parentCluster->addNode_noAddIO(attachedStitch);
	    }
	  }
	}
      }

      if ((parentCluster->numPages == 0) &&
	  (parentCluster->numSegments == 0)) {
	if (parentCluster->isResident) {
	  parentCluster->isResident = 0;

	  SCORECUSTOMLINKEDLIST_DELITEM(residentClusterList,
					parentCluster->clusterResidentListItem);
	} else {
	  if (parentCluster->isFrontier) {
#if FRONTIERLIST_USEPRIORITY
	    SCORECUSTOMPRIORITYLIST_DELITEM(frontierClusterList,
					    parentCluster->clusterFrontierListItem);
#else
	    SCORECUSTOMLINKEDLIST_DELITEM(frontierClusterList,
					  parentCluster->clusterFrontierListItem);
#endif
            parentCluster->isFrontier = 0;
	  } else {
	    SCORECUSTOMLINKEDLIST_DELITEM(waitingClusterList,
					  parentCluster->clusterWaitingListItem);
	  }
	}

	SCORECUSTOMLIST_APPEND(doneClusterList, parentCluster);

	if (parentCluster->isHead) {
	  SCORECUSTOMLIST_REMOVE(headClusterList, parentCluster);
	  parentCluster->isHead = 0;
	}
      } else {
	char isOnHead = 0;

	if (SCORECUSTOMLIST_LENGTH(parentCluster->inputList) == 0) {
	  isOnHead = 1;
	} else {
	  for (j = 0; j < SCORECUSTOMLIST_LENGTH(parentCluster->inputList);
	       j++) {
	    ScoreStream *currentInput;

	    SCORECUSTOMLIST_ITEMAT(parentCluster->inputList, j, currentInput);

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

	if (parentCluster->isHead && !isOnHead) {
	  SCORECUSTOMLIST_REMOVE(headClusterList, parentCluster);
	  parentCluster->isHead = 0;
	} else if (!(parentCluster->isHead) && isOnHead) {
	  SCORECUSTOMLIST_APPEND(headClusterList, parentCluster);
	  parentCluster->isHead = 1;
	}
      }
    } else {
      ScoreSegmentStitch *doneStitch = (ScoreSegmentStitch *) currentNode;

      // if this is a sched_isOnlyFeedProcessor stitch buffer, then we
      // have to treat it differently because it is currently embedded in
      // a cluster and that cluster needs to be cleaned up.
      if (doneStitch->sched_isOnlyFeedProcessor) {
	parentCluster->removeNode_noRemoveIO(doneStitch);

	if ((parentCluster->numPages == 0) &&
	    (parentCluster->numSegments == 0)) {
	  if (parentCluster->isResident) {
	    parentCluster->isResident = 0;

	    SCORECUSTOMLINKEDLIST_DELITEM(residentClusterList,
					  parentCluster->clusterResidentListItem);
	  } else {
	    if (parentCluster->isFrontier) {
#if FRONTIERLIST_USEPRIORITY
	      SCORECUSTOMPRIORITYLIST_DELITEM(frontierClusterList,
					      parentCluster->clusterFrontierListItem);
#else
	      SCORECUSTOMLINKEDLIST_DELITEM(frontierClusterList,
					    parentCluster->clusterFrontierListItem);
#endif
	      parentCluster->isFrontier = 0;
	    } else {
	      SCORECUSTOMLINKEDLIST_DELITEM(waitingClusterList,
					    parentCluster->clusterWaitingListItem);
	    }
	  }

	  SCORECUSTOMLIST_APPEND(doneClusterList, parentCluster);

	  if (parentCluster->isHead) {
	    SCORECUSTOMLIST_REMOVE(headClusterList, parentCluster);
	    parentCluster->isHead = 0;
	  }
	}
      }
    }
  }
  SCORECUSTOMSTACK_PUSH(numFreePageTrial, numFreePage);
  SCORECUSTOMSTACK_PUSH(numFreeMemSegTrial, numFreeMemSeg);
  SCORECUSTOMSTACK_PUSH(traversalTrial, traversal);

#if DOPROFILING_SCHEDULECLUSTERS
  endClock = threadCounter->read_tsc();
  diffClock = endClock - startClock;
  sched_clusters_stat_prof->addSample(currentTimeslice, 
				      PROF_handleDoneNodes,
				      diffClock, DOPROFILING_VERBOSE);

  sched_clusters_stat_prof->
    addSample_perItem(PROF_handleDoneNodes, diffClock,
		      profilerItemCount, DOPROFILING_VERBOSE);


  profilerItemCount = SCORECUSTOMLIST_LENGTH(addedBufferLockStitchBufferList);
  startClock = threadCounter->read_tsc();
#endif
 
  if (SCORECUSTOMLIST_LENGTH(addedBufferLockStitchBufferList) > 0) {
    // first remove any added stitch buffers for bufferlock resolution which
    // are not directly connected to a currently resident node.
    // NOTE: WE SHOULD NEVER HAVE TO DEAL WITH DONE NODES! THEY WOULD NOT BE
    //       IN THE PATH OF A BUFFERLOCKED LOOP!
    i = 0;
    while (i < SCORECUSTOMLIST_LENGTH(addedBufferLockStitchBufferList)) {
      ScoreSegmentStitch *currentStitch;
      ScoreStream *inStream;
      ScoreStream *outStream;
      char isAttachedToResidentNodes = 0;

      SCORECUSTOMLIST_ITEMAT(addedBufferLockStitchBufferList, 
			     i, currentStitch);
      inStream = currentStitch->getSchedInStream();
      outStream = currentStitch->getSchedOutStream();

      if (inStream->sched_srcFunc != STREAM_OPERATOR_TYPE) {
	if (inStream->sched_src->sched_isResident) {
	  isAttachedToResidentNodes = 1;
	}
      }
      if (outStream->sched_snkFunc != STREAM_OPERATOR_TYPE) {
	if (outStream->sched_sink->sched_isResident) {
	  isAttachedToResidentNodes = 1;
	}
      }

      if (!isAttachedToResidentNodes) {
	currentStitch->sched_isScheduled = 0;
	SCORECUSTOMLIST_REMOVEITEMAT(addedBufferLockStitchBufferList, i);
	
	// want to keep index same!
	continue;
      }
      
      i++;
    }
    
    // go through the list of added stitch buffers for bufferlock resolution
    // (after having removed the stitch buffers that will not affect currently
    //  resident nodes), and try to schedule the stitch buffers.
    // if we cannot fit the remaining stitch buffers, we force a complete
    // rescheduling.
    // NOTE: WE ARE ASSUMING THAT NO STITCH BUFFERS WE PLACED ARE DIRECTLY NEXT
    //       TO ANOTHER STITCH BUFFER!
    // NOTE: WE SHOULD NEVER HAVE TO DEAL WITH DONE NODES! THEY WOULD NOT BE
    //       IN THE PATH OF A BUFFERLOCKED LOOP!
    // FIX ME! THERE MUST BE A BETTER WAY!
    // FIX ME! WE ARE ASSUMING NO CLUSTER SPLITTING HAS OCCURED! IF IT HAS,
    //         THIS BECOMES A MUCH HARDER PROBLEM!
    if (SCORECUSTOMLIST_LENGTH(addedBufferLockStitchBufferList) <= 
	(unsigned int) numFreeMemSeg) {
      // decrement the free segment count by the number of stitch buffers
      // we are placing on the array.
      numFreeMemSeg = numFreeMemSeg - 
	SCORECUSTOMLIST_LENGTH(addedBufferLockStitchBufferList);

      // remove the attached clusters (src/sink) from the 
      // freeable cluster list.
      // NOTE: SLIGHTLY INEFFICIENT DUE TO LINEAR SEARCH O(N) OVER M ITEMS!
      // FIX ME! FIND BETTER WAY!
      for (i = 0; i < SCORECUSTOMLIST_LENGTH(addedBufferLockStitchBufferList);
	   i++) {
	ScoreSegmentStitch *currentStitch;
	ScoreStream *inStream;
	ScoreStream *outStream;
	ScoreGraphNode *srcNode;
	ScoreGraphNode *sinkNode;
	
	SCORECUSTOMLIST_ITEMAT(addedBufferLockStitchBufferList, 
			       i, currentStitch);
	inStream = currentStitch->getSchedInStream();
	outStream = currentStitch->getSchedOutStream();
	srcNode = inStream->sched_src;
	sinkNode = outStream->sched_sink;
	
	currentStitch->sched_isScheduled = 1;
      }

      for (i = 0; i < SCORECUSTOMLIST_LENGTH(freeableClusterList); i++) {
	ScoreCluster *currentCluster;

	SCORECUSTOMLIST_ITEMAT(freeableClusterList, i, currentCluster);

	currentCluster->isFreeable = 0;
      }
      SCORECUSTOMLIST_CLEAR(freeableClusterList);
    } else {
      if (VERBOSEDEBUG || DEBUG) {
	cerr << "SCHED: COULD NOT FIT ADDED BUFFERLOCK STITCH BUFFERS! " <<
	  "FORCING COMPLETE RESCHEDULE!" << endl;
      }

      for (i = 0; i < SCORECUSTOMLIST_LENGTH(addedBufferLockStitchBufferList);
	   i++) {
	ScoreSegmentStitch *currentStitch;
	
	SCORECUSTOMLIST_ITEMAT(addedBufferLockStitchBufferList, 
			       i, currentStitch);

	currentStitch->sched_isScheduled = 0;
      }
      SCORECUSTOMLIST_CLEAR(addedBufferLockStitchBufferList);
      
      forceCompleteReschedule = 1;
    }
  }
  SCORECUSTOMSTACK_PUSH(numFreePageTrial, numFreePage);
  SCORECUSTOMSTACK_PUSH(numFreeMemSegTrial, numFreeMemSeg);
  SCORECUSTOMSTACK_PUSH(traversalTrial, traversal);
  
#if DOPROFILING_SCHEDULECLUSTERS
  endClock = threadCounter->read_tsc();
  diffClock = endClock - startClock;

  sched_clusters_stat_prof->addSample(currentTimeslice, 
				      PROF_bufferLockStitchBuffers,
				      diffClock, DOPROFILING_VERBOSE);

  sched_clusters_stat_prof->addSample_perItem(PROF_bufferLockStitchBuffers,
					      diffClock,
					      profilerItemCount, 
					      DOPROFILING_VERBOSE);


#endif

  // only continue if there are actually clusters waiting to be scheduled!
  // (or forced to completely reschedule).
  // otherwise, just keep the current config (minus done pages).
  if (
#if FRONTIERLIST_USEPRIORITY
      !(SCORECUSTOMPRIORITYLIST_ISEMPTY(frontierClusterList)) || 
      !(SCORECUSTOMLINKEDLIST_ISEMPTY(waitingClusterList)) || 
#else
      !(SCORECUSTOMLINKEDLIST_ISEMPTY(frontierClusterList)) || 
      !(SCORECUSTOMLINKEDLIST_ISEMPTY(waitingClusterList)) || 
#endif
      forceCompleteReschedule) {

#if DOPROFILING_SCHEDULECLUSTERS
    profilerItemCount = forceCompleteReschedule ?
      0 : SCORECUSTOMLIST_LENGTH(freeableClusterList);

    startClock = threadCounter->read_tsc();
#endif
 
    SCORECUSTOMSTACK_PUSH(frontierClusterTrial, NULL);

    if (!forceCompleteReschedule) {
      // determine the number of free pages and memory segments we will get if
      // we free all of the freeable clusters.
      for (i = 0; i < SCORECUSTOMLIST_LENGTH(freeableClusterList); i++) {
        ScoreCluster *currentCluster;

	SCORECUSTOMLIST_ITEMAT(freeableClusterList, i, currentCluster);

	// increase the number of free pages/segments by the number of 
	// pages/segments in the cluster.
	numFreePage = numFreePage + currentCluster->numPages;
	numFreeMemSeg = numFreeMemSeg + currentCluster->numSegments;

	currentCluster->isScheduled = 0;

	// mark all of the pages and segments in the cluster not isScheduled.
	// additionally, if any of the pages/segments are marked done, then
	// they will be doubly-counted. so, adjust the count.
	for (j = 0; j < SCORECUSTOMLIST_LENGTH(currentCluster->nodeList);
	     j++) {
	  ScoreGraphNode *currentNode;
	  
	  SCORECUSTOMLIST_ITEMAT(currentCluster->nodeList, j, currentNode);
	  
	  currentNode->sched_isScheduled = 0;
	}

	// correctly account for the addition/subtraction of free memory 
	// segments from adjacent nodes.
	// if input is done, then:
	//   --> (do nothing!)
	// else, if input is operator, then:
	//   --> (do nothing!)
	// else, if input is stitch segment, then:
	//   if the stitch segment is currently scheduled and its input node
	//     is done, is an operator, or is not scheduled, then:
	//       --> numFreeMemSeg++
	// else:
	//   if this is not a cluster self-loop, then:
	//     if input isScheduled, then:
	//       --> numFreeMemSeg--
	//     else, if input not isScheduled, then:
	//       --> (do nothing!)
	for (j = 0; j < SCORECUSTOMLIST_LENGTH(currentCluster->inputList);
	     j++) {
	  SCORE_STREAM attachedStream;
	  ScoreGraphNode *attachedNode;
	  ScoreSegment *attachedSegment;
	  
	  SCORECUSTOMLIST_ITEMAT(currentCluster->inputList, j, attachedStream);
	  attachedNode = attachedStream->sched_src;
	  attachedSegment = (ScoreSegment *) attachedNode;
	  
	  if (attachedStream->sched_srcIsDone) {
	    // do nothing!
	  } else if (attachedStream->sched_srcFunc == STREAM_OPERATOR_TYPE) {
	    // do nothing!
	  } else if ((attachedStream->sched_srcFunc == STREAM_SEGMENT_TYPE) &&
		     attachedSegment->sched_isStitch) {
	    ScoreSegmentStitch *attachedStitch = 
	      (ScoreSegmentStitch *) attachedSegment;
	    SCORE_STREAM stitchSrcStream = attachedStitch->getSchedInStream();
	    ScoreGraphNode *stitchSrcNode = stitchSrcStream->sched_src;
	    
	    if (attachedStitch->sched_isScheduled) {
	      if (stitchSrcStream->sched_srcIsDone ||
		  (stitchSrcStream->sched_srcFunc == STREAM_OPERATOR_TYPE) ||
		  !(stitchSrcNode->sched_isScheduled)) {
		numFreeMemSeg++;
		
		// mark the attached stitch buffer unscheduled.
		attachedStitch->sched_isScheduled = 0;
	      }
	    }
	  } else {
	    if (attachedNode->sched_parentCluster != currentCluster) {
	      if (attachedNode->sched_isScheduled) {
		numFreeMemSeg--;
	      } else {
		// do nothing!
	      }
	    }
	  }
	}
	// if output is done, then:
	//   --> (do nothing!)
	// else, if output is operator, then:
	//   --> (do nothing!)
	// else, if output is stitch segment, then:
	//   if the stitch segment is currently scheduled and its output node
	//     is done, is an operator, or is not scheduled, then:
	//       --> numFreeMemSeg++
	// else:
	//   if this is not a cluster self-loop, then:
	//     if output isScheduled, then:
	//       --> (do nothing!)
	//     else, if output not isScheduled, then:
	//       --> numFreeMemSeg++ (compensates for a previous speculatively 
	//                            added stitch segment).
	for (j = 0; j < SCORECUSTOMLIST_LENGTH(currentCluster->outputList);
	     j++) {
	  SCORE_STREAM attachedStream;
	  ScoreGraphNode *attachedNode;
	  ScoreSegment *attachedSegment;
	  
	  SCORECUSTOMLIST_ITEMAT(currentCluster->outputList, 
				 j, attachedStream);
	  attachedNode = attachedStream->sched_sink;
	  attachedSegment = (ScoreSegment *) attachedNode;
	  
	  if (attachedStream->sched_sinkIsDone) {
	    // do nothing!
	  } else if (attachedStream->sched_snkFunc == STREAM_OPERATOR_TYPE) {
	    // do nothing!
	  } else if ((attachedStream->sched_snkFunc == STREAM_SEGMENT_TYPE) &&
		     attachedSegment->sched_isStitch) {
	    ScoreSegmentStitch *attachedStitch = 
	      (ScoreSegmentStitch *) attachedSegment;
	    SCORE_STREAM stitchSinkStream = 
	      attachedStitch->getSchedOutStream();
	    ScoreGraphNode *stitchSinkNode = stitchSinkStream->sched_sink;
	    
	    if (attachedStitch->sched_isScheduled) {
	      if (stitchSinkStream->sched_sinkIsDone ||
		  (stitchSinkStream->sched_snkFunc == STREAM_OPERATOR_TYPE) ||
		  !(stitchSinkNode->sched_isScheduled)) {
		numFreeMemSeg++;
		
		// mark the attached stitch buffer unscheduled.
		attachedStitch->sched_isScheduled = 0;
	      }
	    }
	  } else {
	    if (attachedNode->sched_parentCluster != currentCluster) {
	      if (attachedNode->sched_isScheduled) {
		// do nothing!
	      } else {
		numFreeMemSeg++;
	      }
	    }
	  }
	}
      }
      SCORECUSTOMSTACK_PUSH(numFreePageTrial, numFreePage);
      SCORECUSTOMSTACK_PUSH(numFreeMemSegTrial, numFreeMemSeg);
      SCORECUSTOMSTACK_PUSH(traversalTrial, traversal);
    } else {
      SCORECUSTOMLIST_CLEAR(freeableClusterList);

      for (i = 0; i < SCORECUSTOMLIST_LENGTH(faultedMemSegList); i++) {
        ScoreSegment *faultedSegment;

	SCORECUSTOMLIST_ITEMAT(faultedMemSegList, i, faultedSegment);

        faultedSegment->sched_isFaulted = 0;
      }
      SCORECUSTOMLIST_CLEAR(faultedMemSegList);

      SCORECUSTOMLINKEDLIST_HEAD(residentClusterList, listItem);
      while (listItem != SCORECUSTOMLINKEDLIST_NULL) {
        ScoreCluster *currentCluster;

	SCORECUSTOMLINKEDLIST_ITEMAT(residentClusterList, 
				     listItem, currentCluster);

        currentCluster->isFreeable = 1;
        currentCluster->isScheduled = 0;

        // mark all of the pages and segments in the cluster not isScheduled.
        // additionally, if any of the pages/segments are marked done, then
        // they will be doubly-counted. so, adjust the count.
	for (i = 0; i < SCORECUSTOMLIST_LENGTH(currentCluster->nodeList);
	     i++) {
    	  ScoreGraphNode *currentNode;

	  SCORECUSTOMLIST_ITEMAT(currentCluster->nodeList, i, currentNode);

	  currentNode->sched_isScheduled = 0;
        }

	SCORECUSTOMLIST_APPEND(freeableClusterList, currentCluster);

	SCORECUSTOMLINKEDLIST_GOTONEXT(residentClusterList, listItem);
      }

      numFreePage = numPhysicalCP;
      numFreeMemSeg = numPhysicalCMB;
      for (i = 0; i < SCORECUSTOMLIST_LENGTH(stitchBufferList); i++) {
        ScoreSegmentStitch *currentStitch;

	SCORECUSTOMLIST_ITEMAT(stitchBufferList, i, currentStitch);

        currentStitch->sched_isScheduled = 0;
      }

      SCORECUSTOMSTACK_PUSH(numFreePageTrial, numFreePage);
      SCORECUSTOMSTACK_PUSH(numFreeMemSegTrial, numFreeMemSeg);
      SCORECUSTOMSTACK_PUSH(traversalTrial, traversal);
    }

#if DOPROFILING_SCHEDULECLUSTERS
    endClock = threadCounter->read_tsc();
    diffClock = endClock - startClock;

    sched_clusters_stat_prof->addSample(currentTimeslice, 
					PROF_handleFreeableClusters,
					diffClock, DOPROFILING_VERBOSE);
    
    sched_clusters_stat_prof->addSample_perItem(PROF_handleFreeableClusters,
						diffClock,
						profilerItemCount,
						DOPROFILING_VERBOSE);


    profilerItemCount = SCORECUSTOMPRIORITYLIST_LENGTH(frontierClusterList);
    startClock = threadCounter->read_tsc();
#endif

#if FRONTIERLIST_USEPRIORITY
    // force all clusters on the frontier list to recompute their
    // priorities.
    // for all clusters on the recompute cluster list, remove them and
    // reinsert them from the waiting cluster list (effectively, forcing
    // a priority recompute).
    // NOTE: If the priority goes down, we keep the original priority.
    for (i = 1; i <= SCORECUSTOMPRIORITYLIST_LENGTH(frontierClusterList); 
	 i++) {
      ScoreCluster *currentCluster;

      SCORECUSTOMPRIORITYLIST_ITEMATMAPINDEX(frontierClusterList,
					     i, currentCluster);

      reprioritizeClusterArray[i] = currentCluster;
    }
    for (i = 1; i <= SCORECUSTOMPRIORITYLIST_LENGTH(frontierClusterList); 
	 i++) {
      ScoreCluster *currentCluster;
      int oldPriority;

      currentCluster = reprioritizeClusterArray[i];
      oldPriority = currentCluster->lastCalculatedPriority;

      calculateClusterPriority(currentCluster);
      if (oldPriority > currentCluster->lastCalculatedPriority) {
	currentCluster->lastCalculatedPriority = oldPriority;
      }

      SCORECUSTOMPRIORITYLIST_INCREASEPRIORITY(frontierClusterList,
					   currentCluster->clusterFrontierListItem, currentCluster->lastCalculatedPriority);
    }
#endif

#if DOPROFILING_SCHEDULECLUSTERS
    endClock = threadCounter->read_tsc();
    diffClock = endClock - startClock;

    sched_clusters_stat_prof->
      addSample(currentTimeslice, 
		PROF_handleFreeableClusters_reprioritize,
		diffClock, DOPROFILING_VERBOSE);
    
    sched_clusters_stat_prof->
      addSample_perItem(PROF_handleFreeableClusters_reprioritize,
			diffClock,
			profilerItemCount, DOPROFILING_VERBOSE);

    startClock = threadCounter->read_tsc();
#endif
 
#if FRONTIERLIST_USEPRIORITY
    if (SCORECUSTOMPRIORITYLIST_ISEMPTY(frontierClusterList)) {
      traversal++;

      for (i = 0; i < SCORECUSTOMLIST_LENGTH(headClusterList); i++) {
	ScoreCluster *headCluster;

	SCORECUSTOMLIST_ITEMAT(headClusterList, i, headCluster);
	
	if (!(headCluster->isScheduled)) {
	  if (!(headCluster->isFrontier)) {
	    if (headCluster->isFreeable) {
	      // do nothing!
	    } else {
	      SCORECUSTOMLINKEDLIST_DELITEM(waitingClusterList,
					    headCluster->
					    clusterWaitingListItem);
	    }
	    
	    calculateClusterPriority(headCluster);
	    SCORECUSTOMPRIORITYLIST_INSERT(frontierClusterList,
					   headCluster, 
					   headCluster->lastCalculatedPriority,
					   headCluster->clusterFrontierListItem);
	    headCluster->isFrontier = 1;
	  }
	}
      }
      
      // if, at this point, the frontier is still empty, then try to
      // put one waiting list cluster on the frontier.
      if (SCORECUSTOMPRIORITYLIST_ISEMPTY(frontierClusterList)) {
	if (!(SCORECUSTOMLINKEDLIST_ISEMPTY(waitingClusterList))) {
	  ScoreCluster *waitingCluster;

	  SCORECUSTOMLINKEDLIST_POP(waitingClusterList, waitingCluster);
	  
	  waitingCluster->clusterWaitingListItem = SCORECUSTOMLINKEDLIST_NULL;
	  
	  calculateClusterPriority(waitingCluster);
	  SCORECUSTOMPRIORITYLIST_INSERT(frontierClusterList,
					 waitingCluster,
					 waitingCluster->clusterFrontierListItem,
					 waitingCluster->clusterFrontierListItem);
	  waitingCluster->isFrontier = 1;
	}
      }
    }
#else
    if (SCORECUSTOMLINKEDLIST_ISEMPTY(frontierClusterList)) {
      traversal++;

      for (i = 0; i < SCORECUSTOMLIST_LENGTH(headClusterList); i++) {
	ScoreCluster *headCluster;

	SCORECUSTOMLIST_ITEMAT(headClusterList, i, headCluster);
	
	if (!(headCluster->isScheduled)) {
	  if (!(headCluster->isFrontier)) {
	    if (headCluster->isFreeable) {
	      // do nothing!
	    } else {
	      SCORECUSTOMLINKEDLIST_DELITEM(waitingClusterList,
					    headCluster->clusterWaitingListItem);
	    }
	    
	    SCORECUSTOMLINKEDLIST_APPEND(frontierClusterList, headCluster,
					 headCluster->clusterFrontierListItem);
	    headCluster->isFrontier = 1;
	  }
	}
      }
      
      // if, at this point, the frontier is still empty, then try to
      // put one waiting list cluster on the frontier.
      if (SCORECUSTOMLINKEDLIST_ISEMPTY(frontierClusterList)) {
	if (!(SCORECUSTOMLINKEDLIST_ISEMPTY(waitingClusterList))) {
	  ScoreCluster *waitingCluster;

	  SCORECUSTOMLINKEDLIST_POP(waitingClusterList, waitingCluster);
	  
	  waitingCluster->clusterWaitingListItem = SCORECUSTOMLINKEDLIST_NULL;
	  
	  SCORECUSTOMLINKEDLIST_APPEND(frontierClusterList, waitingCluster,
				       waitingCluster->clusterFrontierListItem);
	  waitingCluster->isFrontier = 1;
	}
      }
    }
#endif

#if DOPROFILING_SCHEDULECLUSTERS
    endClock = threadCounter->read_tsc();
    diffClock = endClock - startClock;

    sched_clusters_stat_prof->addSample(currentTimeslice,
					PROF_handleEmptyFrontierList,
					diffClock, DOPROFILING_VERBOSE);

    profilerItemCount = 0;
#endif

    // repeatedly add clusters from the frontier list until no more physical
    // pages are available. keep track of the number of memory segments each
    // trial needs.
    // when the frontier list is empty, then reload it with the head list.
    // FIX ME! SHOULD I TRY TO BYPASS LARGE CLUSTERS IF THEY CANNOT BE
    //         SCHEDULED AND GO ON TO A LOWER PRIORITY SMALLER CLUSTER?
    // FIX ME! THIS IS GOING TO GET KIND OF EXPENSIVE, ALL OF THIS PRIORITY
    //         RECALCULATION! ANY OTHER WAY TO DO THIS?
    // FIX ME! THERE IS NO BASIS FOR HOW I AM CURRENTLY REENTERING FREEABLE
    //         BACK ON THE RESIDENT LIST!
#if FRONTIERLIST_USEPRIORITY
    while ((numFreePage > 0) && 
	   (SCORECUSTOMPRIORITYLIST_LENGTH(frontierClusterList) > 0)) 
#else
    while ((numFreePage > 0) && 
	   (SCORECUSTOMLINKEDLIST_LENGTH(frontierClusterList) > 0))
#endif
      {
#if DOPROFILING_SCHEDULECLUSTERS
      profilerItemCount ++;
     startClock = threadCounter->read_tsc();
#endif
      ScoreCluster *currentCluster;

#if FRONTIERLIST_USEPRIORITY
      SCORECUSTOMPRIORITYLIST_POPMAX(frontierClusterList,
				     currentCluster);
      currentCluster->clusterFrontierListItem = SCORECUSTOMPRIORITYLIST_NULL;
#else
      SCORECUSTOMLINKEDLIST_POP(frontierClusterList, currentCluster);
      currentCluster->clusterFrontierListItem = SCORECUSTOMLINKEDLIST_NULL;
#endif

      currentCluster->isFrontier = 0;

      // decrease the number of free pages/segments by the number of 
      // pages/segments in the cluster.
      numFreePage = numFreePage - currentCluster->numPages;
      numFreeMemSeg = numFreeMemSeg - currentCluster->numSegments;

      currentCluster->isScheduled = 1;

      // mark all of the pages/segments in the cluster isScheduled.
      for (i = 0; i < SCORECUSTOMLIST_LENGTH(currentCluster->nodeList);
	   i++) {
	ScoreGraphNode *currentNode;

	SCORECUSTOMLIST_ITEMAT(currentCluster->nodeList, i, currentNode);

	currentNode->sched_isScheduled = 1;
      }

      // correctly account for the addition/subtraction of free memory segments
      //   from adjacent nodes.
      // if input is done, then:
      //   --> (do nothing!)
      // else, if input is operator, then:
      //   --> numFreeMemSeg--
      // else, if input is stitch segment, then:
      //   if the stitch segment is currently not scheduled, then:
      //       --> numFreeMemSeg--
      // else:
      //   if this is not a cluster self-loop, then:
      //     if input isScheduled, then:
      //       --> numFreeMemSeg++ (compensates for a previous speculatively 
      //                            added stitch segment).
      //     else, if input not isScheduled, then:
      //       --> (do nothing!)
      for (i = 0; i < SCORECUSTOMLIST_LENGTH(currentCluster->inputList);
	   i++) {
	SCORE_STREAM attachedStream;
	ScoreGraphNode *attachedNode;
	ScoreSegment *attachedSegment;

	SCORECUSTOMLIST_ITEMAT(currentCluster->inputList, i, attachedStream);
	attachedNode = attachedStream->sched_src;
	attachedSegment = (ScoreSegment *) attachedNode;

	if (attachedStream->sched_srcIsDone) {
	  // do nothing!
	} else if (attachedStream->sched_srcFunc == STREAM_OPERATOR_TYPE) {
	  // FIX ME! AS AN OPTIMIZATION, WE MAY NOT ALWAYS HAVE TO HAVE A
	  //         STITCH BUFFER HERE!
	  numFreeMemSeg--;
	} else if ((attachedStream->sched_srcFunc == STREAM_SEGMENT_TYPE) &&
		   attachedSegment->sched_isStitch) {
	  ScoreSegmentStitch *attachedStitch = 
	    (ScoreSegmentStitch *) attachedSegment;
	  
	  if (!(attachedStitch->sched_isScheduled)) {
	    numFreeMemSeg--;
	  
	    // mark the attached stitch buffer scheduled.
	    attachedStitch->sched_isScheduled = 1;
	  }
	} else {
	  if (attachedNode->sched_parentCluster != currentCluster) {
	    if (attachedNode->sched_isScheduled) {
	      numFreeMemSeg++;
	    } else {
	      // do nothing!
	    }
	  }
	}
      }
      // if output is done, then:
      //   --> (do nothing!)
      // else, if output is operator, then:
      //   --> numFreeMemSeg--
      // else, if output is stitch segment, then:
      //   if the stitch segment is currently not scheduled, then:
      //       --> numFreeMemSeg--
      // else:
      //   if this is not a cluster self-loop, then:
      //     if output isScheduled, then:
      //       --> (do nothing!)
      //     else, if output not isScheduled, then:
      //       --> numFreeMemSeg--
      for (i = 0; i < SCORECUSTOMLIST_LENGTH(currentCluster->outputList);
	   i++) {
	SCORE_STREAM attachedStream;
	ScoreGraphNode *attachedNode;
	ScoreSegment *attachedSegment;

	SCORECUSTOMLIST_ITEMAT(currentCluster->outputList, i, attachedStream);
	attachedNode = attachedStream->sched_sink;
	attachedSegment = (ScoreSegment *) attachedNode;

	if (attachedStream->sched_sinkIsDone) {
	  // do nothing!
	} else if (attachedStream->sched_snkFunc == STREAM_OPERATOR_TYPE) {
	  numFreeMemSeg--;
	} else if ((attachedStream->sched_snkFunc == STREAM_SEGMENT_TYPE) &&
		   attachedSegment->sched_isStitch) {
	  ScoreSegmentStitch *attachedStitch = 
	    (ScoreSegmentStitch *) attachedSegment;
	  
	  if (!(attachedStitch->sched_isScheduled)) {
	    numFreeMemSeg--;

	    // mark the attached stitch buffer scheduled.
	    attachedStitch->sched_isScheduled = 1;
	  }
	} else {
	  if (attachedNode->sched_parentCluster != currentCluster) {
	    if (attachedNode->sched_isScheduled) {
	      // do nothing!
	    } else {
	      numFreeMemSeg--;
	    }
	  }
	}
      }

#if DOPROFILING_SCHEDULECLUSTERS
      endClock = threadCounter->read_tsc();
      diffClock = endClock - startClock;

      current_scheduleClusters_performTrials += diffClock;

      startClock = threadCounter->read_tsc();
#endif

      // add the "frontier" clusters to the frontier list.
      // if the frontier list is still empty, try to add the head list.
#if FRONTIERLIST_USEPRIORITY
      {
	for (i = 0; i < SCORECUSTOMLIST_LENGTH(currentCluster->outputList);
	     i++) {
	  ScoreStream *attachedStream;

	  SCORECUSTOMLIST_ITEMAT(currentCluster->outputList, 
				 i, attachedStream);
	  
	  if (attachedStream->sched_sinkIsDone) {
	    // do nothing!
	  } else if (attachedStream->sched_snkFunc == STREAM_OPERATOR_TYPE) {
	    // do nothing!
	  } else {
	    ScoreGraphNode *attachedNode = attachedStream->sched_sink;
	    
	    if (attachedNode->isSegment()) {
	      ScoreSegment *attachedSegment = (ScoreSegment *) attachedNode;
	  
	      if (attachedSegment->sched_isStitch) {
	        ScoreSegmentStitch *attachedStitch =
	  	  (ScoreSegmentStitch *) attachedSegment;
	      
	        attachedStream = attachedStitch->getSchedOutStream();
	      }
	    }
	  }
	
	  if (attachedStream->sched_sinkIsDone) {
	    // do nothing!
	  } else if (attachedStream->sched_snkFunc == STREAM_OPERATOR_TYPE) {
	    // do nothing!
	  } else {
	    ScoreGraphNode *attachedNode = attachedStream->sched_sink;
	    ScoreCluster *attachedCluster = 
	      attachedNode->sched_parentCluster;
	
	    if (!(attachedCluster->isScheduled)) {
	      // NOTE: if priority decreases, we keep the old priority.
	      if (attachedCluster->isFrontier) {
		int oldPriority;

		oldPriority = attachedCluster->lastCalculatedPriority;

		calculateClusterPriority(attachedCluster);

		if (oldPriority > attachedCluster->lastCalculatedPriority) {
		  attachedCluster->lastCalculatedPriority = oldPriority;
		}

		SCORECUSTOMPRIORITYLIST_INCREASEPRIORITY(frontierClusterList,
							 attachedCluster->clusterFrontierListItem,
							 attachedCluster->lastCalculatedPriority);
		attachedCluster->isFrontier = 1;
	      } else {
		if (attachedCluster->lastFrontierTraversal !=
		    traversal) {
		  if (attachedCluster->isFreeable) {
		    // do nothing!
		  } else {
		    SCORECUSTOMLINKEDLIST_DELITEM(waitingClusterList,
						  attachedCluster->clusterWaitingListItem);
		  }

		  SCORECUSTOMSTACK_PUSH(frontierClusterTrial, attachedCluster);

		  calculateClusterPriority(attachedCluster);
		  SCORECUSTOMPRIORITYLIST_INSERT(frontierClusterList,
						 attachedCluster,
						 attachedCluster->lastCalculatedPriority,
						 attachedCluster->clusterFrontierListItem);
		  attachedCluster->isFrontier = 1;
		}
              }
	    }
          }
  	}

        if (SCORECUSTOMPRIORITYLIST_ISEMPTY(frontierClusterList)) {
	  traversal++;

	  for (i = 0; i < SCORECUSTOMLIST_LENGTH(headClusterList); i++) {
            ScoreCluster *headCluster;

	    SCORECUSTOMLIST_ITEMAT(headClusterList, i, headCluster);

	    if (!(headCluster->isScheduled)) {
	      if (!(headCluster->isFrontier)) {
                if (headCluster->isFreeable) {
		  // do nothing!
                } else {
                  SCORECUSTOMLINKEDLIST_DELITEM(waitingClusterList,
						headCluster->clusterWaitingListItem);
                }

                SCORECUSTOMSTACK_PUSH(frontierClusterTrial, headCluster);

		calculateClusterPriority(headCluster);
		SCORECUSTOMPRIORITYLIST_INSERT(frontierClusterList,
					       headCluster,
					       headCluster->lastCalculatedPriority,
					       headCluster->clusterFrontierListItem);
                headCluster->isFrontier = 1;
              }
	    }
          }

	  // if, at this point, the frontier is still empty, then try to
	  // put one waiting list cluster on the frontier.
	  if (SCORECUSTOMPRIORITYLIST_ISEMPTY(frontierClusterList)) {
	    if (!(SCORECUSTOMLINKEDLIST_ISEMPTY(waitingClusterList))) {
	      ScoreCluster *waitingCluster;

	      SCORECUSTOMLINKEDLIST_POP(waitingClusterList, waitingCluster);

	      waitingCluster->clusterWaitingListItem = 
		SCORECUSTOMLINKEDLIST_NULL;

	      SCORECUSTOMSTACK_PUSH(frontierClusterTrial, waitingCluster);

	      calculateClusterPriority(waitingCluster);
	      SCORECUSTOMPRIORITYLIST_INSERT(frontierClusterList,
					     waitingCluster,
					     waitingCluster->lastCalculatedPriority,
					     waitingCluster->clusterFrontierListItem);
	      waitingCluster->isFrontier = 1;
	    }
	  }
        }

        SCORECUSTOMSTACK_PUSH(frontierClusterTrial, NULL);
      }
#else
      {
	for (i = 0; i < SCORECUSTOMLIST_LENGTH(currentCluster->outputList);
	     i++) {
	  ScoreStream *attachedStream;

	  SCORECUSTOMLIST_ITEMAT(currentCluster->outputList, 
				 i, attachedStream);
	  
	  if (attachedStream->sched_sinkIsDone) {
	    // do nothing!
	  } else if (attachedStream->sched_snkFunc == STREAM_OPERATOR_TYPE) {
	    // do nothing!
	  } else {
	    ScoreGraphNode *attachedNode = attachedStream->sched_sink;
	    
	    if (attachedNode->isSegment()) {
	      ScoreSegment *attachedSegment = (ScoreSegment *) attachedNode;
	  
	      if (attachedSegment->sched_isStitch) {
	        ScoreSegmentStitch *attachedStitch =
	  	  (ScoreSegmentStitch *) attachedSegment;
	      
	        attachedStream = attachedStitch->getSchedOutStream();
	      }
	    }
	  }
	
	  if (attachedStream->sched_sinkIsDone) {
	    // do nothing!
	  } else if (attachedStream->sched_snkFunc == STREAM_OPERATOR_TYPE) {
	    // do nothing!
	  } else {
	    ScoreGraphNode *attachedNode = attachedStream->sched_sink;
	    ScoreCluster *attachedCluster = 
	      attachedNode->sched_parentCluster;
	
	    if (!(attachedCluster->isScheduled)) {
	      if (attachedCluster->isFrontier) {
		// do nothing!
	      } else {
		if (attachedCluster->lastFrontierTraversal !=
		    traversal) {
		  if (attachedCluster->isFreeable) {
		    // do nothing!
		  } else {
		    SCORECUSTOMLINKEDLIST_DELITEM(waitingClusterList,
						  attachedCluster->clusterWaitingListItem);
		  }

		  SCORECUSTOMSTACK_PUSH(frontierClusterTrial, attachedCluster);

		  SCORECUSTOMLINKEDLIST_APPEND(frontierClusterList,
					       attachedCluster, 
					       attachedCluster->clusterFrontierListItem);
		  attachedCluster->isFrontier = 1;
		}
              }
	    }
          }
  	}

        if (SCORECUSTOMLINKEDLIST_ISEMPTY(frontierClusterList)) {
	  traversal++;

	  for (i = 0; i < SCORECUSTOMLIST_LENGTH(headClusterList); i++) {
            ScoreCluster *headCluster;

	    SCORECUSTOMLIST_ITEMAT(headClusterList, i, headCluster);

	    if (!(headCluster->isScheduled)) {
	      if (!(headCluster->isFrontier)) {
                if (headCluster->isFreeable) {
		  // do nothing!
                } else {
                  SCORECUSTOMLINKEDLIST_DELITEM(waitingClusterList,
						headCluster->clusterWaitingListItem);
                }

                SCORECUSTOMSTACK_PUSH(frontierClusterTrial, headCluster);

                SCORECUSTOMLINKEDLIST_APPEND(frontierClusterList,
					     headCluster,
					     headCluster->clusterFrontierListItem);
                headCluster->isFrontier = 1;
              }
	    }
          }

	  // if, at this point, the frontier is still empty, then try to
	  // put one waiting list cluster on the frontier.
	  if (SCORECUSTOMLINKEDLIST_ISEMPTY(frontierClusterList)) {
	    if (!(SCORECUSTOMLINKEDLIST_ISEMPTY(waitingClusterList))) {
	      ScoreCluster *waitingCluster;

	      SCORECUSTOMLINKEDLIST_POP(waitingClusterList, waitingCluster);

	      waitingCluster->clusterWaitingListItem = 
		SCORECUSTOMLINKEDLIST_NULL;

	      SCORECUSTOMSTACK_PUSH(frontierClusterTrial, waitingCluster);

	      SCORECUSTOMLINKEDLIST_APPEND(frontierClusterList,
					   waitingCluster,
					   waitingCluster->clusterFrontierListItem);
	      waitingCluster->isFrontier = 1;
	    }
	  }
        }

        SCORECUSTOMSTACK_PUSH(frontierClusterTrial, NULL);
      }
#endif

      SCORECUSTOMSTACK_PUSH(numFreePageTrial, numFreePage);
      SCORECUSTOMSTACK_PUSH(numFreeMemSegTrial, numFreeMemSeg);
      SCORECUSTOMSTACK_PUSH(scheduledClusterTrial, currentCluster);
      SCORECUSTOMSTACK_PUSH(traversalTrial, traversal);

#if DOPROFILING_SCHEDULECLUSTERS
    endClock = threadCounter->read_tsc();
    diffClock = endClock - startClock;

    current_scheduleClusters_performTrials_reprioritize += diffClock;
#endif
    }

#if DOPROFILING_SCHEDULECLUSTERS
    sched_clusters_stat_prof->addSample(currentTimeslice, 
					PROF_performTrials,
					current_scheduleClusters_performTrials,
					DOPROFILING_VERBOSE);
    
    sched_clusters_stat_prof->
      addSample_perItem(PROF_performTrials,
			current_scheduleClusters_performTrials,
			profilerItemCount, DOPROFILING_VERBOSE);
    

    sched_clusters_stat_prof->
      addSample(currentTimeslice, 
		PROF_performTrials_reprioritize,
		current_scheduleClusters_performTrials_reprioritize,
		DOPROFILING_VERBOSE);
    
    sched_clusters_stat_prof->
      addSample_perItem(PROF_performTrials_reprioritize,
			current_scheduleClusters_performTrials_reprioritize,
			profilerItemCount, DOPROFILING_VERBOSE);



    startClock = threadCounter->read_tsc();
#endif
 
    // find the latest trial that had both physical page and memory segment
    // requirements >= 0.
    while (((numFreePage < 0) || (numFreeMemSeg < 0)) &&
	   (SCORECUSTOMSTACK_LENGTH(scheduledClusterTrial) > 0)) {
      ScoreCluster *unusedCluster;

      SCORECUSTOMSTACK_POP(scheduledClusterTrial, unusedCluster);

      SCORECUSTOMSTACK_POPONLY(numFreePageTrial);
      SCORECUSTOMSTACK_POPONLY(numFreeMemSegTrial);
      SCORECUSTOMSTACK_POPONLY(traversalTrial);
      SCORECUSTOMSTACK_FRONT(numFreePageTrial, numFreePage);
      SCORECUSTOMSTACK_FRONT(numFreeMemSegTrial, numFreeMemSeg);
      SCORECUSTOMSTACK_FRONT(traversalTrial, traversal);

      unusedCluster->isScheduled = 0;

#if FRONTIERLIST_USEPRIORITY
      SCORECUSTOMPRIORITYLIST_INSERT(frontierClusterList,
				     unusedCluster,
				     unusedCluster->lastCalculatedPriority,
				     unusedCluster->clusterFrontierListItem);
#else
      SCORECUSTOMLINKEDLIST_APPEND(frontierClusterList, unusedCluster,
				   unusedCluster->clusterFrontierListItem);
#endif
      unusedCluster->isFrontier = 1;

      {
        ScoreCluster *frontierClusterTrialFront;

        SCORECUSTOMSTACK_POPONLY(frontierClusterTrial);
        SCORECUSTOMSTACK_FRONT(frontierClusterTrial, frontierClusterTrialFront);

        while (frontierClusterTrialFront != NULL) {
          ScoreCluster *unusedFrontierCluster;

          SCORECUSTOMSTACK_POP(frontierClusterTrial, unusedFrontierCluster);
          SCORECUSTOMSTACK_FRONT(frontierClusterTrial, frontierClusterTrialFront);

	  // FIX ME! HACK TO GET AROUND NON-CONSTANT PRIORITIES!
#if FRONTIERLIST_USEPRIORITY
	  SCORECUSTOMPRIORITYLIST_DELITEM(frontierClusterList,
					  unusedFrontierCluster->clusterFrontierListItem);
#else
	  SCORECUSTOMLINKEDLIST_DELITEM(frontierClusterList,
					unusedFrontierCluster->clusterFrontierListItem);
#endif

          if (unusedFrontierCluster->isFreeable) {
	    // do nothing!
          } else {
	    SCORECUSTOMLINKEDLIST_APPEND(waitingClusterList, 
					 unusedFrontierCluster,
					 unusedFrontierCluster->clusterWaitingListItem);
          }

	  unusedFrontierCluster->isFrontier = 0;
        }
      }

      // mark all of the pages/segments in the cluster not isScheduled.
      for (i = 0; i < SCORECUSTOMLIST_LENGTH(unusedCluster->nodeList);
	   i++) {
	ScoreGraphNode *unusedNode;

	SCORECUSTOMLIST_ITEMAT(unusedCluster->nodeList, i, unusedNode);

	unusedNode->sched_isScheduled = 0;
      }

      // if an attached stitch buffer is no longer needed, then mark those
      //   stitch buffers as not isScheduled.
      // if input is done, then:
      //   --> (do nothing!)
      // else, if input is stitch segment, then:
      //   if input to stitch segment is done, then:
      //     --> unschedule stitch buffer.
      //   else, if input to stitch segment is operator, then:
      //     --> unschedule stitch buffer.
      //   else:
      //     if input to stitch segment isScheduled, then:
      //       --> (do nothing!)
      //     else, if input to stitch segment not isScheduled, then:
      //       --> unschedule stitch buffer.
      for (i = 0; i < SCORECUSTOMLIST_LENGTH(unusedCluster->inputList);
	   i++) {
	SCORE_STREAM attachedStream;
	ScoreGraphNode *attachedNode;
	ScoreSegment *attachedSegment;

	SCORECUSTOMLIST_ITEMAT(unusedCluster->inputList, i, attachedStream);
	attachedNode = attachedStream->sched_src;
	attachedSegment = (ScoreSegment *) attachedNode;

	if (attachedStream->sched_srcIsDone) {
	  // do nothing!
	} else if ((attachedStream->sched_srcFunc == STREAM_SEGMENT_TYPE) &&
		   attachedSegment->sched_isStitch) {
	  ScoreSegmentStitch *attachedStitch = 
	    (ScoreSegmentStitch *) attachedSegment;
	  SCORE_STREAM stitchSrcStream = attachedStitch->getSchedInStream();
	  ScoreGraphNode *stitchSrcNode = stitchSrcStream->sched_src;
	
	  if (stitchSrcStream->sched_srcIsDone) {
	    // mark the attached stitch buffer unscheduled.
	    attachedStitch->sched_isScheduled = 0;
	  } else if (stitchSrcStream->sched_srcFunc == STREAM_OPERATOR_TYPE) {
	    // mark the attached stitch buffer unscheduled.
	    attachedStitch->sched_isScheduled = 0;
	  } else {
	    // NOTE: Be careful about cluster self-loops. we will handle it
	    //       at the input, not the output.
	    if (stitchSrcNode->sched_parentCluster != unusedCluster) {
	      if (stitchSrcNode->sched_isScheduled) {
		// do nothing!
	      } else {
		// mark the attached stitch buffer unscheduled.
		attachedStitch->sched_isScheduled = 0;
	      }
	    } else {
	      // mark the attached stitch buffer unscheduled.
	      attachedStitch->sched_isScheduled = 0;
	    }
	  }
	}
      }
      // if output is done, then:
      //   --> (do nothing!)
      // else, if output is stitch segment, then:
      //   if output to stitch segment is done, then:
      //     --> unschedule stitch buffer.
      //   else, if output to stitch segment is operator, then:
      //     --> unschedule stitch buffer.
      //   else:
      //     if output to stitch segment isScheduled, then:
      //       --> (do nothing!)
      //     else, if output to stitch segment not isScheduled, then:
      //       --> unschedule stitch buffer.
      for (i = 0; i < SCORECUSTOMLIST_LENGTH(unusedCluster->outputList);
	   i++) {
	SCORE_STREAM attachedStream;
	ScoreGraphNode *attachedNode;
	ScoreSegment *attachedSegment;
      
	SCORECUSTOMLIST_ITEMAT(unusedCluster->outputList, i, attachedStream);
	attachedNode = attachedStream->sched_sink;
	attachedSegment = (ScoreSegment *) attachedNode;

	if (attachedStream->sched_sinkIsDone) {
	  // do nothing!
	} else if ((attachedStream->sched_snkFunc == STREAM_SEGMENT_TYPE) &&
		   attachedSegment->sched_isStitch) {
	  ScoreSegmentStitch *attachedStitch = 
	    (ScoreSegmentStitch *) attachedSegment;
	  SCORE_STREAM stitchSinkStream = attachedStitch->getSchedOutStream();
	  ScoreGraphNode *stitchSinkNode = stitchSinkStream->sched_sink;
	
	  if (stitchSinkStream->sched_sinkIsDone) {
	    // mark the attached stitch buffer unscheduled.
	    attachedStitch->sched_isScheduled = 0;
	  } else if (stitchSinkStream->sched_snkFunc == STREAM_OPERATOR_TYPE) {
	    // mark the attached stitch buffer unscheduled.
	    attachedStitch->sched_isScheduled = 0;
	  } else {
	    // NOTE: Be careful about cluster self-loops. we will handle it
	    //       at the input, not the output.
	    if (stitchSinkNode->sched_parentCluster != unusedCluster) {
	      if (stitchSinkNode->sched_isScheduled) {
		// do nothing!
	      } else {
		// mark the attached stitch buffer unscheduled.
		attachedStitch->sched_isScheduled = 0;
	      }
	    } else {
	      // do nothing!
	    }
	  }
	}
      }
    }

    // if, at this point, the physical requirements are still not met, then
    // we may have to back up to a point where either, we do not free the
    // freeable clusters, or do not free anything at all (except for done
    // pages/segments)!
    if ((numFreePage < 0) || (numFreeMemSeg < 0)) {
      SCORECUSTOMSTACK_POPONLY(numFreePageTrial);
      SCORECUSTOMSTACK_POPONLY(numFreeMemSegTrial);
      SCORECUSTOMSTACK_POPONLY(traversalTrial);
      SCORECUSTOMSTACK_FRONT(numFreePageTrial, numFreePage);
      SCORECUSTOMSTACK_FRONT(numFreeMemSegTrial, numFreeMemSeg);
      SCORECUSTOMSTACK_FRONT(traversalTrial, traversal);

      for (i = 0; i < SCORECUSTOMLIST_LENGTH(freeableClusterList); i++) {
	ScoreCluster *currentCluster;

	SCORECUSTOMLIST_ITEMAT(freeableClusterList, i, currentCluster);

	currentCluster->isScheduled = 1;

	currentCluster->isFreeable = 0;

	// mark all of the pages/segments in the cluster isScheduled.
	for (j = 0; j < SCORECUSTOMLIST_LENGTH(currentCluster->nodeList);
	     j++) {
	  ScoreGraphNode *currentNode;

	  SCORECUSTOMLIST_ITEMAT(currentCluster->nodeList, j, currentNode);

	  currentNode->sched_isScheduled = 1;
	}

	// if an attached stitch buffer is needed, then mark those
	// stitch buffers as isScheduled.
	// if input is done, then:
	//   --> (do nothing!)
	// else, if input is stitch segment, then:
	//   if input to stitch segment is done, then:
	//     --> schedule stitch buffer.
	//   else, if input to stitch segment is operator, then:
	//     --> schedule stitch buffer.
	//   else:
	//     if input to stitch segment isScheduled, then:
	//       --> (do nothing!)
	//     else, if input to stitch segment not isScheduled, then:
	//       --> schedule stitch buffer.
	for (j = 0; j < SCORECUSTOMLIST_LENGTH(currentCluster->inputList);
	     j++) {
	  SCORE_STREAM attachedStream;
	  ScoreGraphNode *attachedNode;
	  ScoreSegment *attachedSegment;
	  
	  SCORECUSTOMLIST_ITEMAT(currentCluster->inputList, j, attachedStream);
	  attachedNode = attachedStream->sched_src;
	  attachedSegment = (ScoreSegment *) attachedNode;

	  if (attachedStream->sched_srcIsDone) {
	    // do nothing!
	  } else if ((attachedStream->sched_srcFunc == STREAM_SEGMENT_TYPE) &&
		     attachedSegment->sched_isStitch) {
	    ScoreSegmentStitch *attachedStitch = 
	      (ScoreSegmentStitch *) attachedSegment;
	    SCORE_STREAM stitchSrcStream = attachedStitch->getSchedInStream();
	    ScoreGraphNode *stitchSrcNode = stitchSrcStream->sched_src;
	  
	    if (stitchSrcStream->sched_srcIsDone) {
	      // mark the attached stitch buffer scheduled.
	      attachedStitch->sched_isScheduled = 1;
	    } else if (stitchSrcStream->sched_srcFunc == STREAM_OPERATOR_TYPE) {
	      // mark the attached stitch buffer scheduled.
	      attachedStitch->sched_isScheduled = 1;
	    } else {
	      // NOTE: Be careful about cluster self-loops. we will handle it
	      //       at the input, not the output.
	      if (stitchSrcNode->sched_parentCluster != currentCluster) {
		if (stitchSrcNode->sched_isScheduled) {
		  // do nothing!
		} else {
		  // mark the attached stitch buffer scheduled.
		  attachedStitch->sched_isScheduled = 1;
		}
	      } else {
		// mark the attached stitch buffer scheduled.
		attachedStitch->sched_isScheduled = 1;
	      }
	    }
	  }
	}
	// if output is done, then:
	//   --> (do nothing!)
	// else, if output is stitch segment, then:
	//   if output to stitch segment is done, then:
	//     --> schedule stitch buffer.
	//   else, if output to stitch segment is operator, then:
	//     --> schedule stitch buffer.
	//   else:
	//     if output to stitch segment isScheduled, then:
	//       --> (do nothing!)
	//     else, if output to stitch segment not isScheduled, then:
	//       --> schedule stitch buffer.
	for (j = 0; j < SCORECUSTOMLIST_LENGTH(currentCluster->outputList);
	     j++) {
	  SCORE_STREAM attachedStream;
	  ScoreGraphNode *attachedNode;
	  ScoreSegment *attachedSegment;
	
	  SCORECUSTOMLIST_ITEMAT(currentCluster->outputList, j, attachedStream);
	  attachedNode = attachedStream->sched_sink;
	  attachedSegment = (ScoreSegment *) attachedNode;

	  if (attachedStream->sched_sinkIsDone) {
	    // do nothing!
	  } else if ((attachedStream->sched_snkFunc == STREAM_SEGMENT_TYPE) &&
		     attachedSegment->sched_isStitch) {
	    ScoreSegmentStitch *attachedStitch = 
	      (ScoreSegmentStitch *) attachedSegment;
	    SCORE_STREAM stitchSinkStream = attachedStitch->getSchedOutStream();
	    ScoreGraphNode *stitchSinkNode = stitchSinkStream->sched_sink;
	    
	    if (stitchSinkStream->sched_sinkIsDone) {
	      // mark the attached stitch buffer scheduled.
	      attachedStitch->sched_isScheduled = 1;
	    } else if (stitchSinkStream->sched_snkFunc == STREAM_OPERATOR_TYPE) {
	      // mark the attached stitch buffer scheduled.
	      attachedStitch->sched_isScheduled = 1;
	    } else {
	      // NOTE: Be careful about cluster self-loops. we will handle it
	      //       at the input, not the output.
	      if (stitchSinkNode->sched_parentCluster != currentCluster) {
		if (stitchSinkNode->sched_isScheduled) {
		  // do nothing!
		} else {
		  // mark the attached stitch buffer scheduled.
		  attachedStitch->sched_isScheduled = 1;
		}
	      } else {
		// do nothing!
	      }
	    }
	  }
	}
      }
      SCORECUSTOMLIST_CLEAR(freeableClusterList);

      if ((numFreePage < 0) || (numFreeMemSeg < 0)) {
	cerr << "SCHEDERR: Miscounting in the scheduling pass!" << endl;
      }
    }

#if DOPROFILING_SCHEDULECLUSTERS
    endClock = threadCounter->read_tsc();
    diffClock = endClock - startClock;

    sched_clusters_stat_prof->addSample(currentTimeslice,
				       PROF_backOutOfTrials,
				       diffClock,
				       DOPROFILING_VERBOSE);

#endif
  }

#if DOPROFILING_SCHEDULECLUSTERS
  profilerItemCount =
    SCORECUSTOMLIST_LENGTH(doneNodeList) + 
    SCORECUSTOMLIST_LENGTH(freeableClusterList) +
    SCORECUSTOMSTACK_LENGTH(scheduledClusterTrial);
  startClock = threadCounter->read_tsc();
#endif

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
  for (i = 0; i < SCORECUSTOMLIST_LENGTH(freeableClusterList); i++) {
    ScoreCluster *currentCluster;

    SCORECUSTOMLIST_ITEMAT(freeableClusterList, i, currentCluster);

    // make sure cluster is not still scheduled!
    if (!(currentCluster->isScheduled)) {
      for (j = 0; j < SCORECUSTOMLIST_LENGTH(currentCluster->nodeList);
	   j++) {
	ScoreGraphNode *currentNode;

	SCORECUSTOMLIST_ITEMAT(currentCluster->nodeList, j, currentNode);
      
	if (currentNode->isPage()) {
	  SCORECUSTOMLIST_APPEND(removedPageList,
				 (ScorePage *) currentNode);
	} else {
	  SCORECUSTOMLIST_APPEND(removedMemSegList,
				 (ScoreSegment *) currentNode);
	}
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

#if DOPROFILING_SCHEDULECLUSTERS
  endClock = threadCounter->read_tsc();
  diffClock = endClock - startClock;
  sched_clusters_stat_prof->addSample(currentTimeslice, 
				      PROF_addRemoveNodesOfClusters,
				      diffClock, DOPROFILING_VERBOSE);
    
  sched_clusters_stat_prof->addSample_perItem(PROF_addRemoveNodesOfClusters,
					      diffClock,
					      profilerItemCount,
					      DOPROFILING_VERBOSE);



  profilerItemCount = SCORECUSTOMLIST_LENGTH(stitchBufferList);
  startClock = threadCounter->read_tsc();
#endif
 
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

#if DOPROFILING_SCHEDULECLUSTERS
  endClock = threadCounter->read_tsc();
  diffClock = endClock - startClock;

  sched_clusters_stat_prof->addSample(currentTimeslice, 
				      PROF_handleStitchBuffersOld,
				      diffClock,
				      DOPROFILING_VERBOSE);
  
  sched_clusters_stat_prof->addSample_perItem(PROF_handleStitchBuffersOld,
					      diffClock,
					      profilerItemCount,
					      DOPROFILING_VERBOSE);

  profilerItemCount = 
    SCORECUSTOMLIST_LENGTH(freeableClusterList) +
    SCORECUSTOMSTACK_LENGTH(scheduledClusterTrial);
  startClock = threadCounter->read_tsc();
#endif

  // go through each cluster/page/segment in the freeable clusters list
  // and check to make sure that, if a stitch segment is required, then it
  // is instantiated and "stitched" in.
  // NOTE: Done pages/segments do not require a new stitch buffer since the
  //       streams that connect to them cannot be used again!
  for (i = 0; i < SCORECUSTOMLIST_LENGTH(freeableClusterList); i++) {
    ScoreCluster *currentCluster;

    SCORECUSTOMLIST_ITEMAT(freeableClusterList, i, currentCluster);
      
    // make sure cluster is not still scheduled!
    if (!(currentCluster->isScheduled)) {
      // if input is done, then:
      //   --> do nothing!
      // else, if input is operator, then:
      //   --> do nothing! should not happen currently!
      // else, if input is not stitch buffer, then:
      //   if input isResident and isScheduled:
      //     --> instantiate new stitch buffer.
      //   else, if input not isResident and isScheduled:
      //     --> (do nothing! newly scheduled cluster will handle this!)
      //   else, if input isResident and not isScheduled:
      //     --> (do nothing!)
      //   else, if input not isResident and not isScheduled:
      //     --> (do nothing!)
      for (j = 0; j < SCORECUSTOMLIST_LENGTH(currentCluster->inputList);
	   j++) {
	SCORE_STREAM attachedStream;
	ScoreGraphNode *attachedNode;
	ScoreSegment *attachedSegment;
      
	SCORECUSTOMLIST_ITEMAT(currentCluster->inputList, j, attachedStream);
	attachedNode = attachedStream->sched_src;
	attachedSegment = (ScoreSegment *) attachedNode;

	if (attachedStream->sched_srcIsDone) {
	  // do nothing!
	} else if (attachedStream->sched_srcFunc == STREAM_OPERATOR_TYPE) {
	  // do nothing! should not happen currently!
	} else if (!((attachedStream->sched_srcFunc == STREAM_SEGMENT_TYPE) &&
		     attachedSegment->sched_isStitch)) {
	  if (attachedNode->sched_isResident && 
	      attachedNode->sched_isScheduled) {
	    ScoreCluster *attachedCluster = attachedNode->sched_parentCluster;
	    ScoreStreamStitch *newStream = NULL;
	    int oldAttachedStreamSrcNum = attachedStream->sched_srcNum;
	    ScoreStreamType *oldAttachedStreamSrcType =
	      attachedNode->outputType(oldAttachedStreamSrcNum);
	    ScoreStream *oldStream = 
	      attachedNode->getSchedOutput(oldAttachedStreamSrcNum);
	  
	    // attempt to get a spare stream stitch!
	    if (SCORECUSTOMSTACK_LENGTH(spareStreamStitchList) > 0) {
	      ScoreStream *tempStream;

	      SCORECUSTOMSTACK_POP(spareStreamStitchList, tempStream);
	      newStream = (ScoreStreamStitch *) tempStream;
	    
	      newStream->recycle(attachedStream->get_width(),
				 attachedStream->get_fixed(),
				 ARRAY_FIFO_SIZE,
				 //attachedStream->get_length(),
				 attachedStream->get_type());
	    } else {
	      if (VERBOSEDEBUG || DEBUG) {
		cout << "SCHED: RAN OUT OF SPARE STITCH STREAMS! " <<
		  "INSTANTIATING A NEW ONE!" << endl;
	      }
	    
	      newStream = new ScoreStreamStitch(attachedStream->get_width(),
						attachedStream->get_fixed(),
						ARRAY_FIFO_SIZE,
						//attachedStream->get_length(),
						attachedStream->get_type());

	      newStream->reset();
	      newStream->sched_spareStreamStitchList = spareStreamStitchList;
	    }
	  
	    newStream->producerClosed = oldStream->producerClosed;
	    oldStream->producerClosed = 0;
	    newStream->producerClosed_hw = oldStream->producerClosed_hw;
	    oldStream->producerClosed_hw = 0;
	    newStream->sched_isCrossCluster = oldStream->sched_isCrossCluster;

	    attachedNode->unbindSchedOutput(oldAttachedStreamSrcNum);
	    attachedNode->bindSchedOutput(oldAttachedStreamSrcNum,
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

	      newStitch = new ScoreSegmentStitch(SCORE_DATASEGMENTBLOCK_LOADSIZE, 8,
						 NULL, NULL);
	      newStitch->reset();
	    }
	  
	    newStitch->recycle(SCORE_DATASEGMENTBLOCK_LOADSIZE/(SCORE_ALIGN_BITWIDTH_TO_8(attachedStream->get_width())/8),
			       attachedStream->get_width(),
			       newStream, attachedStream);

	    // FIX ME! I ASSUME THAT THE PROCESS TO ASSOCIATE THIS STITCH
	    //         BUFFER TO IS THE SAME AS THE ATTACHEDNODE.
	    newStitch->sched_parentProcess = attachedNode->sched_parentProcess;

	    // YM: assign a uniqTag right away
	    newStitch->uniqTag = newStitch->sched_parentProcess->getNextTag();
	  
	    newStitch->sched_residentStart = 0;
	    newStitch->sched_maxAddr = newStitch->length();
	    newStitch->sched_residentLength = newStitch->length();

	    // fix the output list of the attached cluster.
	    SCORECUSTOMLIST_REPLACE(attachedCluster->outputList, 
				    attachedStream, newStream);
	  
	    // add the new stitch buffer to the stitch buffer list.
	    SCORECUSTOMLIST_APPEND(stitchBufferList, newStitch);
	    SCORECUSTOMLIST_APPEND(newStitch->sched_parentProcess->stitchBufferList, newStitch);
	    newStitch->sched_parentProcess->numSegments++;
	  
	    newStitch->sched_old_mode = newStitch->sched_mode =
	      SCORE_CMB_SEQSINK;
	  
	    // add the new stitch buffer to the scheduled memory segment
	    // list.
	    newStitch->sched_isScheduled = 1;
	    SCORECUSTOMLIST_APPEND(scheduledMemSegList, newStitch);
	  } else if (!(attachedNode->sched_isResident) && 
		     attachedNode->sched_isScheduled) {
	    // do nothing!
	  } else if (attachedNode->sched_isResident &&
		     !(attachedNode->sched_isScheduled)) {
	    // do nothing!
	  } else if (!(attachedNode->sched_isResident) &&
		     !(attachedNode->sched_isScheduled)) {
	    // do nothing!
	  }
	}
      }
    }
  }
  for (i = 0; i < SCORECUSTOMSTACK_LENGTH(scheduledClusterTrial); i++) {
    ScoreCluster *currentCluster;

    SCORECUSTOMSTACK_ITEMAT(scheduledClusterTrial, i, currentCluster);

    // make sure cluster is not already resident!
    if (!(currentCluster->isResident)) {
      // if input is done, then:
      //   --> do nothing!
      // else, if input is operator, then:
      //   --> add stitch buffer! (and perhaps copy over any buffered stream
      //       tokens).
      // else:
      //   --> (do nothing!)
      for (j = 0; j < SCORECUSTOMLIST_LENGTH(currentCluster->inputList);
	   j++) {
	SCORE_STREAM attachedStream;
	ScoreGraphNode *currentNode;
	
	SCORECUSTOMLIST_ITEMAT(currentCluster->inputList, j, attachedStream);
	currentNode = attachedStream->sched_sink;
	
	if (attachedStream->sched_srcIsDone) {
	  // do nothing!
	} else if (attachedStream->sched_srcFunc == STREAM_OPERATOR_TYPE) {
	  // if our input is attached to the processor directly, then we
	  // must be being scheduled for the first time!
	  // in this case, we should put an intermediate stitch buffer in!
	  // this is because, it might be possible that the processor has
	  // already written tokens to the stream and these tokens should
	  // be placed in a stitch buffer on the array.
	  // NOTE: We have to keep the processor streams unaltered! So,
	  //       therefore, we will always change the stream on the array.
	  // FIX ME! WE MAY BE ABLE TO OPTIMIZE BY NOT ALWAYS HAVING A STITCH
	  //         BUFFER IF NO TOKENS HAVE BEEN PRODUCED 
	  //         YET BY THE PROCESSOR!
	  ScoreStreamStitch *newStream = NULL;
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
			       ARRAY_FIFO_SIZE,
			       //attachedStream->get_length(),
			       attachedStream->get_type());
	  } else {
	    if (VERBOSEDEBUG || DEBUG) {
	      cout << "SCHED: RAN OUT OF SPARE STITCH STREAMS! " <<
		"INSTANTIATING A NEW ONE!" << endl;
	    }
	    
	    newStream = new ScoreStreamStitch(attachedStream->get_width(),
					      attachedStream->get_fixed(),
					      ARRAY_FIFO_SIZE,
					      //attachedStream->get_length(),
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

	    newStitch = 
	      new ScoreSegmentStitch(SCORE_DATASEGMENTBLOCK_LOADSIZE, 8,
				     NULL, NULL);
	    newStitch->reset();
	  }
	  
	  newStitch->recycle(SCORE_DATASEGMENTBLOCK_LOADSIZE/(SCORE_ALIGN_BITWIDTH_TO_8(attachedStream->get_width())/8),
			     attachedStream->get_width(),
			     attachedStream, newStream);
	  
	  // FIX ME! I ASSUME THAT THE PROCESS TO ASSOCIATE THIS STITCH
	  //         BUFFER TO IS THE SAME AS THE CURRENTNODE.
	  newStitch->sched_parentProcess = currentNode->sched_parentProcess;
	  
	  // YM: assign a uniqTag right away
	  newStitch->uniqTag = newStitch->sched_parentProcess->getNextTag();

	  newStitch->sched_residentStart = 0;
	  newStitch->sched_maxAddr = newStitch->length();
	  newStitch->sched_residentLength = newStitch->length();
	  
	  // fix the input list of the current cluster.
	  // NOTE: cannot risk disturbing the linked list structure while
	  //       traversing it!
	  SCORECUSTOMLIST_ASSIGN(currentCluster->inputList, j, newStream);
	  
	  // add the new stitch buffer to the stitch buffer list.
	  SCORECUSTOMLIST_APPEND(stitchBufferList, newStitch);
	  SCORECUSTOMLIST_APPEND(newStitch->sched_parentProcess->stitchBufferList, newStitch);
	  newStitch->sched_parentProcess->numSegments++;
	  
	  newStitch->sched_old_mode = newStitch->sched_mode =
	    SCORE_CMB_SEQSRCSINK;
	  
	  // add the new stitch buffer to the scheduled memory segment
	  // list.
	  newStitch->sched_isScheduled = 1;
	  SCORECUSTOMLIST_APPEND(scheduledMemSegList, newStitch);

	  // FIX ME! WE SHOULD BE DOING SOMETHING LIKE COPYING THE
	  //         BUFFERED STREAM CONTENTS INTO THE SEGMENT AND PERHAPS
	  //         DISABLING WRITES TO THE STREAM UNTIL WE ACTUALLY
	  //         PUT THE STITCH BUFFER ON THE ARRAY!

	  // make sure the stitch buffer does not get cleaned up when empty!
	  newStitch->sched_mustBeInDataFlow = 1;
	} else {
	  // do nothing!
	}
      }
      // if output is done, then:
      //   --> do nothing!
      // else, if output is operator, then:
      //   --> add stitch buffer!
      // else, if output is not stitch buffer, then:
      //   if output isResident and isScheduled:
      //     --> (do nothing!)
      //   else, if output not isResident and isScheduled:
      //     --> (do nothing!)
      //   else, if output isResident and not isScheduled:
      //     --> instantiate new stitch buffer.
      //   else, if output not isResident and not isScheduled:
      //     --> instantiate new stitch buffer.
      for (j = 0; j < SCORECUSTOMLIST_LENGTH(currentCluster->outputList);
	   j++) {
	SCORE_STREAM attachedStream;
	ScoreGraphNode *currentNode;
	ScoreGraphNode *attachedNode;
	ScoreSegment *attachedSegment;
	
	SCORECUSTOMLIST_ITEMAT(currentCluster->outputList, j, attachedStream);
	currentNode = attachedStream->sched_src;
	attachedNode = attachedStream->sched_sink;
	attachedSegment = (ScoreSegment *) attachedNode;
	
	if (attachedStream->sched_sinkIsDone) {
	  // do nothing!
	} else if (attachedStream->sched_snkFunc == STREAM_OPERATOR_TYPE) {
	  // if our output is attached to the processor directly, then we
	  // must be being scheduled for the first time!
	  // in this case, we should put an intermediate stitch buffer in!
	  // this is because, it might be possible that the processor may not
	  // always be reading from the stream and the produced tokens should
	  // be placed in a stitch buffer on the array.
	  // NOTE: We have to keep the processor streams unaltered! So,
	  //       therefore, we will always change the stream on the array.
	  // FIX ME! WE MAY BE ABLE TO OPTIMIZE BY NOT ALWAYS HAVING A STITCH
	  //         BUFFER WHEN ARRAY AND PROCESSOR ARE IN SYNC!
	  ScoreStreamStitch *newStream = NULL;
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
			       ARRAY_FIFO_SIZE,
			       //attachedStream->get_length(),
			       attachedStream->get_type());
	  } else {
	    if (VERBOSEDEBUG || DEBUG) {
	      cout << "SCHED: RAN OUT OF SPARE STITCH STREAMS! " <<
		"INSTANTIATING A NEW ONE!" << endl;
	    }
	    
	    newStream = new ScoreStreamStitch(attachedStream->get_width(),
					      attachedStream->get_fixed(),
					      ARRAY_FIFO_SIZE,
					      //attachedStream->get_length(),
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

	  // YM: assign a uniqTag right away
	  newStitch->uniqTag = newStitch->sched_parentProcess->getNextTag();
	  
	  newStitch->sched_residentStart = 0;
	  newStitch->sched_maxAddr = newStitch->length();
	  newStitch->sched_residentLength = newStitch->length();
	  
	  // fix the output list of the current cluster.
	  // NOTE: cannot risk disturbing the linked list structure while
	  //       traversing it!
	  SCORECUSTOMLIST_ASSIGN(currentCluster->outputList, j, newStream);
	  
	  // add the new stitch buffer to the stitch buffer list.
	  SCORECUSTOMLIST_APPEND(stitchBufferList, newStitch);
	  SCORECUSTOMLIST_APPEND(newStitch->sched_parentProcess->stitchBufferList, newStitch);
	  newStitch->sched_parentProcess->numSegments++;
	  
	  newStitch->sched_old_mode = newStitch->sched_mode =
	    SCORE_CMB_SEQSRCSINK;
	  
	  // add the new stitch buffer to the scheduled memory segment
	  // list.
	  newStitch->sched_isScheduled = 1;
	  SCORECUSTOMLIST_APPEND(scheduledMemSegList, newStitch);
	  
	  // make sure the stitch buffer does not get cleaned up when empty!
	  newStitch->sched_mustBeInDataFlow = 1;
	} else if (!((attachedStream->sched_snkFunc == STREAM_SEGMENT_TYPE) &&
		     attachedSegment->sched_isStitch)) {
	  if (attachedNode->sched_isResident && 
	      attachedNode->sched_isScheduled) {
	    // do nothing!
	  } else if (!(attachedNode->sched_isResident) && 
		     attachedNode->sched_isScheduled) {
	    // do nothing!
	  } else if (attachedNode->sched_isResident &&
		     !(attachedNode->sched_isScheduled)) {
	    ScoreStreamStitch *newStream = NULL;
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
				 ARRAY_FIFO_SIZE,
				 //attachedStream->get_length(),
				 attachedStream->get_type());
	    } else {
	      if (VERBOSEDEBUG || DEBUG) {
		cout << "SCHED: RAN OUT OF SPARE STITCH STREAMS! " <<
		  "INSTANTIATING A NEW ONE!" << endl;
	      }
	      
	      newStream = new ScoreStreamStitch(attachedStream->get_width(),
						attachedStream->get_fixed(),
						ARRAY_FIFO_SIZE,
						//attachedStream->get_length(),
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
	    //         BUFFER TO IS THE SAME AS THE ATTACHEDNODE.
	    newStitch->sched_parentProcess = attachedNode->sched_parentProcess;

	    // YM: assign a uniqTag right away
	    newStitch->uniqTag = newStitch->sched_parentProcess->getNextTag();
	    
	    newStitch->sched_residentStart = 0;
	    newStitch->sched_maxAddr = newStitch->length();
	    newStitch->sched_residentLength = newStitch->length();
	    
	    // fix the output list of the current cluster.
	    SCORECUSTOMLIST_REPLACE(currentCluster->outputList, 
				    attachedStream, newStream);
	    
	    // add the new stitch buffer to the stitch buffer list.
	    SCORECUSTOMLIST_APPEND(stitchBufferList, newStitch);
	    SCORECUSTOMLIST_APPEND(newStitch->sched_parentProcess->stitchBufferList, newStitch);
	    newStitch->sched_parentProcess->numSegments++;
	    
	    newStitch->sched_old_mode = newStitch->sched_mode =
	      SCORE_CMB_SEQSRCSINK;
	    
	    // add the new stitch buffer to the scheduled memory segment
	    // list.
	    newStitch->sched_isScheduled = 1;
	    SCORECUSTOMLIST_APPEND(scheduledMemSegList, newStitch);
	  } else if (!(attachedNode->sched_isResident) &&
		     !(attachedNode->sched_isScheduled)) {
	    ScoreStreamStitch *newStream = NULL;
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
				 ARRAY_FIFO_SIZE,
				 //attachedStream->get_length(),
				 attachedStream->get_type());
	    } else {
	      if (VERBOSEDEBUG || DEBUG) {
		cout << "SCHED: RAN OUT OF SPARE STITCH STREAMS! " <<
		  "INSTANTIATING A NEW ONE!" << endl;
	      }
	      
	      newStream = new ScoreStreamStitch(attachedStream->get_width(),
						attachedStream->get_fixed(),
						ARRAY_FIFO_SIZE,
						//attachedStream->get_length(),
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
	    //         BUFFER TO IS THE SAME AS THE ATTACHEDNODE.
	    newStitch->sched_parentProcess = attachedNode->sched_parentProcess;

	    // YM: assign a uniqTag right away
	    newStitch->uniqTag = newStitch->sched_parentProcess->getNextTag();

	    
	    newStitch->sched_residentStart = 0;
	    newStitch->sched_maxAddr = newStitch->length();
	    newStitch->sched_residentLength = newStitch->length();
	    
	    // fix the output list of the current cluster.
	    SCORECUSTOMLIST_REPLACE(currentCluster->outputList, 
				    attachedStream, newStream);
	    
	    // add the new stitch buffer to the stitch buffer list.
	    SCORECUSTOMLIST_APPEND(stitchBufferList, newStitch);
	    SCORECUSTOMLIST_APPEND(newStitch->sched_parentProcess->stitchBufferList, newStitch);
	    newStitch->sched_parentProcess->numSegments++;
	    
	    newStitch->sched_old_mode = newStitch->sched_mode =
	      SCORE_CMB_SEQSRCSINK;
	    
	    // add the new stitch buffer to the scheduled memory segment
	    // list.
	    newStitch->sched_isScheduled = 1;
	    SCORECUSTOMLIST_APPEND(scheduledMemSegList, newStitch);
	  }
	}
      }
    }
  }

#if DOPROFILING_SCHEDULECLUSTERS
  endClock = threadCounter->read_tsc();
  diffClock = endClock - startClock;

  sched_clusters_stat_prof->addSample(currentTimeslice, 
				      PROF_handleStitchBuffersNew,
				      diffClock,
				      DOPROFILING_VERBOSE);
  
  sched_clusters_stat_prof->addSample_perItem(PROF_handleStitchBuffersNew,
					      diffClock,
					      profilerItemCount,
					      DOPROFILING_VERBOSE);

  profilerItemCount = 
    SCORECUSTOMLIST_LENGTH(freeableClusterList) + 
    SCORECUSTOMSTACK_LENGTH(scheduledClusterTrial);
  startClock = threadCounter->read_tsc();
#endif
 
  // FIX ME! SHOULD HAVE SOME WAY TO REVERSE STITCH BUFFERS! FOR EXAMPLE,
  // REDUCE THE REQUIREMENTS FOR THEM WHEN THEY ARE NOT NEEDED.

  // now that all the decisions about which clusters/pages are scheduled
  // and removed have been made, let's actually adjust the cluster lists to 
  // reflect this decision.
  for (i = 0; i < SCORECUSTOMLIST_LENGTH(freeableClusterList); i++) {
    ScoreCluster *currentCluster;

    SCORECUSTOMLIST_ITEMAT(freeableClusterList, i, currentCluster);

    currentCluster->isFreeable = 0;

    // make sure it is not still scheduled!
    if (!(currentCluster->isScheduled)) {
      currentCluster->isResident = 0;

      SCORECUSTOMLINKEDLIST_DELITEM(residentClusterList,
				    currentCluster->clusterResidentListItem);

      if (!(currentCluster->isFrontier)) {
	SCORECUSTOMLINKEDLIST_APPEND(waitingClusterList, currentCluster,
				     currentCluster->clusterWaitingListItem);
      }
    }
  }
  for (i = 0; i < SCORECUSTOMSTACK_LENGTH(scheduledClusterTrial); i++) {
    ScoreCluster *currentCluster;

    SCORECUSTOMSTACK_ITEMAT(scheduledClusterTrial, i, currentCluster);

    // make sure it is not already resident!
    if (!(currentCluster->isResident)) {
      currentCluster->isResident = 1;
      currentCluster->lastResidentTimeslice = currentTimeslice;
      currentCluster->lastFrontierTraversal = traversal;

      SCORECUSTOMLINKEDLIST_APPEND(residentClusterList, currentCluster,
				   currentCluster->clusterResidentListItem);
    }
  }

#if DOPROFILING_SCHEDULECLUSTERS
  endClock = threadCounter->read_tsc();
  diffClock = endClock - startClock;

  sched_clusters_stat_prof->addSample(currentTimeslice, 
				      PROF_updateClusterLists,
				      diffClock,
				      DOPROFILING_VERBOSE);
  
  sched_clusters_stat_prof->addSample_perItem(PROF_updateClusterLists,
					      diffClock,
					      profilerItemCount,
					      DOPROFILING_VERBOSE);

  startClock = threadCounter->read_tsc();
#endif

  currentNumFreeCPs = numFreePage;
  currentNumFreeCMBs = numFreeMemSeg;
  currentTraversal = traversal;

  SCORECUSTOMLIST_CLEAR(addedBufferLockStitchBufferList);
  SCORECUSTOMLIST_CLEAR(freeableClusterList);

  SCORECUSTOMSTACK_CLEAR(numFreePageTrial);
  SCORECUSTOMSTACK_CLEAR(numFreeMemSegTrial);
  SCORECUSTOMSTACK_CLEAR(traversalTrial);
  SCORECUSTOMSTACK_CLEAR(scheduledClusterTrial);
  SCORECUSTOMSTACK_CLEAR(frontierClusterTrial);

#if DOPROFILING_SCHEDULECLUSTERS
  endClock = threadCounter->read_tsc();
  diffClock = endClock - startClock;
  
  sched_clusters_stat_prof->addSample(currentTimeslice,
				      PROF_cleanup,
				      diffClock,
				      DOPROFILING_VERBOSE);

  if (DOPROFILING_VERBOSE) {
    cerr << "===========> SCHEDULECLUSTERS END" << endl;
  }
#endif

#if DOPROFILING
  endClock = threadCounter->read_tsc();

  crit_loop_prof->addSample(currentTimeslice, PROF_scheduleClusters,
			    endClock - startClock,
			    DOPROFILING_VERBOSE);

#endif
}


///////////////////////////////////////////////////////////////////////////////
// ScoreSchedulerDynamic::performPlacement:
//   Given scheduledPageList, scheduledMemSegList, removedPageList, and
//     removedMemSegList, doneNotRemovedPageList, doneNotRemovedMemSegList,
//     faultedMemSegList find the best placement for the pages.
//
// Parameters: none.
//
// Return value: none.
///////////////////////////////////////////////////////////////////////////////
void ScoreSchedulerDynamic::performPlacement() {
  unsigned int i, j;

  if (VERBOSEDEBUG || DEBUG) {
    cerr << "SCHED:    performPlacement()" << endl;
  }

#if DOPROFILING
  startClock = threadCounter->read_tsc();
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
  endClock = threadCounter->read_tsc();
  cerr << "****** PERFORMPLACMENT1: " << endClock-startClock << endl;

  startClock = threadCounter->read_tsc();
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

#if DOPROFILING
  endClock = threadCounter->read_tsc();
  crit_loop_prof->addSample(currentTimeslice, PROF_performPlacement,
			    endClock - startClock,
			    DOPROFILING_VERBOSE);
#endif
}


///////////////////////////////////////////////////////////////////////////////
// ScoreSchedulerDynamic::issueReconfigCommands:
//   Issue the reconfiguration commands to the array in order to load/dump
//     the correct pages/memory segments.
//
// Parameters: none.
//
// Return value: none.
///////////////////////////////////////////////////////////////////////////////
void ScoreSchedulerDynamic::
    issueReconfigCommands(unsigned long long sched_overhead) {

  if (VERBOSEDEBUG || DEBUG) {
    cerr << "SCHED:    issueReconfigCommands()" << endl;
  }

  unsigned int i;
  list<ScorePage *> dumpPageState_todo;
  list<ScorePage *> dumpPageFIFO_todo;
  list<ScoreSegment *> dumpSegmentFIFO_todo;
  list<ScorePage *> loadPageConfig_todo;
  list<ScorePage *> loadPageState_todo;
  list<ScorePage *> loadPageFIFO_todo;
  list<ScoreSegment *> loadSegmentFIFO_todo;

  // if there is nothing to do, then just return.
  if ((SCORECUSTOMLIST_LENGTH(scheduledPageList) == 0) &&
      (SCORECUSTOMLIST_LENGTH(scheduledMemSegList) == 0) &&
      (SCORECUSTOMLIST_LENGTH(removedPageList) == 0) &&
      (SCORECUSTOMLIST_LENGTH(removedMemSegList) == 0) &&
      (SCORECUSTOMLIST_LENGTH(configChangedStitchSegList) == 0) &&
      (SCORECUSTOMLIST_LENGTH(faultedMemSegList) == 0)) {

#if GET_FEEDBACK
    // stop every page and CMB.
    batchCommandBegin();
    for (i = 0; i < numPhysicalCP; i++) {
      if (arrayCP[i].active != NULL) {
	stopPage(arrayCP[i].active);
      }
    }
    for (i = 0; i < numPhysicalCMB; i++) {
      if (arrayCMB[i].active != NULL) {
	stopSegment(arrayCMB[i].active);
      }
    }
    batchCommandEnd();

    if (gFeedbackMode == MAKEFEEDBACK)
      makeFeedback();
    
    // start every page and CMB.
    // make sure that any done pages left on the array are not started again!
    batchCommandBegin();
    for (i = 0; i < numPhysicalCP; i++) {
      if ((arrayCP[i].active != NULL) &&
	  !(arrayCP[i].active->sched_isDone)) {
	startPage(arrayCP[i].active);
      }
    }
    for (i = 0; i < numPhysicalCMB; i++) {
      if (arrayCMB[i].active != NULL) {
	startSegment(arrayCMB[i].active);
      }
    }
    batchCommandEnd();
#endif
    return;
  }

  // stop every page and CMB.
  batchCommandBegin();
  for (i = 0; i < numPhysicalCP; i++) {
    if (arrayCP[i].active != NULL) {
      stopPage(arrayCP[i].active);
#if KEEPRECONFIGSTATISTICS
      reconfig_stat_prof->addSample(currentTimeslice, PROF_stopPage, 1);
#endif
    }
  }
  for (i = 0; i < numPhysicalCMB; i++) {
    if (arrayCMB[i].active != NULL) {
      stopSegment(arrayCMB[i].active);
#if KEEPRECONFIGSTATISTICS
      reconfig_stat_prof->addSample(currentTimeslice, PROF_stopSegment, 1);
#endif
    }
  }
  batchCommandEnd();

#if !PARALLEL_TIME
  // XXX: this will not be executed if this routine returns w/o doing anything
  // (lists are empty) 
  // DO NOT RUN WITH PARALLEL_TIME == 0
  advanceSimulatorTimeOffset(sched_overhead);
#endif


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
	reconfig_stat_prof->
	  addSample(currentTimeslice, PROF_getSegmentPointers, 1);
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
	reconfig_stat_prof->addSample(currentTimeslice, PROF_dumpPageState, 1);
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
	reconfig_stat_prof->addSample(currentTimeslice, PROF_dumpPageFIFO, 1);
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
	reconfig_stat_prof->
	  addSample(currentTimeslice, PROF_dumpSegmentFIFO, 1);
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
	reconfig_stat_prof->
	  addSample(currentTimeslice, PROF_memXferCMBToPrimary, 1);
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
	      reconfig_stat_prof->
		addSample(currentTimeslice, PROF_memXferCMBToPrimary, 1);
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
		(dumpStitchReadAddr*
		 (SCORE_ALIGN_BITWIDTH_TO_8(dumpStitch->width())/8));
	      void *dumpStitchBufferUpper =
		(void *) (((unsigned long long *) dumpBlock->buffer) +
			  dumpStitchReadAddr);

	      batchCommandBegin();
	      memXferCMBToPrimary(i,
				  dumpStitchStartLower,
				  dumpStitchSizeLower,
				  dumpStitchBufferLower);
#if KEEPRECONFIGSTATISTICS
	      reconfig_stat_prof->
		addSample(currentTimeslice, PROF_memXferCMBToPrimary, 1);
#endif
	      batchCommandEnd();

	      batchCommandBegin();
	      memXferCMBToPrimary(i,
				  dumpStitchStartUpper,
				  dumpStitchSizeUpper,
				  dumpStitchBufferUpper);
#if KEEPRECONFIGSTATISTICS
	      reconfig_stat_prof->
		addSample(currentTimeslice, PROF_memXferCMBToPrimary, 1);
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
	reconfig_stat_prof->
	  addSample(currentTimeslice, PROF_memXferPrimaryToCMB, 1);
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
	      (loadStitchReadAddr*
	       (SCORE_ALIGN_BITWIDTH_TO_8(loadStitch->width())/8));
	    void *loadStitchBuffer =
	      (void *) (((unsigned long long *) loadBlock->buffer) +
			loadStitchReadAddr);

	    batchCommandBegin();
	    memXferPrimaryToCMB(loadStitchBuffer,
				i,
				loadStitchStart,
				loadStitchSize);
#if KEEPRECONFIGSTATISTICS
	    reconfig_stat_prof->
	      addSample(currentTimeslice, PROF_memXferPrimaryToCMB, 1);
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
	      (loadStitchReadAddr*
	       (SCORE_ALIGN_BITWIDTH_TO_8(loadStitch->width())/8));
	    void *loadStitchBufferUpper =
	      (void *) (((unsigned long long *) loadBlock->buffer) +
			loadStitchReadAddr);

	    batchCommandBegin();
	    memXferPrimaryToCMB(loadStitchBufferLower,
				i,
				loadStitchStartLower,
				loadStitchSizeLower);
#if KEEPRECONFIGSTATISTICS
	    reconfig_stat_prof->
	      addSample(currentTimeslice, PROF_memXferPrimaryToCMB, 1);
#endif
	    batchCommandEnd();

	    batchCommandBegin();
	    memXferPrimaryToCMB(loadStitchBufferUpper,
				i,
				loadStitchStartUpper,
				loadStitchSizeUpper);
#if KEEPRECONFIGSTATISTICS
	    reconfig_stat_prof->
	      addSample(currentTimeslice, PROF_memXferPrimaryToCMB, 1);
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
    reconfig_stat_prof->
      addSample(currentTimeslice, PROF_changeSegmentTRAandPBOandMAX, 1);
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
      reconfig_stat_prof->
	addSample(currentTimeslice, PROF_setSegmentConfigPointers, 1);
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
	reconfig_stat_prof->
	  addSample(currentTimeslice, PROF_loadPageConfig, 1);
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
	reconfig_stat_prof->
	  addSample(currentTimeslice, PROF_loadPageState, 1);
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
	  reconfig_stat_prof->
	    addSample(currentTimeslice, PROF_loadPageFIFO, 1);
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
	  reconfig_stat_prof->
	    addSample(currentTimeslice, PROF_loadSegmentFIFO, 1);
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
    reconfig_stat_prof->
      addSample(currentTimeslice, PROF_changeSegmentMode, 1);
#endif
    batchCommandEnd();

    if ((currentSegment->sched_old_mode == SCORE_CMB_SEQSINK) &&
        (currentSegment->sched_mode == SCORE_CMB_SEQSRCSINK)) {
      batchCommandBegin();
      resetSegmentDoneFlag(currentSegment);
#if KEEPRECONFIGSTATISTICS
      reconfig_stat_prof->
	addSample(currentTimeslice, PROF_resetSegmentDoneFlag, 1);
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
	  reconfig_stat_prof->
	    addSample(currentTimeslice, PROF_connectStream, 1);
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
	  reconfig_stat_prof->
	    addSample(currentTimeslice, PROF_connectStream, 1);
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
      reconfig_stat_prof->addSample(currentTimeslice, PROF_startPage, 1);
#endif
    }
  }
  for (i = 0; i < numPhysicalCMB; i++) {
    if (arrayCMB[i].active != NULL) {
      startSegment(arrayCMB[i].active);
#if KEEPRECONFIGSTATISTICS
      reconfig_stat_prof->addSample(currentTimeslice, PROF_startSegment, 1);
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
}


///////////////////////////////////////////////////////////////////////////////
// ScoreSchedulerDynamic::performCleanup:
//   Perform any cleanup operations.
//
// Parameters: none.
//
// Return value: none.
///////////////////////////////////////////////////////////////////////////////
void ScoreSchedulerDynamic::performCleanup() {
  unsigned int i, j;

  if (VERBOSEDEBUG || DEBUG) {
    cerr << "SCHED:    performCleanup()" << endl;
  }

  // for each done page/segment, remove it from its operator, and 
  // process. if this node is the last node in the cluster, operator, or 
  // process, then remove those as well.
  // in addition, free any memory and return access to the user if necessary.
  for (i = 0; i < SCORECUSTOMLIST_LENGTH(doneClusterList); i++) {
    ScoreCluster *currentCluster;
    ScoreProcess *parentProcess;

    SCORECUSTOMLIST_ITEMAT(doneClusterList, i, currentCluster);
    parentProcess = currentCluster->parentProcess;

    SCORECUSTOMLIST_REMOVE(parentProcess->clusterList, currentCluster);
    delete(currentCluster);
    currentCluster = NULL;
  }
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
	  (SCORECUSTOMLIST_LENGTH(parentProcess->clusterList) == 0) &&
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
	  (SCORECUSTOMLIST_LENGTH(parentProcess->clusterList) == 0) &&
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
      cerr
	<< "SCHEDERR: WHOOPS! FOR MOST STITCH BUFFERS, THE STITCH STREAM "
	<< "SHOULD BE ON THE INPUT! NOT OUTPUT! "
	<< (unsigned int) emptyStitch << endl;
      cerr 
	<< "SCHEDERR: CANNOT EMPTY STITCH BUFFERS WITH STITCH ON OUTPUT! "
	<< "(ACTUALLY JUST FOR SIMULATION!)" << endl;
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
  SCORECUSTOMLIST_CLEAR(doneClusterList);
  SCORECUSTOMLIST_CLEAR(emptyStitchList);

}


#if FRONTIERLIST_USEPRIORITY
///////////////////////////////////////////////////////////////////////////////
// ScoreSchedulerDynamic::calculateClusterPriority:
//   Calculate the cluster priority based on some heuristic.
//
// Parameters:
//   currentCluster: the cluster whose priority should be checked.
//
// Return value:
//   none. the cluster priority is stored in lastCalculatedPriority.
///////////////////////////////////////////////////////////////////////////////
void ScoreSchedulerDynamic::calculateClusterPriority(
  ScoreCluster *currentCluster) {
  char hasInputs = 0;
  char hasOutputs = 0;
  unsigned int numIterationsBasedOnInputs = 0;
  // FIX ME! PRIORITIES!
#if 1
  unsigned int numIterationsBasedOnOutputs = 0;
#endif
  unsigned int maxNumIterations = 0;
  unsigned int numInputs = 0;
  unsigned int numOutputs = 0;
  unsigned int aggregateInputTokens = 0;
  unsigned int aggregateOutputTokens = 0;
  unsigned int avgNumInputTokens = 0;
  unsigned int avgNumOutputTokens = 0;
  unsigned int i;

  
  // FIX ME! THIS IS ONE WAY TO CALCULATE PRIORITY! BUT, THERE MIGHT BE
  //         BETTER WAYS!!! THERE ARE SEVERAL MISSED OPPORTUNITIES HERE!
  // FIX ME! WE MIGHT TRY TO MEMOIZE RESULTS THAT DON'T CHANGE!

  // input tokens can come from:
  //   - internal input FIFO.
  //   - attached src stitch buffer (and buffer's internal FIFO).
  //   - src cluster (directly, or indirectly through stitch buffer).
  // output tokens can go to:
  //   - attached stitch buffer (and buffer's internal FIFO).
  //   - sink cluster (directly, or indirectly through stitch buffer).
  //     (FIFO??)
  hasInputs = 0;
  for (i = 0; i < SCORECUSTOMLIST_LENGTH(currentCluster->inputList); i++) {
    ScoreStream *currentStream;
    ScoreGraphNode *sinkNode;
    unsigned int sinkNum;
    ScoreGraphNode *srcNode;
    unsigned int srcNum;
    unsigned int currentInputConsumption;
    unsigned int availableNumInputTokens = 0;
    unsigned int numIterationsBasedOnThisInput = 0;

    SCORECUSTOMLIST_ITEMAT(currentCluster->inputList, i, currentStream);
    sinkNode = currentStream->sched_sink;
    sinkNum = currentStream->sched_snkNum;
    srcNode = currentStream->sched_src;
    srcNum = currentStream->sched_srcNum;
    currentInputConsumption = sinkNode->sched_inputConsumption[sinkNum];

    if (currentInputConsumption > 0) {
      // get the number of input tokens available.
      availableNumInputTokens = 0;
      availableNumInputTokens = availableNumInputTokens +
	sinkNode->sched_lastKnownInputFIFONumTokens[sinkNum];
      if (!(currentStream->sched_srcIsDone)) {
	if (currentStream->sched_srcFunc != STREAM_OPERATOR_TYPE) {
	  if (srcNode->isSegment()) {
	    ScoreSegment *srcSegment = (ScoreSegment *) srcNode;
	    
	    // if this is a stitch buffer, add any buffered contents, PLUS
	    // anything in its input FIFO.
	    if (srcSegment->sched_isStitch) {
	      ScoreSegmentStitch *srcStitch = 
		(ScoreSegmentStitch *) srcSegment;
	      unsigned int startAddr = srcStitch->sched_readAddr;
	      unsigned int endAddr = srcStitch->sched_writeAddr;
	      
	      // if the endAddr has wrapped around below the startAddr,
	      // artificially "unwrap" it.
	      if (endAddr < startAddr) {
		endAddr = endAddr + srcStitch->length();
	      }
	      
	      availableNumInputTokens = availableNumInputTokens +
		(endAddr - startAddr);
	      
	      availableNumInputTokens = availableNumInputTokens +
		srcStitch->sched_lastKnownInputFIFONumTokens
		[SCORE_CMB_STITCH_DATAW_INNUM];
	      
	      currentStream = srcStitch->getSchedInStream();
	      srcNode = currentStream->sched_src;
	      srcNum = currentStream->sched_srcNum;
	    }
	  }
	}
      }
      if (!(currentStream->sched_srcIsDone)) {
	if (currentStream->sched_srcFunc != STREAM_OPERATOR_TYPE) {
	  ScoreCluster *srcCluster = srcNode->sched_parentCluster;

	  if (srcCluster->isScheduled) {
	    // since the source cluster is scheduled, it must have been
	    // through determineClusterPriority() also, and must have
	    // expected output production filled in also.
	    // NOTE: We do miss some opportunities here because, once the
	    //       cluster has been scheduled, we do not update it
	    //       statistical calculations when surround clusters are
	    //       added...
	    // NOTE: The extra "+ 1" is so that, even if no tokens have
	    //       been put through the system, we favor clusters whose
	    //       predecessors are all currently scheduled...
	    availableNumInputTokens = availableNumInputTokens +
	      srcNode->sched_expectedOutputProduction[srcNum] + 1;
	  }
	} else {
	  // NOTE: The extra "+ SCORE_TIMESLICE" is so that a cluster that is 
          //       directly input from the processor but has not been scheduled
          //       yet will not be pushed all the way down on the
          //       priority list!
          // NOTE: WE DO NOT KNOW EXACTLY HOW MUCH WILL BE PRODUCED BY THE
          //       PROCESSOR! WE ARE CURRENTLY ASSUMING IT CAN PRODUCE
          //       ALL TIMESLICE!
          availableNumInputTokens = availableNumInputTokens + SCORE_TIMESLICE;
	}
      }

      numIterationsBasedOnThisInput = 
	availableNumInputTokens / currentInputConsumption;
      
      if (!hasInputs) {
	numIterationsBasedOnInputs = numIterationsBasedOnThisInput;
	hasInputs = 1;
      } else {
	if (numIterationsBasedOnThisInput < numIterationsBasedOnInputs) {
	  numIterationsBasedOnInputs = numIterationsBasedOnThisInput;
	}
      }
    }
  }

  // FIX ME! PRIORITIES!
#if 1
  hasOutputs = 0;
  for (i = 0; i < SCORECUSTOMLIST_LENGTH(currentCluster->outputList);
       i++) {
    ScoreStream *currentStream;
    ScoreGraphNode *srcNode;
    unsigned int srcNum;
    ScoreGraphNode *sinkNode;
    ScoreSegment *sinkSegment;
    unsigned int currentOutputProduction;
    unsigned int availableNumOutputSpace = 0;
    unsigned int numIterationsBasedOnThisOutput = 0;

    SCORECUSTOMLIST_ITEMAT(currentCluster->outputList, i, currentStream);
    srcNode = currentStream->sched_src;
    srcNum = currentStream->sched_srcNum;
    sinkNode = currentStream->sched_sink;
    sinkSegment = (ScoreSegment *) sinkNode;
    currentOutputProduction = srcNode->sched_outputProduction[srcNum];

    if (currentOutputProduction > 0) {
      // get the output space available.
      availableNumOutputSpace = 0;
      if (!(currentStream->sched_sinkIsDone)) {
	if (currentStream->sched_snkFunc != STREAM_OPERATOR_TYPE) {
	  // if this is a stitch buffer, consider how much space is
	  // available in the buffer itself and its input FIFO.
	  if (sinkNode->isSegment() && sinkSegment->sched_isStitch) {
	    ScoreSegmentStitch *sinkStitch = 
	      (ScoreSegmentStitch *) sinkSegment;
	    unsigned int startAddr = sinkStitch->sched_readAddr;
	    unsigned int endAddr = sinkStitch->sched_writeAddr;
	      
	    // if the endAddr has wrapped around below the startAddr,
	    // artificially "unwrap" it.
	    if (endAddr < startAddr) {
	      endAddr = endAddr + sinkStitch->length();
	    }
	      
	    availableNumOutputSpace = availableNumOutputSpace +
	      (sinkStitch->length() - (endAddr - startAddr));
	  } else {
	    // since it is our policy to always place a stitch buffer on
	    // processor/array boundaries, we automatically gain the
	    // space from the stitch buffer and its input FIFO.
	    availableNumOutputSpace = availableNumOutputSpace +
	      (SCORE_DATASEGMENTBLOCK_LOADSIZE/
	       (SCORE_ALIGN_BITWIDTH_TO_8(currentStream->get_width())/8));
	    
	    // FIX ME! WHAT DO WE DO HERE??
	  }
	} else {
	  // since it is our policy to always place a stitch buffer on
	  // processor/array boundaries, we automatically gain the
	  // space from the stitch buffer and its input FIFO.
	  // FIX ME! MAKE SURE AMOUNT OF SPACE ACCOUNT FOR CORRECLTY IN
	  //         DIFFERENT BIT SIZES!
	  availableNumOutputSpace = availableNumOutputSpace +
	    (SCORE_DATASEGMENTBLOCK_LOADSIZE/
	     (SCORE_ALIGN_BITWIDTH_TO_8(currentStream->get_width())/8));
	    
	  // FIX ME! WHAT DO WE DO HERE??
	}
      }

      numIterationsBasedOnThisOutput = 
	availableNumOutputSpace / currentOutputProduction;
      
      if (!hasOutputs) {
	numIterationsBasedOnOutputs = numIterationsBasedOnThisOutput;
	hasOutputs = 1;
      } else {
	if (numIterationsBasedOnThisOutput < numIterationsBasedOnOutputs) {
	  numIterationsBasedOnOutputs = numIterationsBasedOnThisOutput;
	}
      }
    }
  }

  // find the maximum number of iterations possible by finding the minimum
  // of the numItereationsBasedOnInputs and numIterationsBasedOnOutputs.
  if (hasInputs && hasOutputs) {
    if (numIterationsBasedOnInputs < numIterationsBasedOnOutputs) {
      maxNumIterations = numIterationsBasedOnInputs;
    } else {
      maxNumIterations = numIterationsBasedOnOutputs;
    }
  } else if (!hasInputs) {
    maxNumIterations = numIterationsBasedOnOutputs;
  } else {
    maxNumIterations = numIterationsBasedOnInputs;
  }
#else
  maxNumIterations = numIterationsBasedOnInputs;
#endif
  
  // update the expected input/output consumption/production amounts.
  // calculate the average number of input/output tokens per input/output 
  // streams that will be consumed/produced.
  numInputs = SCORECUSTOMLIST_LENGTH(currentCluster->inputList);
  aggregateInputTokens = 0;
  for (i = 0; i < numInputs; i++) {
    ScoreStream *currentStream;
    ScoreGraphNode *sinkNode;
    unsigned int sinkNum;
    unsigned int currentInputConsumption;
    unsigned int expectedInputConsumption = 0;
    
    SCORECUSTOMLIST_ITEMAT(currentCluster->inputList, i, currentStream);
    sinkNode = currentStream->sched_sink;
    sinkNum = currentStream->sched_snkNum;
    currentInputConsumption = sinkNode->sched_inputConsumption[sinkNum];

    expectedInputConsumption = maxNumIterations*currentInputConsumption;

    aggregateInputTokens = aggregateInputTokens + expectedInputConsumption;
    
    sinkNode->sched_expectedInputConsumption[sinkNum] = 
      expectedInputConsumption;
  }
  if (numInputs > 0) {
    avgNumInputTokens = aggregateInputTokens / numInputs;
  } else {
    avgNumInputTokens = 0;
  }

  numOutputs = SCORECUSTOMLIST_LENGTH(currentCluster->outputList);
  aggregateOutputTokens = 0;
  for (i = 0; i < numOutputs; i++) {
    ScoreStream *currentStream;
    ScoreGraphNode *srcNode;
    unsigned int srcNum;
    unsigned int currentOutputProduction;
    unsigned int expectedOutputProduction = 0;
    
    SCORECUSTOMLIST_ITEMAT(currentCluster->outputList, i, currentStream);
    srcNode = currentStream->sched_src;
    srcNum = currentStream->sched_srcNum;
    currentOutputProduction = srcNode->sched_outputProduction[srcNum];

    expectedOutputProduction = maxNumIterations*currentOutputProduction;

    aggregateOutputTokens = aggregateOutputTokens + expectedOutputProduction;
    
    srcNode->sched_expectedOutputProduction[srcNum] = 
      expectedOutputProduction;
  }
  if (numOutputs > 0) {
    avgNumOutputTokens = aggregateOutputTokens / numOutputs;
  } else {
    avgNumOutputTokens = 0;
  }

  // FIX ME! IS THIS THE CORRECT WAY TO DO PRIORITY??
  if (hasOutputs) {
    currentCluster->lastCalculatedPriority = avgNumOutputTokens;
  } else {
    currentCluster->lastCalculatedPriority = avgNumInputTokens;
  }
}
#endif


///////////////////////////////////////////////////////////////////////////////
// ScoreSchedulerDynamic::findPotentiallyDeadLockedProcesses:
//   Finds processes which are potentially dead locked.
//
// Parameters:
//   none.
//
// Return value:
//   none.
///////////////////////////////////////////////////////////////////////////////
void ScoreSchedulerDynamic::findPotentiallyDeadLockedProcesses() {
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
// ScoreSchedulerDynamic::findDeadLock:
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
void ScoreSchedulerDynamic::findDeadLock(
  ScoreProcess *currentProcess,
  list<ScoreStream *> *bufferLockedStreams,
  list<list<ScoreStream *> *> *deadLockedCycles) {
  list_item listItem;
  list<ScoreStream *> traversedStreams;
  list<list<ScoreStream *> *> foundCycles;

  if (FINDDEADLOCK_VERBOSE) {
    cerr << "FINDDEADLOCK: Entering ScoreSchedulerDynamic::findDeadLock\n";
  }

  // built a processor node to represent the processor.
  // FIX ME! WE CURRENTLY REPRESENT THE PROCESSOR AS ONE LARGE NODE AND
  //         ASSUME THAT IT IS READING FROM AND WRITING TO ALL OF ITS IO
  //         STREAMS! THIS IS CONSERVATIVE AND INEFFICIENT!
  // FIX ME! WE ARE CURRENTLY "MAGICALLY" GETTING THE FULL/EMPTY STATUS
  //         OF PROCESSOR-ARRAY STREAMS! SHOULD FIGURE IT OUT FROM THE
  //         MEMORY BUFFER! !!! OR PERHAPS, FOR PROCESSOR->ARRAY STREAMS
  //         THEY SHOULD NEVER BE FULL! (AUTOMATICALLY EXPANDED).
  processorNode->
    setNumIO(SCORECUSTOMLIST_LENGTH(currentProcess->processorOStreamList), 
	     SCORECUSTOMLIST_LENGTH(currentProcess->processorIStreamList));
  for (unsigned int i = 0;
       i < SCORECUSTOMLIST_LENGTH(currentProcess->processorOStreamList);
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

  if (FINDDEADLOCK_VERBOSE) {
    cerr << "FINDDEADLOCK: processorOStreamList done\n";
  }
  for (unsigned int i = 0;
       i < SCORECUSTOMLIST_LENGTH(currentProcess->processorIStreamList);
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

  if (FINDDEADLOCK_VERBOSE) {
    cerr << "FINDDEADLOCK: processorIStreamList done\n";
  }


  // build up the current dependency graph.
  for (unsigned j = 0;
       j < SCORECUSTOMLIST_LENGTH(currentProcess->operatorList);
       j++) {
    ScoreOperatorInstance *currentOperator;

    SCORECUSTOMLIST_ITEMAT(currentProcess->operatorList, j, currentOperator);

    if (FINDDEADLOCK_VERBOSE) {
      cerr << "FINDDEADLOCK:    operator " << j << endl;
    }

    // check the inputs to see if it is being read in the current state, and
    // if so, if it has any tokens available.
    // check the outputs to see if it is being written in the current state,
    // and if so, if it has any space for tokens available.
    for (unsigned int i = 0; i < currentOperator->pages; i++) {
      ScorePage *currentPage = currentOperator->page[i];

      // ignore done pages.
      if (currentPage != NULL) {
	if (!(currentPage->sched_isDone)) {
	  unsigned int numInputs = currentPage->getInputs();
	  unsigned int numOutputs = currentPage->getOutputs();
	  int currentState = currentPage->sched_lastKnownState;
	  ScoreIOMaskType currentConsumed =
	    currentPage->inputs_consumed(currentState);
	  ScoreIOMaskType currentProduced = 
	    currentPage->outputs_produced(currentState);
	  
	  currentPage->sched_visited = 0;
	  currentPage->sched_visited2 = 0;
	  
	  for (unsigned int k = 0; k < numInputs; k++) {
	    char isBeingConsumed = (currentConsumed >> k) & 1;
	    
	    currentPage->sched_dependentOnInputBuffer[k] = 0;
	    
	    if (isBeingConsumed) {
	      SCORE_STREAM currentInput = currentPage->getSchedInput(k);
	      char hasInputTokens = !(currentInput->sched_isPotentiallyEmpty);
	      
	      if (!hasInputTokens) {
		currentPage->sched_dependentOnInputBuffer[k] = 1;
	      }
	    }
	  }
	  for (unsigned int k = 0; k < numOutputs; k++) {
	    char isBeingProduced = (currentProduced >> k) & 1;
	    
	    currentPage->sched_dependentOnOutputBuffer[k] = 0;
	    
	    if (isBeingProduced) {
	      SCORE_STREAM currentOutput = currentPage->getSchedOutput(k);
	      char hasOutputTokenSpace = 
		!(currentOutput->sched_isPotentiallyFull);
	      
	      if (!hasOutputTokenSpace) {
		currentPage->sched_dependentOnOutputBuffer[k] = 1;
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
    for (unsigned int i = 0; i < currentOperator->segments; i++) {
      ScoreSegment *currentSegment = currentOperator->segment[i];

      // ignore done segments.
      if (currentSegment != NULL) {
	if (!(currentSegment->sched_isDone)) {
	  unsigned int numInputs = currentSegment->getInputs();
	  unsigned int numOutputs = currentSegment->getOutputs();
	  
	  currentSegment->sched_visited = 0;
	  currentSegment->sched_visited2 = 0;
	  
	  for (unsigned int k = 0; k < numInputs; k++) {
	    char isBeingConsumed = 1;
	    
	    currentSegment->sched_dependentOnInputBuffer[k] = 0;
	    
	    if (isBeingConsumed) {
	      SCORE_STREAM currentInput = currentSegment->getSchedInput(k);
	      char hasInputTokens = !(currentInput->sched_isPotentiallyEmpty);
	      
	      if (!hasInputTokens) {
		currentSegment->sched_dependentOnInputBuffer[k] = 1;
	      }
	    }
	  }
	  for (unsigned int k = 0; k < numOutputs; k++) {
	    char isBeingProduced = 1;
	    
	    currentSegment->sched_dependentOnOutputBuffer[k] = 0;
	    
	    if (isBeingProduced) {
	      SCORE_STREAM currentOutput = currentSegment->getSchedOutput(k);
	      char hasOutputTokenSpace = 
		!(currentOutput->sched_isPotentiallyFull);
	      
	      if (!hasOutputTokenSpace) {
		currentSegment->sched_dependentOnOutputBuffer[k] = 1;
	      }
	    }
	  }
	}
      }
    }
  }


  if (FINDDEADLOCK_VERBOSE) {
    cerr << "FINDDEADLOCK: processorIStreamList done\n";
  }


  // THIS MIGHT BE SUPERFLUOUS TO DO ALL THE TIME. PERHAPS THIS SHOULD BE
  // SET WHEN THE SEGMENT IS INSTANTIATED? (BECAUSE SEGMENTS CAN BE THOUGHT
  // OF AS READING FROM ALL OF ITS INPUTS AND GENERATING ALL OF ITS OUTPUTS
  // EVERYTIME) (NOT ENTIRELY ACCURATE...)
  for (unsigned int i = 0;
       i < SCORECUSTOMLIST_LENGTH(currentProcess->stitchBufferList);
       i++) {
    ScoreSegment *currentStitch;

    SCORECUSTOMLIST_ITEMAT(currentProcess->stitchBufferList, i, currentStitch);

    // ignore done stitch buffers.
    if (!(currentStitch->sched_isDone)) {
      unsigned int numInputs = currentStitch->getInputs();
      unsigned int numOutputs = currentStitch->getOutputs();

      currentStitch->sched_visited = 0;
      currentStitch->sched_visited2 = 0;
      
      for (unsigned int j = 0; j < numInputs; j++) {
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
      for (unsigned int j = 0; j < numOutputs; j++) {
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

  if (FINDDEADLOCK_VERBOSE) {
    cerr << "FINDDEADLOCK: first step is complete\n";
  }

  {
    unsigned int numInputs = processorNode->getInputs();
    unsigned int numOutputs = processorNode->getOutputs();

    processorNode->sched_visited = 0;
    processorNode->sched_visited2 = 0;

    for (unsigned int j = 0; j < numInputs; j++) {
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
    for (unsigned int j = 0; j < numOutputs; j++) {
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

  if (FINDDEADLOCK_VERBOSE) {
    cerr << "FINDDEADLOCK: second step is complete\n";
  }

  // now, traverse the graph with a DFS and find loops.
  traversedStreams.clear();
  foundCycles.clear();
  deadLockedCycles->clear();
  for (unsigned int j = 0;
       j < SCORECUSTOMLIST_LENGTH(currentProcess->operatorList);
       j++) {
    ScoreOperatorInstance *currentOperator;

    SCORECUSTOMLIST_ITEMAT(currentProcess->operatorList, j, currentOperator);

    for (unsigned int i = 0; i < currentOperator->pages; i++) {
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
    for (unsigned int i = 0; i < currentOperator->segments; i++) {
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

  if (FINDDEADLOCK_VERBOSE) {
    cerr << "FINDDEADLOCK: third step completed: helpers ran\n";
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
    while (listItem2 != NULL) {
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

  if (FINDDEADLOCK_VERBOSE) {
    cerr << "FINDDEADLOCK: fifth step completed: processing foundCycles\n";
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

  if (FINDDEADLOCK_VERBOSE) {
    cerr << "FINDDEADLOCK: exiting\n";
  }
}


///////////////////////////////////////////////////////////////////////////////
// ScoreSchedulerDynamic::findDeadLockedStreams_traverse_helper:
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
void ScoreSchedulerDynamic::findDeadLockedStreams_traverse_helper(
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
    while (listItem != NULL) {
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
// ScoreSchedulerDynamic::resolveBufferLockedStreams:
//   Given a set of buffer locked streams, attempts to resolve the
//     buffer lock.
//
// Parameters:
//   bufferLockedStreams: list of buffer locked streams.
//
// Return value:
//   none.
///////////////////////////////////////////////////////////////////////////////
void ScoreSchedulerDynamic::resolveBufferLockedStreams(
  list<ScoreStream *> *bufferLockedStreams) {
  list_item listItem;

  // deal with each bufferlocked stream.
  forall_items(listItem, *bufferLockedStreams) {
    ScoreStream *currentStream = bufferLockedStreams->inf(listItem);

    if (1) {
      cerr << "BUFFERLOCK STREAM:\n";
      currentStream->print(stderr);
    }

    ScoreGraphNode *currentSrc = currentStream->sched_src;
    ScoreGraphNode *currentSink = currentStream->sched_sink;
    int currentSrcNum = currentStream->sched_srcNum;
    ScoreStreamType *currentSrcType = currentSrc->outputType(currentSrcNum);
    ScoreCluster *currentSrcCluster = currentSrc->sched_parentCluster;
    ScoreCluster *currentSinkCluster = currentSink->sched_parentCluster;

    if (EXTRA_DEBUG) {
      cerr << "RESOLVING BUFFERLOCK on stream " <<
	(long) currentStream << endl;
    }
    
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
			   ARRAY_FIFO_SIZE,
			   //currentStream->get_length(),
			   currentStream->get_type());
      } else {
	if (VERBOSEDEBUG || DEBUG) {
	  cout << "SCHED: RAN OUT OF SPARE STITCH STREAMS! " <<
	    "INSTANTIATING A NEW ONE!" << endl;
	}
	
	newStream = new ScoreStreamStitch(currentStream->get_width(),
					  currentStream->get_fixed(),
					  ARRAY_FIFO_SIZE,
					  //currentStream->get_length(),
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
	
      newStitch->
	recycle(SCORE_DATASEGMENTBLOCK_LOADSIZE/
		(SCORE_ALIGN_BITWIDTH_TO_8(currentStream->get_width())/8),
		currentStream->get_width(),
		newStream, currentStream);
      
      // FIX ME! I ASSUME THAT THE PROCESS TO ASSOCIATE THIS STITCH
      //         BUFFER TO IS THE SAME AS THE ATTACHEDNODE.
      newStitch->sched_parentProcess = currentSrc->sched_parentProcess;

      // YM: assign a uniqTag right away
      newStitch->uniqTag = newStitch->sched_parentProcess->getNextTag();

	      
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
      SCORECUSTOMLIST_APPEND(newStitch->sched_parentProcess->stitchBufferList,
			     newStitch);
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
			   ARRAY_FIFO_SIZE,
			   //currentStream->get_length(),
			   currentStream->get_type());
      } else {
	if (VERBOSEDEBUG || DEBUG) {
	  cout << "SCHED: RAN OUT OF SPARE STITCH STREAMS! " <<
	    "INSTANTIATING A NEW ONE!" << endl;
	}
	
	newStream = new ScoreStreamStitch(currentStream->get_width(),
					  currentStream->get_fixed(),
					  ARRAY_FIFO_SIZE,
					  //currentStream->get_length(),
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

      // YM: assign a uniqTag right away
      newStitch->uniqTag = newStitch->sched_parentProcess->getNextTag();


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

	cerr << "SCHEDERR: The cluster that caused the problem contains:\n";
	for (unsigned int listIndex = 0; 
	     listIndex < SCORECUSTOMLIST_LENGTH(currentSrcCluster->nodeList);
	     listIndex ++) {
	  cerr << "SCHEDERR: ";
	  ScoreGraphNode *node;
	  SCORECUSTOMLIST_ITEMAT(currentSrcCluster->nodeList, listIndex,
				 node);
	  if (node->isPage()) {
	    ScorePage *page = (ScorePage*) node;
	    cerr << "Page: \'" << page->getSource() << "\' numInputs = " <<
	      page->getInputs() << " numOutputs = " <<
	      page->getOutputs() << endl;
	  } else {
	    ScoreSegment *segment = (ScoreSegment *) node;
	    cerr << "Segment: numInputs = " << segment->getInputs() <<
	      " numOutputs = " << segment->getOutputs() << endl;
	  }
	}
	cerr << "SCHEDERR: The cluster requires " << 
	  currentSrcCluster->getNumPagesRequired() << " CPs and " <<
	  currentSrcCluster->getNumMemSegRequired() << " CMBs" << endl;
	cerr << "SCHEDERR: Array size: " << numPhysicalCP << " CPs and " <<
	  numPhysicalCMB << " CMBs" << endl;

	exit(1);
      }
    }
  }
}


///////////////////////////////////////////////////////////////////////////////
// ScoreSchedulerDynamic::resolveDeadLockedCycles:
//   Given a set of dead locked cycles, attempts to resolve the
//     dead lock.
//
// Parameters:
//   deadLockedCycles: list of dead locked streams.
//
// Return value:
//   none.
///////////////////////////////////////////////////////////////////////////////
void ScoreSchedulerDynamic::resolveDeadLockedCycles(
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

      cerr << "SCHED:      STREAM " << (long) currentStream << endl;
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
	  cerr << "(" << (long) currentStream->sched_src << ")(" <<
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
	  cerr << "(" << (long) currentStream->sched_sink << ")(" <<
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
// ScoreSchedulerDynamic::printCurrentState:
//   Prints out the current state of the scheduler.
//
// Parameters:
//   none.
//
// Return value:
//   none.
///////////////////////////////////////////////////////////////////////////////
void ScoreSchedulerDynamic::printCurrentState() {
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
	(long) opi << endl;

      cerr << "SCHED: PAGES===================" << endl;
      for (j = 0; j < opi->pages; j++) {
	if (opi->page[j] != NULL) {
	  cerr << "SCHED:    PAGE " << j << 
	    " (source=" << opi->page[j]->getSource() << ")" << 
	    " (state=" << opi->page[j]->sched_lastKnownState << ")" << 
	    "\t" << (long) opi->page[j] << endl;
	  
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
	    cerr << "\t"<< (long) opi->page[j]->getSchedInput(j);
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
	    cerr << "\t"<< (long) opi->page[j]->getSchedOutput(k);
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
	    "\t" << (long) opi->segment[j] << endl;
	  
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
	    cerr << "\t"<< (long) opi->segment[j]->getSchedInput(k);
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
	    cerr << "\t"<< (long) opi->segment[j]->getSchedOutput(k);
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
	"\t" << (long) currentStitch << endl;
      
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
	cerr << "\t"<< (long) currentStitch->getSchedInput(j);
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
	cerr << "\t"<< (long) currentStitch->getSchedOutput(j);
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
// ScoreSchedulerDynamic::visualizeCurrentState:
//   Dumps current state as a frame in the visualFile
//
// Parameters:
//   none.
//
// Return value:
//   none.
///////////////////////////////////////////////////////////////////////////////
void ScoreSchedulerDynamic::visualizeCurrentState(unsigned int timesliceNo,
						  bool confirmOutputNetlist) {
  
  if ((visualFile == NULL) && (!outputNetlist))
    return;

  if (timesliceNo == (unsigned int) -1)
    timesliceNo = currentTimeslice;

  FILE *fp = NULL;


  if (visualFile) {
    if ((fp = fopen(visualFile, "ab")) == NULL) {
      cerr << "WARNING: visualFile could not be opened, the simulation will proceed without visualization\n";
      delete visualFile;
      visualFile = NULL;
    }
  }

  unsigned int h, i, j, k;
  ScoreStateGraph stateGraph(timesliceNo);

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

 
  if (fp) {
    // dump the created graph to disk
    stateGraph.write (fp);
    
    fflush(fp);
    fclose(fp);
  }

  // output netlist for Randy
  if (outputNetlist && confirmOutputNetlist) {
    char namebuf[100];
    sprintf(namebuf, "%s/net.%d", netlistDirName, timesliceNo);

    FILE *f = fopen (namebuf, "w");
    if (!f) {
      fprintf(stderr, "unable to open netlist file \'%s\' for write\n", 
	      namebuf);
    } else {
      stateGraph.setCMBoffset(numPhysicalCP);
      stateGraph.writeNetlist(f, uniqNetlistRes);
      fflush(f);
      fclose(f);
    }
  }
}

#if GET_FEEDBACK
void ScoreSchedulerDynamic::makeFeedback() {
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
