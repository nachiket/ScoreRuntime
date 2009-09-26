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
// $Revision: 1.7 $
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

// a pointer to the visualFile where state graphs are written
extern char *visualFile;

extern const char *segment_modes[];

///////////////////////////////////////////////////////////////////////////////
// ScoreSchedulerRandom::getCurrentTimeslice:
//   Get the current timeslice.
//
// Parameters: none.
//
// Return value: 
//   the current timeslice.
///////////////////////////////////////////////////////////////////////////////
unsigned int ScoreSchedulerRandom::getCurrentTimeslice() {
  return(currentTimeslice);
}

///////////////////////////////////////////////////////////////////////////////
// ScoreSchedulerRandom::performCleanup:
//   Perform any cleanup operations.
//
// Parameters: none.
//
// Return value: none.
///////////////////////////////////////////////////////////////////////////////
void ScoreSchedulerRandom::performCleanup() {
  unsigned int i, j;

#if PROFILE_PERFORMCLEANUP
  unsigned long long startTime, endTime;
#endif

#if 0 && DOPROFILING
  startClock = threadCounter->ScoreThreadSchCounterRead();
#endif

  // for each done page/segment, remove it from its operator, and 
  // process. if this node is the last node in the cluster, operator, or 
  // process, then remove those as well.
  // in addition, free any memory and return access to the user if necessary.
  if (DEBUG|| PRINTSTATE) {
    cerr << "CLEANUP: doneCluster size = " << SCORECUSTOMLIST_LENGTH(doneClusterList) << endl;
  }
  for (i = 0; i < SCORECUSTOMLIST_LENGTH(doneClusterList); i++) {
    ScoreCluster *currentCluster;
    ScoreProcess *parentProcess;

    SCORECUSTOMLIST_ITEMAT(doneClusterList, i, currentCluster);
    parentProcess = currentCluster->parentProcess;

    SCORECUSTOMLIST_REMOVE(parentProcess->clusterList, currentCluster);

    // FIXME (YM) temporary solution to eliminate seg faults on head
    // FIXME (YM) list traversals
    if (currentCluster->isHead) {
      SCORECUSTOMLIST_REMOVE(headClusterList, currentCluster);
    }
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
	if (EXTRA_DEBUG)
	  cerr << "<<<<<<<<<<<<dlClose = " << oldHandle << ">>>>>>>>>>>>>>>>\n";
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
  SCORECUSTOMLIST_CLEAR(doneClusterList);
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
// ScoreSchedulerRandom::printCurrentState:
//   Prints out the current state of the scheduler.
//
// Parameters:
//   none.
//
// Return value:
//   none.
///////////////////////////////////////////////////////////////////////////////
void ScoreSchedulerRandom::printCurrentState() {
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
	"\t" << (unsigned int) currentStitch << " {" <<
	segment_modes[currentStitch->sched_mode] << "}" << endl;
      
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
// ScoreSchedulerRandom::visualizeCurrentState:
//   Dumps current state as a frame in the visualFile
//
// Parameters:
//   none.
//
// Return value:
//   none.
///////////////////////////////////////////////////////////////////////////////
void ScoreSchedulerRandom::visualizeCurrentState() {
  
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
	if (opi->page[j] != NULL) {
	  opi->page[j]->sched_isResident = 
	    (opi->page[j]->getLastTimesliceScheduled() == currentTimeslice);
	  stateGraph.addNode(currentProcess->pid, j, opi->page[j]);
	}
      }
      for (j = 0; j < opi->segments; j++) {
	if (opi->segment[j] != NULL) {
	  opi->segment[j]->sched_isResident = 
	    (opi->segment[j]->getLastTimesliceScheduled() == currentTimeslice);
	  stateGraph.addNode(currentProcess->pid, j, opi->segment[j]);
	}
      }
    }

    for (i = 0; i < SCORECUSTOMLIST_LENGTH(currentProcess->stitchBufferList);
	 i++) {
      ScoreSegmentStitch *currentStitch;

      SCORECUSTOMLIST_ITEMAT(currentProcess->stitchBufferList, i, currentStitch);
      currentStitch->sched_isResident = 
	(currentStitch->getLastTimesliceScheduled() == currentTimeslice) &&
	(currentStitch->sched_lastTimesliceConfigured == currentTimeslice);

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
void ScoreSchedulerRandom::makeFeedback() {
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
	    ScoreGraphNode *mynode = graphNode[j];
	    mynode->feedbackNode->recordConsumption(mynode->getConsumptionVector(),
						    mynode->getInputs());
	    mynode->feedbackNode->recordProduction(mynode->getProductionVector(),
						   mynode->getOutputs());
	    mynode->feedbackNode->recordFireCount(mynode->getFire());
	  }
	} // iterate through all nodes within a op instance
      } // first do pages, then segments
    } // go through all operators
  } // go through all processes
}
#endif

