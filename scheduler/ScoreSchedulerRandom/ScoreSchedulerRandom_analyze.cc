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
// ScoreSchedulerRandom::getCurrentStatus:
//   Get the current status of the physical array.
//
// Parameters: none.
//
// Return value: none.
///////////////////////////////////////////////////////////////////////////////
void ScoreSchedulerRandom::getCurrentStatus() {
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
// ScoreSchedulerRandom::gatherStatusInfo:
//   Given the array status, stores current array status info into scheduler
//     data structures.
//
// Parameters: none.
//
// Return value: none.
///////////////////////////////////////////////////////////////////////////////
void ScoreSchedulerRandom::gatherStatusInfo() {
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
// ScoreSchedulerRandom::findDonePagesSegments:
//   Look at the physical array status and determine which pages/segments are 
//     done. Place all done pages on the done page list.
//
// Parameters: none.
//
// Return value: none.
///////////////////////////////////////////////////////////////////////////////
void ScoreSchedulerRandom::findDonePagesSegments() {
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

      if (VERBOSEDEBUG || DEBUG || EXTRA_DEBUG) {
	cerr << "SCHED: EXPLICIT DONE PAGE: " << (unsigned int) donePage << 
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

      if (VERBOSEDEBUG || DEBUG || EXTRA_DEBUG) {
	cerr << "SCHED: EXPLICIT DONE SEGMENT: " << 
	  (unsigned int) doneSegment << endl;
      }

      // if this is a stitch buffer that is in SEQSINK mode, then ignore
      // this done signal! we will let it retransmit this done signal
      // when we flip the mode of the stitch buffer to SEQSRCSINK and the
      // contents have been allowed to drain.
      // YM: since stitch buffers do not report done when they received 
      // YM: eos in the seqsink mode, then do not bother checking for it
      // YM: simply verify that there are no lingering bugs
      assert(!(doneSegment->sched_isStitch &&
	       (doneSegment->sched_mode == SCORE_CMB_SEQSINK)));

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
// ScoreSchedulerRandom::checkImplicitDonePagesSegments:
//   Check to see if a page/segment is implicitly done. (not explicitly
//     signalling done).
//
// Parameters:
//   currentNode: node to check.
//
// Return value: 0 if false; 1 if true.
///////////////////////////////////////////////////////////////////////////////
int ScoreSchedulerRandom::checkImplicitDonePagesSegments(ScoreGraphNode 
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
// ScoreSchedulerRandom::findFaultedMemSeg:
//   Look at the physical array status and determine which memory segments
//     have faulted on their address. Place these memory segments on the
//     faulted segment list.
//
// Parameters: none.
//
// Return value: none.
///////////////////////////////////////////////////////////////////////////////
void ScoreSchedulerRandom::findFaultedMemSeg() {
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
	  cerr << "SCHEDERR: STITCH BUFFER [" << (unsigned int) faultedSegment << 
	    "] (SEQSRCSINK) BECAME FULL! DON'T KNOW WHAT TO DO!" << endl;
	} else if (faultedSegment->sched_mode == SCORE_CMB_SEQSINK) {
	  // FIX ME! WE SHOULD DECIDE WHETHER OR NOT TO INCREASE THE STITCH
	  // BUFFER SIZE! (WHAT IF IT IS A BUFFERLOCK STITCH BUFFER?)
	  cerr << "SCHEDERR: STITCH BUFFER [" << (unsigned int) faultedSegment << 
	    "] (SEQSINK) BECAME FULL! DON'T KNOW WHAT TO DO!" << endl;
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

	  // clear the status of the CMB location the segment is active in.
	  cmbStatus[i].clearStatus();

	  SCORECUSTOMLIST_APPEND(faultedMemSegList, faultedSegment);

#if 0
 	  SCORECUSTOMLINKEDLISTITEM listItem;
	  ScoreProcess *parentProcess = faultedSegment->sched_parentProcess;
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
#endif
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


#if RANDOM_SCHEDULER_VERSION == 1

///////////////////////////////////////////////////////////////////////////////
// ScoreSchedulerRandom::findFreeableClusters:
//   Look at the physical array status and determine which clusters are
//     freeable. Place all freeable clusters on the freeable cluster list.
//
// Parameters: none.
//
// Return value: none.
///////////////////////////////////////////////////////////////////////////////
void ScoreSchedulerRandom::findFreeableClusters() {
  SCORECUSTOMLINKEDLISTITEM listItem;
  unsigned int i;


#if DOPROFILING
  startClock = threadCounter->ScoreThreadSchCounterRead();
#endif

#if DOSCHEDULE_FOR_SCHEDTIME
  startClock = threadCounter->ScoreThreadSchCounterRead();
#endif

  // if there are no waiting clusters, then nothing should be freeable!

  if (!(SCORECUSTOMLIST_ISEMPTY(waitingClusterList))) {
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

#if DOSCHEDULE_FOR_SCHEDTIME
  endClock = threadCounter->ScoreThreadSchCounterRead();
  diffClock = endClock - startClock;
  current_findFreeableClusters = diffClock;
#endif

#if DOPROFILING
  endClock = threadCounter->ScoreThreadSchCounterRead();
  diffClock = endClock - startClock;
  if (diffClock < min_findFreeableClusters) {
    min_findFreeableClusters = diffClock;
  }
  if (diffClock > max_findFreeableClusters) {
    max_findFreeableClusters = diffClock;
  }
  total_findFreeableClusters = total_findFreeableClusters + diffClock;
  current_findFreeableClusters = diffClock;
  cerr << "   findFreeableClusters() ==> " << 
    diffClock <<
    " cycle(s)" << endl;
#endif
}

#endif // if RANDOM_SCHEDULER_VERSION == 1
