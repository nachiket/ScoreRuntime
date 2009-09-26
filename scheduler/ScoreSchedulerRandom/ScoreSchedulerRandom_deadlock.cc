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
// ScoreSchedulerRandom::dealWithDeadLock:
//   Determine if any processes have deadlocked. If so, deal with it!
//
// Parameters: none.
//
// Return value: none.
///////////////////////////////////////////////////////////////////////////////
void ScoreSchedulerRandom::dealWithDeadLock() {
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
// ScoreSchedulerRandom::findPotentiallyDeadLockedProcesses:
//   Finds processes which are potentially dead locked.
//
// Parameters:
//   none.
//
// Return value:
//   none.
///////////////////////////////////////////////////////////////////////////////
void ScoreSchedulerRandom::findPotentiallyDeadLockedProcesses() {
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
// ScoreSchedulerRandom::findDeadLock:
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
void ScoreSchedulerRandom::findDeadLock(
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
// ScoreSchedulerRandom::findDeadLockedStreams_traverse_helper:
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
void ScoreSchedulerRandom::findDeadLockedStreams_traverse_helper(
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
// ScoreSchedulerRandom::resolveBufferLockedStreams:
//   Given a set of buffer locked streams, attempts to resolve the
//     buffer lock.
//
// Parameters:
//   bufferLockedStreams: list of buffer locked streams.
//
// Return value:
//   none.
///////////////////////////////////////////////////////////////////////////////
void ScoreSchedulerRandom::resolveBufferLockedStreams(
  list<ScoreStream *> *bufferLockedStreams) {
  list_item listItem;

  if (PRINTSTATE) {
    cerr << "<<<<<<<<<<<<<<<<<<resolveBufferLockedStreams>>>>>>>>>>>>>>>>>>>>>\n";
  }

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
	      
#if 1
      // fix the input/output list of the cluster.
      SCORECUSTOMLIST_APPEND(currentSrcCluster->inputList, currentStream);
      SCORECUSTOMLIST_APPEND(currentSrcCluster->outputList, newStream);
#else
      // make the stitch used to resolve bufferlock be a part of the 
      // cluster (since src and srk clusters are the same)
      currentSrcCluster->addNode(newStitch);
#endif
	      
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
// ScoreSchedulerRandom::resolveDeadLockedCycles:
//   Given a set of dead locked cycles, attempts to resolve the
//     dead lock.
//
// Parameters:
//   deadLockedCycles: list of dead locked streams.
//
// Return value:
//   none.
///////////////////////////////////////////////////////////////////////////////
void ScoreSchedulerRandom::resolveDeadLockedCycles(
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