void ScoreSchedulerRandom::printSchedState()
{
  unsigned int i;
#if 0
  cerr << "SCHED: HEAD CLUSTER LIST HAS " << 
    SCORECUSTOMLIST_LENGTH(headClusterList) << " CLUSTERS" << endl;
  for (i = 0; i < SCORECUSTOMLIST_LENGTH(headClusterList); i++) {
    ScoreCluster *currentCluster;
    
    SCORECUSTOMLIST_ITEMAT(headClusterList, i, currentCluster);
    
    cerr << "SCHED:    CLUSTER " << (unsigned int) currentCluster << endl;
    for (j = 0; j < SCORECUSTOMLIST_LENGTH(currentCluster->nodeList);
	 j++) {
      ScoreGraphNode *currentNode;
      
      SCORECUSTOMLIST_ITEMAT(currentCluster->nodeList, j, currentNode);
      
      if (currentNode->isPage()) {
	cerr << "SCHED:       PAGE ";
      } else if (currentNode->isSegment()) {
	cerr << "SCHED:       SEGMENT ";
      } else {
	cerr << "SCHED:       UNKNOWN ";
      }
      
      cerr << (unsigned int) currentNode << endl;
    }
  }
#endif 

  cerr << "SCHED: DONE CLUSTER LIST HAS " << 
    SCORECUSTOMLINKEDLIST_LENGTH(doneClusterList) << " CLUSTERS" << endl;
  
  for(unsigned listIndex = 0; 
      listIndex < SCORECUSTOMLIST_LENGTH(doneClusterList); listIndex ++) {
    ScoreCluster *currentCluster;
    
    SCORECUSTOMLIST_ITEMAT(doneClusterList, listIndex, currentCluster);
    
    cerr << "SCHED:    CLUSTER " << (unsigned int) currentCluster << endl;
    for (i = 0; i < SCORECUSTOMLIST_LENGTH(currentCluster->nodeList);
	 i++) {
      ScoreGraphNode *currentNode;
      
      SCORECUSTOMLIST_ITEMAT(currentCluster->nodeList, i, currentNode);
      
      if (currentNode->isPage()) {
	cerr << "SCHED:       PAGE ";
      } else if (currentNode->isSegment()) {
	cerr << "SCHED:       SEGMENT ";
      } else {
	cerr << "SCHED:       UNKNOWN ";
      }
      cerr << (unsigned int) currentNode << 
	"[cluster = " << ((unsigned int)(currentNode->sched_parentCluster)) << "]" << endl;
    }
  }


  cerr << "SCHED: RESIDENT CLUSTER LIST HAS " << 
    SCORECUSTOMLINKEDLIST_LENGTH(residentClusterList) << " CLUSTERS" << endl;
  
  SCORECUSTOMLINKEDLISTITEM it;
  SCORECUSTOMLINKEDLIST_HEAD(residentClusterList, it);
  while (it != SCORECUSTOMLINKEDLIST_NULL) {
    ScoreCluster *currentCluster;
    
    SCORECUSTOMLINKEDLIST_ITEMAT(residentClusterList, it, currentCluster);
    
    cerr << "SCHED:    CLUSTER " << (unsigned int) currentCluster << endl;
    for (i = 0; i < SCORECUSTOMLIST_LENGTH(currentCluster->nodeList);
	 i++) {
      ScoreGraphNode *currentNode;
      
      SCORECUSTOMLIST_ITEMAT(currentCluster->nodeList, i, currentNode);
      
      if (currentNode->isPage()) {
	cerr << "SCHED:       PAGE ";
      } else if (currentNode->isSegment()) {
	cerr << "SCHED:       SEGMENT ";
      } else {
	cerr << "SCHED:       UNKNOWN ";
      }
      cerr << (unsigned int) currentNode << 
	"[cluster = " << ((unsigned int)(currentNode->sched_parentCluster)) << "]" << endl;
    }
    SCORECUSTOMLINKEDLIST_GOTONEXT(residentClusterList, it);
  }

  cerr << "SCHED: WAITING CLUSTER LIST HAS " << 
    SCORECUSTOMLIST_LENGTH(waitingClusterList) << " CLUSTERS" << endl;
  for (unsigned int listIndex = 0;
       listIndex < SCORECUSTOMLIST_LENGTH(waitingClusterList);
       listIndex ++) {
    ScoreCluster *currentCluster;
    
    SCORECUSTOMLIST_ITEMAT(waitingClusterList, listIndex, currentCluster);
    
    cerr << "SCHED:    CLUSTER " << (unsigned int) currentCluster << endl;
    for (i = 0; i < SCORECUSTOMLIST_LENGTH(currentCluster->nodeList);
	 i++) {
      ScoreGraphNode *currentNode;
      
      SCORECUSTOMLIST_ITEMAT(currentCluster->nodeList, i, currentNode);
      
      if (currentNode->isPage()) {
	cerr << "SCHED:       PAGE ";
      } else if (currentNode->isSegment()) {
	cerr << "SCHED:       SEGMENT ";
      } else {
	cerr << "SCHED:       UNKNOWN ";
      }
      cerr << (unsigned int) currentNode << 
	"[cluster = " << ((unsigned int)(currentNode->sched_parentCluster)) << "]" << endl;
    }
  }
}

void ScoreSchedulerRandom::printPlacementState() 
{
  unsigned int i;

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
  cerr << "SCHED: NUMBER OF CHANGED CONFIG MEMSEG: " << 
    SCORECUSTOMLIST_LENGTH(configChangedStitchSegList) << endl;
  for (i = 0; i < SCORECUSTOMLIST_LENGTH(configChangedStitchSegList); i++) {
    ScoreSegment *currentMemSeg;
    
    SCORECUSTOMLIST_ITEMAT(configChangedStitchSegList, i, currentMemSeg);
    
    cerr << "SCHED:    REMOVED MEMSEG: " << 
      (unsigned int) currentMemSeg << endl;
  }
}
