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
// SCORE Simulator
// $Revision: 1.88 $
//
//////////////////////////////////////////////////////////////////////////////

#include <math.h>
#include "ScoreStream.h"
#include "ScoreSimulator.h"
#include "ScoreVisualization.h"
#include "ScoreConfig.h"

extern FILE *gReconfigAcctFile;

#if GET_FEEDBACK
#include "ScoreFeedbackGraph.h"
extern ScoreFeedbackMode gFeedbackMode;
extern int gFeedbackSampleFreq;
extern ScoreFeedbackGraph *gFeedbackObj;
#endif

#if TRACK_LAST_VISITED_NODE
ScoreGraphNode *lastVisitedSimNode = 0;
#endif

//////////////////////////////////////////////////////////////////////////////
// ScoreSimulator::ScoreSimulator:
//   Constructor for ScoreSimulator.
//   Initializes all internal structures.
//
// Parameters: None.
//
// Return value: None.
//////////////////////////////////////////////////////////////////////////////
ScoreSimulator::ScoreSimulator(ScoreStream *input, ScoreStream *output) {

  if (TIMEACC) {
    threadCounter = new ScoreThreadCounter(SIMULATOR);
    threadCounter->ScoreThreadCounterEnable(SIMULATOR);
  } else {
    threadCounter = NULL;
  }

  unsigned index;

  if (TIMEACC)
    simCycle = &(threadCounter->record->simCycle);
  else
    simCycle = new unsigned int;

  *simCycle = 0;
  currTimeSlice = 0;
  inputStream = input;
  outputStream = output;
  needToken = 0;

  saArray = new ScorePage *[numPhysicalPages];
  saArrayPhysicalStatus = new ScoreArrayPhysicalStatus *[numPhysicalPages];
  cmbArray = new ScoreSegment *[numPhysicalSegments];
  cmbArrayPhysicalStatus = new ScoreArrayPhysicalStatus *[numPhysicalSegments];
  lastVisualizationEventCP = new int[numPhysicalPages];
  currentVisualizationEventCP = new int[numPhysicalPages];
  lastVisualizationEventCMB = new int[numPhysicalSegments];
  currentVisualizationEventCMB = new int[numPhysicalSegments];

  for (index = 0; index < numPhysicalPages; index++) {
    saArray[index] = NULL;
    saArrayPhysicalStatus[index] = NULL;
    lastVisualizationEventCP[index] = VISUALIZATION_EVENT_IDLE;
    currentVisualizationEventCP[index] = VISUALIZATION_EVENT_IDLE;
  } 

  for (index = 0; index < numPhysicalSegments; index++) {
    cmbArray[index] = NULL;
    cmbArrayPhysicalStatus[index] = new ScoreArrayPhysicalStatus(1,1);
    lastVisualizationEventCMB[index] = VISUALIZATION_EVENT_IDLE;
    currentVisualizationEventCMB[index] = VISUALIZATION_EVENT_IDLE;
  } 

  for (index = 0; index < NumberOf_SCORE_EVENT; index++) {
    statsOfSchedToSimCommands[index] = 0;
  }
}


ScoreSimulator::~ScoreSimulator() {
  unsigned int i;

  delete(saArray);
  for (i = 0; i < numPhysicalPages; i++) {
    if (saArrayPhysicalStatus[i] != NULL) {
      delete(saArrayPhysicalStatus[i]);
    }
  }
  delete(saArrayPhysicalStatus);
  delete(cmbArray);
  for (i = 0; i < numPhysicalSegments; i++) {
    if (cmbArrayPhysicalStatus[i] != NULL) {
      delete(cmbArrayPhysicalStatus[i]);
    }
  }
  delete(cmbArrayPhysicalStatus);
  delete(lastVisualizationEventCP);
  delete(currentVisualizationEventCP);
  delete(lastVisualizationEventCMB);
  delete(currentVisualizationEventCMB);
}


/* Notes
   1. what is the sematic of the run()? ie. when start() gets called, does it begin
   running immediately? 
   2. how about add stuff to ScoreGraphNode? add cycle information and status?
   3. so the way to handle, start and stop is the change the "status" of the node;
   nodes that have a run status will get run by the simulator.
   4. setting dumpPage/loadPage stuff will get the "cycle" field set
   5. does scheduler guarantee that no conflicting commands will be issued?
   simulator should probably do some checking.
   6. simulator has an array of ScoreGraphNode to call on.
   7. connect stream does nothing (will not new any streams)
   8. when dump, create FIFO to dump info into; when load, re-write info back to streams
   9. reset number of stall cycles counter when start()

   10. need to give fault address to scheduler
   11. stats for consumption and production (implmented in streams already)
   12. reset statics on start();

   13. when a command is issued, run them right away
   14. scheduler will issue one token at a time
   15. need to set the stream length 
   16. return cycle time on timeslice event
   17. deal with memory 
   18. remember to delete sync event when done, or memory problem later on
   19. add the stats for array (input rate/output rate) 
   20. need to deal with placement later; should be a simple change in stream class
   21. need to arrayStatus

   
 */


//////////////////////////////////////////////////////////////////////////////
// ScoreSimulator::runUntil:
//   command from scheduler to simulator
//   simulator will run until time = time + cycle
//
// Parameters: cycle.
//
// Return value: None.
//////////////////////////////////////////////////////////////////////////////
void ScoreSimulator::runUntil() {

  if (VERBOSEDEBUG || DEBUG) {
    // Sanity checking
    if (currTimeSlice < *simCycle)
      cout << "   SIMERR: setting slice bad in time" << endl;
    cout << "   SIM: runUntil - " << *simCycle << endl;
  }
  needToken = 1;
  outputTokenType = SCORE_EVENT_TIMESLICE;
}


//////////////////////////////////////////////////////////////////////////////
// ScoreSimulator::getArrayStatus:
//   command from scheduler to simulator
//   simulator will insert a array status token to output stream
//
// Parameters:
//   cpMask: indicates which CPs we care about.
//   cmbMask: indicates which CMBs we care about.
//
// Return value: None.
//////////////////////////////////////////////////////////////////////////////
void ScoreSimulator::getArrayStatus(char *cpMask, char *cmbMask) {
  unsigned i;
  int j;

  saArrayStatus =  new ScoreArrayCPStatus[numPhysicalPages];
  cmbArrayStatus = new ScoreArrayCMBStatus[numPhysicalSegments];

  for (i=0; i<numPhysicalPages; i++) {
    if (cpMask[i]) {
      if (saArrayPhysicalStatus[i] != NULL) {
	saArrayStatus[i].isDone = saArrayPhysicalStatus[i]->isDone;
	saArrayStatus[i].isStalled = saArrayPhysicalStatus[i]->isStalled;
	saArrayStatus[i].stallCount = saArrayPhysicalStatus[i]->stallCount;
	
	if (saArray[i] != NULL) {
	  saArrayStatus[i].inputConsumption = new 
	    unsigned[saArrayPhysicalStatus[i]->numOfInputs] ;
	  for (j=0; j<saArrayPhysicalStatus[i]->numOfInputs; j++) {
	    saArrayStatus[i].inputConsumption[j] = 
	      saArray[i]->getInputConsumption(j);
	  }
	  saArrayStatus[i].outputProduction = new 
	    unsigned[saArrayPhysicalStatus[i]->numOfOutputs];
	  for (j=0; j<saArrayPhysicalStatus[i]->numOfOutputs; j++) {
	    saArrayStatus[i].outputProduction[j] = 
	      saArray[i]->getOutputProduction(j);
	  }
	  saArrayStatus[i].currentState = saArray[i]->get_state();
	  saArrayStatus[i].emptyInputs = 0;
	  saArrayStatus[i].inputFIFONumTokens = new
	    unsigned[saArrayPhysicalStatus[i]->numOfInputs];
	  for (j=0; j<saArrayPhysicalStatus[i]->numOfInputs; j++) {
	    char isEmpty = STREAM_EMPTY(saArray[i]->getInput(j));
	    // FIX ME! IS THIS THE RIGHT WAY TO GET FIFO TOKEN COUNT?
	    unsigned int tokenCount = 
	      STREAM_NUMTOKENS(saArray[i]->getInput(j));
	    
	    if (isEmpty) {
	      saArrayStatus[i].emptyInputs = 
		saArrayStatus[i].emptyInputs | (1 << j);
	    }
	    
	    saArrayStatus[i].inputFIFONumTokens[j] = tokenCount;
	  }
	  saArrayStatus[i].fullOutputs = 0;
	  for (j=0; j<saArrayPhysicalStatus[i]->numOfOutputs; j++) {
	    char isFull = STREAM_FULL(saArray[i]->getOutput(j));
	    
	    if (isFull) {
	      saArrayStatus[i].fullOutputs = 
		saArrayStatus[i].fullOutputs | (1 << j);
	    }
	  }
	}
      } else {
	saArrayStatus[i].clearStatus();
      }
    }
  }
  
  for (i=0; i<numPhysicalSegments; i++) {
    if (cmbMask[i]) {
      if (cmbArrayPhysicalStatus[i] != NULL) {
	cmbArrayStatus[i].isDone = cmbArrayPhysicalStatus[i]->isDone;
	cmbArrayStatus[i].isStalled = cmbArrayPhysicalStatus[i]->isStalled;
	cmbArrayStatus[i].isFaulted = cmbArrayPhysicalStatus[i]->isFaulted;
	cmbArrayStatus[i].stallCount = cmbArrayPhysicalStatus[i]->stallCount;
	cmbArrayStatus[i].faultedAddr = 
	  cmbArrayPhysicalStatus[i]->faultedAddr;
	
	if (cmbArray[i] != NULL) {
	  cmbArrayStatus[i].readAddr = cmbArray[i]->readAddr;
	  cmbArrayStatus[i].writeAddr = cmbArray[i]->writeAddr;
	  cmbArrayStatus[i].readCount = cmbArray[i]->readCount;
	  cmbArrayStatus[i].writeCount = cmbArray[i]->writeCount;
	  cmbArrayStatus[i].emptyInputs = 0;
	  cmbArrayStatus[i].inputFIFONumTokens = new
	    unsigned[cmbArrayPhysicalStatus[i]->numOfInputs];
	  for (j=0; j<cmbArrayPhysicalStatus[i]->numOfInputs; j++) {
	    char isEmpty = STREAM_EMPTY(cmbArray[i]->getInput(j));
	    unsigned int tokenCount = 
	      STREAM_NUMTOKENS(cmbArray[i]->getInput(j));
	    
	    if (isEmpty) {
	      cmbArrayStatus[i].emptyInputs = 
		cmbArrayStatus[i].emptyInputs | (1 << j);
	    }
	    
	    cmbArrayStatus[i].inputFIFONumTokens[j] = tokenCount;
	  }
	  cmbArrayStatus[i].fullOutputs = 0;
	  for (j=0; j<cmbArrayPhysicalStatus[i]->numOfOutputs; j++) {
	    char isFull = STREAM_FULL(cmbArray[i]->getOutput(j));
	    
	    if (isFull) {
	      cmbArrayStatus[i].fullOutputs = 
		cmbArrayStatus[i].fullOutputs | (1 << j);
	    }
	  }
	}
      } else {
	cmbArrayStatus[i].clearStatus();
      }
    }
  }

  // insert token to output stream
  needToken = 1;
  outputTokenType = SCORE_EVENT_RETURNARRAYSTATUS;

}


//////////////////////////////////////////////////////////////////////////////
// ScoreSimulator::stopPage
//   command from scheduler to simulator
//   set the runCycle variable to IDLE; this will stop the page from 
//   executing
//
// Parameters: index into saArrayPhysicalStatus
//
// Return value: None.
//////////////////////////////////////////////////////////////////////////////
void ScoreSimulator::stopPage(int index) {

  if (VERBOSEDEBUG || DEBUG) {

    if ((saArrayPhysicalStatus[index]->getRunCycle()) > 0) {
      cout << "   SIMERR: stopping a busy subarray " << index << " - " 
	   << *simCycle << endl;
    }
    cout << "   SIM: stopPage " << index << " - " << *simCycle << endl;
  }

  saArrayPhysicalStatus[index]->setRunCycle(IDLE);

}


//////////////////////////////////////////////////////////////////////////////
// ScoreSimulator::startPage
//   command from scheduler to simulator
//   set the runCycle variable to RUNNABLE; this will enable the page 
//
// Parameters: index into saArrayPhysicalStatus
//
// Return value: None.
//////////////////////////////////////////////////////////////////////////////
void ScoreSimulator::startPage(int index) {

  if (VERBOSEDEBUG || DEBUG) {
    if ((saArrayPhysicalStatus[index]->getRunCycle()) > 0) {
      cout << "   SIMERR: starting a busy subarray " << index << " - " 
	   << *simCycle << endl;
    }
    cout << "   SIM: startPage " << index << " - " << *simCycle << endl;
  }

  saArrayPhysicalStatus[index]->setRunCycle(RUNNABLE);
  saArrayPhysicalStatus[index]->currentVisualizationEvent =
    VISUALIZATION_EVENT_IDLE;

  saArray[index]->syncSchedToReal();
}


//////////////////////////////////////////////////////////////////////////////
// ScoreSimulator::stopSegment
//   command from scheduler to simulator
//   set the runCycle variable to IDLE; this will stop the segment from 
//   executing
//
// Parameters: index to the CMB array
//
// Return value: None.
//////////////////////////////////////////////////////////////////////////////
void ScoreSimulator::stopSegment(int index) {

  if (VERBOSEDEBUG || DEBUG) {
    if ((cmbArrayPhysicalStatus[index]->getRunCycle()) > 0) {
      cout << "   SIMERR: stopping a busy segment " << index << " - " 
	   << *simCycle << endl;
    }
    cout << "   SIM: stopSegment " << index << " - " << *simCycle << endl;
  }

  cmbArrayPhysicalStatus[index]->setRunCycle(IDLE);
  cmbArrayPhysicalStatus[index]->currentVisualizationEvent =
    VISUALIZATION_EVENT_IDLE;
}


//////////////////////////////////////////////////////////////////////////////
// ScoreSimulator::startSegment
//   command from scheduler to simulator
//   set the runCycle variable to RUNNABLE; this will enable the page 
//
// Parameters: index to the CMB array
//
// Return value: None.
//////////////////////////////////////////////////////////////////////////////
void ScoreSimulator::startSegment(int index) {

  if (VERBOSEDEBUG || DEBUG) {
    if ((cmbArrayPhysicalStatus[index]->getRunCycle()) > 0) {
      cout << "   SIMERR: starting a busy segment " << index << " - " 
	   << *simCycle << endl;
    }
    cout << "   SIM: startSegment " << index << " - " << *simCycle << endl;
  }

  cmbArrayPhysicalStatus[index]->setRunCycle(RUNNABLE);
  cmbArrayPhysicalStatus[index]->currentVisualizationEvent =
    VISUALIZATION_EVENT_IDLE;

  cmbArray[index]->syncSchedToReal();
}


//////////////////////////////////////////////////////////////////////////////
// ScoreSimulator::dumpPageState
//   command from scheduler to simulator
//   
//
// Parameters: index to SA array (source CP).
//             index to CMB array (sink CMB).
//
// Return value: None.
//////////////////////////////////////////////////////////////////////////////
void ScoreSimulator::dumpPageState(int indexCP, int indexCMB) {

  if (VERBOSEDEBUG || DEBUG) {
    if ((saArrayPhysicalStatus[indexCP]->getRunCycle()) != IDLE) {
      cout << "   SIMERR: dumping a non-idle page state " << indexCP << " - " 
	   << *simCycle << endl;
    }
    if ((cmbArrayPhysicalStatus[indexCMB]->getRunCycle()) != IDLE) {
      cout << "   SIMERR: dumping to a non-idle segment " << indexCMB << " - " 
	   << *simCycle << endl;
    }
    cout << "   SIM: dumpPageState " << indexCP << " - " << *simCycle << endl;
  }

  saArrayPhysicalStatus[indexCP]->setRunCycle(SIM_COST_DUMPPAGESTATE);
  cmbArrayPhysicalStatus[indexCMB]->setRunCycle(SIM_COST_DUMPPAGESTATE);
  saArrayPhysicalStatus[indexCP]->currentVisualizationEvent =
    cmbArrayPhysicalStatus[indexCMB]->currentVisualizationEvent =
    VISUALIZATION_EVENT_DUMPPAGESTATE;
}


//////////////////////////////////////////////////////////////////////////////
// ScoreSimulator::dumpPageFIFO
//   command from scheduler to simulator
//   
//
// Parameters: index to SA array (source CP).
//             index to CMB array (sink CMB).
//
// Return value: None.
//////////////////////////////////////////////////////////////////////////////
void ScoreSimulator::dumpPageFIFO(int indexCP, int indexCMB) {

  if (VERBOSEDEBUG || DEBUG) {
    if ((saArrayPhysicalStatus[indexCP]->getRunCycle()) != IDLE) {
      cout << "   SIMERR: dumping a non-idle page FIFO " << indexCP << " - " 
	   << *simCycle << endl;
    }
    if ((cmbArrayPhysicalStatus[indexCMB]->getRunCycle()) != IDLE) {
      cout << "   SIMERR: dumping to a non-idle segment " << indexCMB << " - " 
	   << *simCycle << endl;
    }
    cout << "   SIM: dumpPageFIFO " << indexCP << " - " << *simCycle << endl;
  }
  // please fix me 
  // need to create a FIFO, read all the token from the input stream of the 
  // PAGE and stored the info in some location that we can find it later on

  saArrayPhysicalStatus[indexCP]->setRunCycle(SIM_COST_DUMPPAGEFIFO);
  cmbArrayPhysicalStatus[indexCMB]->setRunCycle(SIM_COST_DUMPPAGEFIFO);
  saArrayPhysicalStatus[indexCP]->currentVisualizationEvent =
    cmbArrayPhysicalStatus[indexCMB]->currentVisualizationEvent =
    VISUALIZATION_EVENT_DUMPPAGEFIFO;
}


//////////////////////////////////////////////////////////////////////////////
// ScoreSimulator::loadPageConfig
//   command from scheduler to simulator
//   
//
// Parameters: index to SA array (sink CP).
//             index to CMB array (source CMB).
//
// Return value: None.
//////////////////////////////////////////////////////////////////////////////
void ScoreSimulator::loadPageConfig(int indexCP, int indexCMB, 
				    ScorePage *page) {

  saArray[indexCP] = page;
  if (saArrayPhysicalStatus[indexCP] != NULL)
    delete(saArrayPhysicalStatus[indexCP]);
  saArrayPhysicalStatus[indexCP] = 
    new ScoreArrayPhysicalStatus(page->getInputs(),
				 page->getOutputs());
  page->setStall(0);

  if (VERBOSEDEBUG || DEBUG) {
    if ((saArrayPhysicalStatus[indexCP]->getRunCycle()) != IDLE) {
      cout << "   SIMERR: load config to non-idle page " << indexCP << " - " 
	   << *simCycle << endl;
    }
    if ((cmbArrayPhysicalStatus[indexCMB]->getRunCycle()) != IDLE) {
      cout << "   SIMERR: load config from non-idle segment " << indexCMB << " - " 
	   << *simCycle << " runCycle=" << 
	cmbArrayPhysicalStatus[indexCMB]->getRunCycle() << endl;
    }
    cout << "   SIM: loadPageConfig " << indexCP << " - " << *simCycle << endl;
  }

  saArrayPhysicalStatus[indexCP]->setRunCycle(SIM_COST_LOADPAGECONFIG);
  cmbArrayPhysicalStatus[indexCMB]->setRunCycle(SIM_COST_LOADPAGECONFIG);
  saArrayPhysicalStatus[indexCP]->currentVisualizationEvent =
    cmbArrayPhysicalStatus[indexCMB]->currentVisualizationEvent =
    VISUALIZATION_EVENT_LOADPAGECONFIG;
}


//////////////////////////////////////////////////////////////////////////////
// ScoreSimulator::loadPageState
//   command from scheduler to simulator
//   
//
// Parameters: index to SA array (sink CP).
//             index to CMB array (source CMB).
//
// Return value: None.
//////////////////////////////////////////////////////////////////////////////
void ScoreSimulator::loadPageState(int indexCP, int indexCMB) {

  if (VERBOSEDEBUG || DEBUG) {
    if ((saArrayPhysicalStatus[indexCP]->getRunCycle()) != IDLE) {
      cout << "   SIMERR: load state to non-idle page " << indexCP << " - " 
	   << *simCycle << endl;
    }
    if ((cmbArrayPhysicalStatus[indexCMB]->getRunCycle()) != IDLE) {
      cout << "   SIMERR: load state from non-idle segment " << indexCMB << " - " 
	   << *simCycle << endl;
    }
    cout << "   SIM: loadPageState " << indexCP << " - " << *simCycle << endl;
  }

  saArrayPhysicalStatus[indexCP]->setRunCycle(SIM_COST_LOADPAGESTATE);
  cmbArrayPhysicalStatus[indexCMB]->setRunCycle(SIM_COST_LOADPAGESTATE);
  saArrayPhysicalStatus[indexCP]->currentVisualizationEvent =
    cmbArrayPhysicalStatus[indexCMB]->currentVisualizationEvent =
    VISUALIZATION_EVENT_LOADPAGESTATE;
}


//////////////////////////////////////////////////////////////////////////////
// ScoreSimulator::loadPageFIFO
//   command from scheduler to simulator
//   
//
// Parameters: index to SA array (sink CP).
//             index to CMB array (source CMB).
//
// Return value: None.
//////////////////////////////////////////////////////////////////////////////
void ScoreSimulator::loadPageFIFO(int indexCP, int indexCMB) {

  if (VERBOSEDEBUG || DEBUG) {
    if ((saArrayPhysicalStatus[indexCP]->getRunCycle()) != IDLE) {
      cout << "   SIMERR: load FIFO to non-idle page " << indexCP << " - " 
	   << *simCycle << endl;
    }
    if ((cmbArrayPhysicalStatus[indexCMB]->getRunCycle()) != IDLE) {
      cout << "   SIMERR: load FIFO from non-idle segment " << indexCMB << " - " 
	   << *simCycle << endl;
    }
    cout << "   SIM: loadPageFIFO " << indexCP << " - " << *simCycle << endl;
  }

  // please fix me 
  // need to find the FIFO info from some place and stream write to 
  // each of the input stream for the page

  saArrayPhysicalStatus[indexCP]->setRunCycle(SIM_COST_LOADPAGEFIFO);
  cmbArrayPhysicalStatus[indexCMB]->setRunCycle(SIM_COST_LOADPAGEFIFO);
  saArrayPhysicalStatus[indexCP]->currentVisualizationEvent =
    cmbArrayPhysicalStatus[indexCMB]->currentVisualizationEvent =
    VISUALIZATION_EVENT_LOADPAGEFIFO;
}


//////////////////////////////////////////////////////////////////////////////
// ScoreSimulator::getSegmentPointers
//   command from scheduler to simulator
//   
//
// Parameters: index to CMB array.
//
// Return value: None.
//////////////////////////////////////////////////////////////////////////////
void ScoreSimulator::getSegmentPointers(int index) {

  // please fix me 
  if (VERBOSEDEBUG || DEBUG) {
    if ((cmbArrayPhysicalStatus[index]->getRunCycle()) != IDLE) {
      cout << "   SIMERR: get pointers to non-idle segment " << index << " - " 
	   << *simCycle << endl;
    }
    cout << "   SIM: getSegmentPointers " << index << " - " << *simCycle << endl;
  }

  cmbArrayPhysicalStatus[index]->setRunCycle(SIM_COST_GETSEGMENTPOINTERS);
  cmbArrayPhysicalStatus[index]->currentVisualizationEvent =
    VISUALIZATION_EVENT_GETSEGMENTPOINTERS;

  // copy over the pointers.
  // NOT TRANSFERRED!
  //cmbArray[index]->sched_mode = cmbArray[index]->mode;
  //cmbArray[index]->sched_maxAddr = cmbArray[index]->maxAddr;
  //cmbArray[index]->sched_traAddr = cmbArray[index]->traAddr;
  //cmbArray[index]->sched_pboAddr = cmbArray[index]->pboAddr;
  cmbArray[index]->sched_readAddr = cmbArray[index]->readAddr;
  cmbArray[index]->sched_writeAddr = cmbArray[index]->writeAddr;
  cmbArray[index]->sched_readCount = cmbArray[index]->readCount;
  cmbArray[index]->sched_writeCount = cmbArray[index]->writeCount;
  cmbArray[index]->sched_this_segment_is_done =
    cmbArray[index]->this_segment_is_done;
}


//////////////////////////////////////////////////////////////////////////////
// ScoreSimulator::dumpSegmentFIFO
//   command from scheduler to simulator
//   
//
// Parameters: index to CMB array (both source and sink CMB).
//
// Return value: None.
//////////////////////////////////////////////////////////////////////////////
void ScoreSimulator::dumpSegmentFIFO(int indexSrc, int indexSink) {

  // please fix me 
  if (VERBOSEDEBUG || DEBUG) {
    if ((cmbArrayPhysicalStatus[indexSrc]->getRunCycle()) != IDLE) {
      cout << "   SIMERR: dump FIFO from non-idle segment " << indexSrc << " - " 
	   << *simCycle << endl;
    }
    if ((cmbArrayPhysicalStatus[indexSink]->getRunCycle()) != IDLE) {
      cout << "   SIMERR: dump FIFO to non-idle segment " << indexSink << " - " 
	   << *simCycle << endl;
    }
    cout << "   SIM: dumpSegmentFIFO " << indexSrc << 
      " - " << *simCycle << endl;
  }
  cmbArrayPhysicalStatus[indexSrc]->setRunCycle(SIM_COST_DUMPSEGMENTFIFO);
  cmbArrayPhysicalStatus[indexSink]->setRunCycle(SIM_COST_DUMPSEGMENTFIFO);
  cmbArrayPhysicalStatus[indexSrc]->currentVisualizationEvent =
    cmbArrayPhysicalStatus[indexSink]->currentVisualizationEvent =
    VISUALIZATION_EVENT_DUMPSEGMENTFIFO;
}


//////////////////////////////////////////////////////////////////////////////
// ScoreSimulator::setSegmentConfigPointers
//   command from scheduler to simulator
//   
//
// Parameters: index to CMB array.
//
// Return value: None.
//////////////////////////////////////////////////////////////////////////////
void ScoreSimulator::setSegmentConfigPointers(int index, ScoreSegment *segment) {

  // please fix me 
  cmbArray[index] = segment;

  if (VERBOSEDEBUG || DEBUG) {
    if ((cmbArrayPhysicalStatus[index]->getRunCycle()) != IDLE) {
      cout << "   SIMERR: set pointers to non-idle segment " << index << " - " 
	   << *simCycle << endl;
    }
    cout << "   SIM: setSegmentConfigPointers " << index << " - " << *simCycle << endl;
  }

  if ((cmbArrayPhysicalStatus[index]->getRunCycle()) == IDLE) {
    delete cmbArrayPhysicalStatus[index];
    cmbArrayPhysicalStatus[index] = 
      new ScoreArrayPhysicalStatus(segment->getInputs(), 
				   segment->getOutputs());

    segment->setStall(0);
  }

  cmbArrayPhysicalStatus[index]->setRunCycle(SIM_COST_SETSEGMENTCONFIGPOINTERS);
  cmbArrayPhysicalStatus[index]->currentVisualizationEvent =
    VISUALIZATION_EVENT_SETSEGMENTCONFIGPOINTERS;

  // transfer the new pointers over.
  cmbArray[index]->mode = cmbArray[index]->sched_mode;
  cmbArray[index]->maxAddr = cmbArray[index]->sched_maxAddr;
  cmbArray[index]->traAddr = cmbArray[index]->sched_traAddr;
  cmbArray[index]->pboAddr = cmbArray[index]->sched_pboAddr;
  cmbArray[index]->readAddr = cmbArray[index]->sched_readAddr;
  cmbArray[index]->writeAddr = cmbArray[index]->sched_writeAddr;
  cmbArray[index]->readCount = cmbArray[index]->sched_readCount;
  cmbArray[index]->writeCount = cmbArray[index]->sched_writeCount;
  cmbArray[index]->this_segment_is_done = 
    cmbArray[index]->sched_this_segment_is_done;
}


//////////////////////////////////////////////////////////////////////////////
// ScoreSimulator::changeSegmentMode
//   command from scheduler to simulator
//   NOTE: Changes segment mode as well as resetting done flag.
//   
//
// Parameters: index to CMB array.
//
// Return value: None.
//////////////////////////////////////////////////////////////////////////////
void ScoreSimulator::changeSegmentMode(int index, ScoreSegment *segment) {

  // please fix me 
  if (VERBOSEDEBUG || DEBUG) {
    if ((cmbArrayPhysicalStatus[index]->getRunCycle()) != IDLE) {
      cout << "   SIMERR: changing mode of non-idle segment " << index << " - " 
	   << *simCycle << endl;
    }
    cout << "   SIM: changeSegmentMode " << index << " - " << *simCycle << endl;
  }

  if ((cmbArrayPhysicalStatus[index]->getRunCycle()) == IDLE) {
    delete cmbArrayPhysicalStatus[index];
    cmbArrayPhysicalStatus[index] = 
      new ScoreArrayPhysicalStatus(segment->getInputs(), 
				   segment->getOutputs());
  }

  cmbArrayPhysicalStatus[index]->setRunCycle(SIM_COST_CHANGESEGMENTMODE);
  cmbArrayPhysicalStatus[index]->currentVisualizationEvent =
    VISUALIZATION_EVENT_CHANGESEGMENTMODE;

  // transfer the new mode over.
  cmbArray[index]->mode = cmbArray[index]->sched_mode;
}


//////////////////////////////////////////////////////////////////////////////
// ScoreSimulator::changeSegmentTRAandPBOandMAX
//   command from scheduler to simulator
//   
//
// Parameters: index to CMB array.
//
// Return value: None.
//////////////////////////////////////////////////////////////////////////////
void ScoreSimulator::changeSegmentTRAandPBOandMAX(int index, ScoreSegment *segment) {

  // please fix me 
  if (VERBOSEDEBUG || DEBUG) {
    if ((cmbArrayPhysicalStatus[index]->getRunCycle()) != IDLE) {
      cout << "   SIMERR: changing TRA and MAX of non-idle segment " << 
        index << " - " << *simCycle << endl;
    }
    cout << "   SIM: changeSegmentTRAandMAX " << 
      index << " - " << *simCycle << endl;
  }

  if ((cmbArrayPhysicalStatus[index]->getRunCycle()) == IDLE) {
    delete cmbArrayPhysicalStatus[index];
    cmbArrayPhysicalStatus[index] = 
      new ScoreArrayPhysicalStatus(segment->getInputs(), 
				   segment->getOutputs());
  }

  cmbArrayPhysicalStatus[index]->setRunCycle(SIM_COST_CHANGESEGMENTTRAANDPBOANDMAX);
  cmbArrayPhysicalStatus[index]->currentVisualizationEvent =
    VISUALIZATION_EVENT_CHANGESEGMENTTRAANDPBOANDMAX;

  // transfer the new tra over.
  cmbArray[index]->traAddr = cmbArray[index]->sched_traAddr;
  cmbArray[index]->pboAddr = cmbArray[index]->sched_pboAddr;
  cmbArray[index]->maxAddr = cmbArray[index]->sched_maxAddr;
}


//////////////////////////////////////////////////////////////////////////////
// ScoreSimulator::resetSegmentDoneFlag
//   command from scheduler to simulator
//   
//
// Parameters: index to CMB array.
//
// Return value: None.
//////////////////////////////////////////////////////////////////////////////
void ScoreSimulator::resetSegmentDoneFlag(int index, ScoreSegment *segment) {

  // please fix me 
  if (VERBOSEDEBUG || DEBUG) {
    if ((cmbArrayPhysicalStatus[index]->getRunCycle()) != IDLE) {
      cout << "   SIMERR: resetting done flag of non-idle segment " << index << " - " 
	   << *simCycle << endl;
    }
    cout << "   SIM: resetSegmentDoneFlag " << index << " - " << *simCycle << endl;
  }

  if ((cmbArrayPhysicalStatus[index]->getRunCycle()) == IDLE) {
    delete cmbArrayPhysicalStatus[index];
    cmbArrayPhysicalStatus[index] = 
      new ScoreArrayPhysicalStatus(segment->getInputs(), 
				   segment->getOutputs());
  }

  cmbArrayPhysicalStatus[index]->setRunCycle(SIM_COST_RESETSEGMENTDONEFLAG);
  cmbArrayPhysicalStatus[index]->currentVisualizationEvent =
    VISUALIZATION_EVENT_RESETSEGMENTDONEFLAG;

  // reset done flag.
  cmbArray[index]->this_segment_is_done = 0;
}


//////////////////////////////////////////////////////////////////////////////
// ScoreSimulator::loadSegmentFIFO
//   command from scheduler to simulator
//   
//
// Parameters: index to CMB array (both source and sink CMB).
//
// Return value: None.
//////////////////////////////////////////////////////////////////////////////
void ScoreSimulator::loadSegmentFIFO(int indexSink, int indexSrc) {

  // please fix me 
  if (VERBOSEDEBUG || DEBUG) {
    if ((cmbArrayPhysicalStatus[indexSrc]->getRunCycle()) != IDLE) {
      cout << "   SIMERR: load FIFO from non-idle segment " << indexSrc << " - " 
	   << *simCycle << endl;
    }
    if ((cmbArrayPhysicalStatus[indexSink]->getRunCycle()) != IDLE) {
      cout << "   SIMERR: load FIFO to non-idle segment " << indexSink << " - " 
	   << *simCycle << endl;
    }
    cout << "   SIM: loadSegmentFIFO " << indexSink << " - " << *simCycle << endl;
  }

  cmbArrayPhysicalStatus[indexSrc]->setRunCycle(SIM_COST_LOADSEGMENTFIFO);
  cmbArrayPhysicalStatus[indexSink]->setRunCycle(SIM_COST_LOADSEGMENTFIFO);
  cmbArrayPhysicalStatus[indexSrc]->currentVisualizationEvent =
    cmbArrayPhysicalStatus[indexSink]->currentVisualizationEvent =
    VISUALIZATION_EVENT_LOADSEGMENTFIFO;
}


//////////////////////////////////////////////////////////////////////////////
// ScoreSimulator::memXferCMBToCMB
//   command from scheduler to simulator
//   
//
// Parameters: 
//
// Return value: None.
//////////////////////////////////////////////////////////////////////////////
void ScoreSimulator::memXferCMBToCMB(int srcIndex, int snkIndex, unsigned int xferSize) {

  if (VERBOSEDEBUG || DEBUG) {
    if ((cmbArrayPhysicalStatus[srcIndex]->getRunCycle()) != IDLE) {
      cout << "   SIMERR: CMB transfer from non-idle segment " << srcIndex << " - " 
	   << *simCycle << endl;
    }
    if ((cmbArrayPhysicalStatus[snkIndex]->getRunCycle()) != IDLE) {
      cout << "   SIMERR: CMB transfer to non-idle segment " << snkIndex << " - " 
	   << *simCycle << endl;
    }
    cout << "   SIM: memXferCMBToCMB " << srcIndex << ""
	 << snkIndex << " - " << *simCycle << endl;
  }

  cmbArrayPhysicalStatus[srcIndex]->setRunCycle((int)ceil(xferSize*SIM_COST_MEMXFERCMBTOCMB));
  cmbArrayPhysicalStatus[snkIndex]->setRunCycle((int)ceil(xferSize*SIM_COST_MEMXFERCMBTOCMB));
  cmbArrayPhysicalStatus[srcIndex]->currentVisualizationEvent =
    cmbArrayPhysicalStatus[snkIndex]->currentVisualizationEvent =
    VISUALIZATION_EVENT_MEMXFERCMBTOCMB;
}


//////////////////////////////////////////////////////////////////////////////
// ScoreSimulator::memXferCMBToPrimary
//   command from scheduler to simulator
//   
//
// Parameters: 
//
// Return value: None.
//////////////////////////////////////////////////////////////////////////////
void ScoreSimulator::memXferCMBToPrimary(int index, unsigned int xferSize) {

  // does this mean that the data is no longer needed?
  // that we can call "allow access" on this CMB segment?


  if (VERBOSEDEBUG || DEBUG) {
    if ((cmbArrayPhysicalStatus[index]->getRunCycle()) != IDLE) {
      cout << "   SIMERR: CMB transfer from non-idle segment " << index << " - " 
	   << *simCycle << endl;
    }
    cout << "   SIM: memXferCMBToPrimary " << index << " - " << *simCycle << endl;
  }
  cmbArrayPhysicalStatus[index]->setRunCycle((int)ceil(xferSize*SIM_COST_MEMXFERCMBTOPRIMARY));
  cmbArrayPhysicalStatus[index]->currentVisualizationEvent =
    VISUALIZATION_EVENT_MEMXFERCMBTOPRIMARY;
}


//////////////////////////////////////////////////////////////////////////////
// ScoreSimulator::memXferPrimaryToCMB
//   command from scheduler to simulator
//   
//
// Parameters: 
//
// Return value: None.
//////////////////////////////////////////////////////////////////////////////
void ScoreSimulator::memXferPrimaryToCMB(int index, unsigned int xferSize) {

  if (VERBOSEDEBUG || DEBUG) {
    if ((cmbArrayPhysicalStatus[index]->getRunCycle()) != IDLE) {
      cout << "   SIMERR: CMB transfer to non-idle segment " << index << " - " 
	   << *simCycle << " runCycle=" << 
	cmbArrayPhysicalStatus[index]->getRunCycle() << endl;
    }
    cout << "   SIM: memXferPrimaryToCMB " << index << " - " << *simCycle << endl;
  }

  cmbArrayPhysicalStatus[index]->setRunCycle((int)ceil(xferSize*SIM_COST_MEMXFERPRIMARYTOCMB));
  cmbArrayPhysicalStatus[index]->currentVisualizationEvent =
    VISUALIZATION_EVENT_MEMXFERPRIMARYTOCMB;
}


//////////////////////////////////////////////////////////////////////////////
// ScoreSimulator::connectStream
//   command from scheduler to simulator
//   
//
// Parameters: src and snk nodes.
//
// Return value: None.
//////////////////////////////////////////////////////////////////////////////
void ScoreSimulator::connectStream(ScoreGraphNode *srcNode, 
				   ScoreGraphNode *snkNode) {
  char srcNodeIsCP = srcNode->isPage();
  char sinkNodeIsCP = snkNode->isPage();
  unsigned int srcLoc;
  unsigned int sinkLoc;

  if (srcNodeIsCP) {
    srcLoc = ((ScorePage *) srcNode)->sched_residentLoc;
  } else {
    srcLoc = ((ScoreSegment *) srcNode)->sched_residentLoc;
  }
  if (sinkNodeIsCP) {
    sinkLoc = ((ScorePage *) snkNode)->sched_residentLoc;
  } else {
    sinkLoc = ((ScoreSegment *) snkNode)->sched_residentLoc;
  }
  if (VERBOSEDEBUG || DEBUG) {
    // FOR NOW, DON'T DO BUSY CHECKING FOR CONNECTSTREAM... NEEDS TO BE
    // FIXED SO THAT CONCURRENT CONNECTSTREAMS ARE ALLOWED FOR A PARTICULAR
    // PAGE, BUT STARTING A PAGE/SEGMENT THAT IS BUSY WITH A CONNECTSTREAM
    // IS A NO-NO...
    if (srcNodeIsCP) {
#if 0
      if ((saArrayPhysicalStatus[srcLoc]->getRunCycle()) != IDLE) {
	cout << "   SIMERR: Connecting stream from non-idle page " << srcLoc << " - " 
	     << *simCycle << endl;
      }
#endif
    } else {
#if 0
      if ((cmbArrayPhysicalStatus[srcLoc]->getRunCycle()) != IDLE) {
	cout << "   SIMERR: Connecting stream from non-idle segment " << srcLoc << " - " 
	     << *simCycle << endl;
      }
#endif
    }
    if (sinkNodeIsCP) {
#if 0
      if ((saArrayPhysicalStatus[sinkLoc]->getRunCycle()) != IDLE) {
	cout << "   SIMERR: Connecting stream to non-idle page " << sinkLoc << " - " 
	     << *simCycle << endl;
      }
#endif
    } else {
#if 0
      if ((cmbArrayPhysicalStatus[sinkLoc]->getRunCycle()) != IDLE) {
	cout << "   SIMERR: Connecting stream to non-idle segment " << sinkLoc << " - " 
	     << *simCycle << endl;
      }
#endif
    }
    cout << "   SIM: connectStream " << (srcNodeIsCP?"CP":"CMB") << srcLoc 
	 << " " << (sinkNodeIsCP?"CP":"CMB") << sinkLoc
	 << " - " << *simCycle << endl;
  }
  // please fix me 
  // need to adjust the length of the stream to accomadate relocation of 
  // the physical array

  if (srcNodeIsCP) {
    saArrayPhysicalStatus[srcLoc]->setRunCycle(SIM_COST_CONNECTSTREAM);
    saArrayPhysicalStatus[srcLoc]->currentVisualizationEvent =
      VISUALIZATION_EVENT_CONNECTSTREAM;
  } else {
    cmbArrayPhysicalStatus[srcLoc]->setRunCycle(SIM_COST_CONNECTSTREAM);
    cmbArrayPhysicalStatus[srcLoc]->currentVisualizationEvent =
      VISUALIZATION_EVENT_CONNECTSTREAM;
  }
  if (sinkNodeIsCP) {
    saArrayPhysicalStatus[sinkLoc]->setRunCycle(SIM_COST_CONNECTSTREAM);
    saArrayPhysicalStatus[sinkLoc]->currentVisualizationEvent =
      VISUALIZATION_EVENT_CONNECTSTREAM;
  } else {
    cmbArrayPhysicalStatus[sinkLoc]->setRunCycle(SIM_COST_CONNECTSTREAM);
    cmbArrayPhysicalStatus[sinkLoc]->currentVisualizationEvent =
      VISUALIZATION_EVENT_CONNECTSTREAM;
  }
}


//////////////////////////////////////////////////////////////////////////////
// ScoreSimulator::run:
//   command from scheduler to simulator
//   simulator will insert a array status token to output stream
//
// Parameters: None.
//
// Return value: None.
//////////////////////////////////////////////////////////////////////////////
void ScoreSimulator::run() {

  unsigned index; 
  ScorePage *page;
  ScoreSegment *segment;
  int currRunCycle;
  int status;

  //unsigned long long startTime, endTime;
#if ENABLE_STALL_DETECT
  unsigned stallCountCP, totalCountCP, stallCountCMB, totalCountCMB;
#endif

  while (1) {

    // Grab token from input stream
    // this token will not take affect until the next cycle
    // to simulate ScoreBoard global controller
    // simulator will block on stream_read
    // do not need to check for stream_eos since scheduler is supposed to
    // run forever
    
    ScoreSyncEvent *token = 0;
    STREAM_READ_RT(inputStream,token);

    if (VERBOSEDEBUG || DEBUG) {
      cerr <<   "   SIM: "; 
      token->print(stderr);
      //cout << "   SIM: run() receivd token - " << *simCycle << endl;
    }

    // need to update the currTimeSlice variable
    currTimeSlice = token->currentTime;

#if 0
    if (token->command == SCORE_EVENT_RUNUNTIL) {
      fprintf(stderr, "SCORE_EVENT_RUNUNTIL: %d\n", token->currentTime);
    }
#endif

    // run the simulator until *simCycle == currTimeSlice

    for (; *simCycle < currTimeSlice ; (*simCycle)++) {

#if 0
      if (((*simCycle) % 10000) == 0) {
	fprintf(stderr, "simCycle = %d/%d\n",
		*simCycle, currTimeSlice);
      }
#endif

#if ENABLE_STALL_DETECT
      // check every 4096 cycles
      if (((*simCycle & 0x000000FF) == 0) &&
	  (token->command == SCORE_EVENT_RUNUNTIL)) {
        totalCountCP = stallCountCP = totalCountCMB = stallCountCMB = 0;
	int break_exe = 0;
#if STALL_DETECT_VERBOSE
	fprintf(stderr, "{");
#endif
        for (index = 0; index < numPhysicalPages; index++) {
          if ((saArrayPhysicalStatus[index] != NULL) &&
	      (saArrayPhysicalStatus[index]->getRunCycle() == RUNNABLE)) {
	    totalCountCP ++;

#if STALL_DETECT_VERBOSE
	    fprintf(stderr, "%d ", saArrayPhysicalStatus[index]->tag);	    
#endif

	    if (saArrayPhysicalStatus[index]->isDone) {
	      stallCountCP++;
#if STALL_DETECT_VERBOSE
	      fprintf(stderr, "~DONE~(%d,%d), ",
		      saArrayPhysicalStatus[index]->consecutiveStallCount,
		      saArrayPhysicalStatus[index]->getRunCycle());
#endif
	    } else {
	      if (saArrayPhysicalStatus[index]->consecutiveStallCount <
		  SIM_RUNNABLETOSTALLED_THRESHOLD) {
#if STALL_DETECT_VERBOSE
		fprintf(stderr, "%d and %d",
			saArrayPhysicalStatus[index]->consecutiveStallCount,
			saArrayPhysicalStatus[index]->getRunCycle());
#endif
		break_exe = 1;
		break;
	      } else {
		stallCountCP++;
#if STALL_DETECT_VERBOSE
		fprintf(stderr, "S(%d,%d), ",
			saArrayPhysicalStatus[index]->consecutiveStallCount,
			saArrayPhysicalStatus[index]->getRunCycle());
#endif
	      }
	    }
	  }
	}

	if (!break_exe) {
	  for (index = 0; index < numPhysicalSegments; index++) {
	    if ((cmbArrayPhysicalStatus[index] != NULL) &&
		(cmbArrayPhysicalStatus[index]->getRunCycle() == RUNNABLE)) {
	      totalCountCMB ++;
	      
#if STALL_DETECT_VERBOSE
	      fprintf(stderr, "%d ", cmbArrayPhysicalStatus[index]->tag);	    
#endif
	      
	      if (cmbArrayPhysicalStatus[index]->isDone) {
		stallCountCMB++;
#if STALL_DETECT_VERBOSE
		fprintf(stderr, "~DONE~(%d,%d), ",
			cmbArrayPhysicalStatus[index]->consecutiveStallCount,
			cmbArrayPhysicalStatus[index]->getRunCycle());
#endif
	      } else {
		if (cmbArrayPhysicalStatus[index]->consecutiveStallCount <
		    SIM_RUNNABLETOSTALLED_THRESHOLD) {
#if STALL_DETECT_VERBOSE
		  fprintf(stderr, "%d and %d",
			  cmbArrayPhysicalStatus[index]->consecutiveStallCount,
			  cmbArrayPhysicalStatus[index]->getRunCycle());
#endif
		  break;
		} else {
		  stallCountCMB++;
#if STALL_DETECT_VERBOSE
		  fprintf(stderr, "S(%d,%d), ",
			  cmbArrayPhysicalStatus[index]->consecutiveStallCount,
			  cmbArrayPhysicalStatus[index]->getRunCycle());
#endif
		}
	      }
	    }
	  }
	}
	

#if STALL_DETECT_VERBOSE
	fprintf(stderr, "} ");
	fprintf(stderr, "stallCount is %d/%dcp and %d/%dcmb [time = %d <%d>]\n",
		stallCountCP, totalCountCP,
		stallCountCMB, totalCountCMB,
		*simCycle, currTimeSlice);
#endif
	if ((stallCountCP == totalCountCP) && (stallCountCMB == totalCountCMB)) {
#if STALL_DETECT_VERBOSE
	  fprintf(stderr,
		  "Array-wide stall condition detected [time = %d <%d>]\n",
		  *simCycle, currTimeSlice);
#endif
	  for (index = 0; index < numPhysicalPages; index++) {
	    if (saArrayPhysicalStatus[index] != NULL) {
              saArrayPhysicalStatus[index]->consecutiveStallCount = 0;
	    }
	  }

	  for (index = 0; index < numPhysicalSegments; index++) {
	    if (cmbArrayPhysicalStatus[index] != NULL) {
              cmbArrayPhysicalStatus[index]->consecutiveStallCount = 0;
	    }
	  }

	  break;
	}
      }
#endif /* ENABLE_STALL_DETECT */

#if 1
      // run the pagestep() in each of the subArray
      for (index = 0; index < numPhysicalPages; index++) {
#if TONY_GRAPH 
	lastVisualizationEventCP[index] = currentVisualizationEventCP[index];
	currentVisualizationEventCP[index] = VISUALIZATION_EVENT_IDLE;
#endif

	if (saArray[index] != NULL) {
	  page = saArray[index];

#if GET_FEEDBACK
	  if (gFeedbackMode == SAMPLERATES) {
	    if (((*simCycle) % gFeedbackSampleFreq) == 0) {
	      page->feedbackNode->recordConsumption(page->getConsumptionVector(),
						    page->getInputs());
	      page->feedbackNode->recordProduction(page->getProductionVector(),
						   page->getOutputs());
	      page->feedbackNode->recordFireCount(page->getFire());
	    }
	  }
#endif

	  currRunCycle = saArrayPhysicalStatus[index]->getRunCycle();
	  if (currRunCycle == RUNNABLE) {
	    if (saArrayPhysicalStatus[index]->isDone != DONE) {
	      //	      rdtscll(startTime);
#if TRACK_LAST_VISITED_NODE
	      assert(!lastVisitedSimNode);
	      lastVisitedSimNode = page;
#endif

	      status = page->pagestep();

#if TRACK_LAST_VISITED_NODE
	      lastVisitedSimNode = 0;
#endif
	      //	      rdtscll(endTime);
	      //	      printf("%llu\n",endTime-startTime);
	      /*
	      if ((endTime-startTime) > 50000) {
		printf("%d %s %d %llu\n",*simCycle, page->getSource(), 
		       page->get_state(), endTime-startTime);
	      }
	      */
	      saArrayPhysicalStatus[index]->update(page,status);
	    }
#if TONY_GRAPH
	    if (saArrayPhysicalStatus[index]->isStalled) {
	      saArrayPhysicalStatus[index]->currentVisualizationEvent =
		VISUALIZATION_EVENT_IDLE;
	    } else {
	      saArrayPhysicalStatus[index]->currentVisualizationEvent =
		VISUALIZATION_EVENT_FIRING;
	    }
#endif
	  } else if (currRunCycle != IDLE) {
	    if (VERBOSEDEBUG || DEBUG) {
	      /*
	      cout << "   SIM: page " << index << "is busy until " 
		   << currRunCycle << " - " << *simCycle << endl;
	      */
	    }
	    saArrayPhysicalStatus[index]->setRunCycle(currRunCycle - 1);
	  } else {
#if TONY_GRAPH
	    saArrayPhysicalStatus[index]->currentVisualizationEvent =
	      VISUALIZATION_EVENT_IDLE;
#endif
	  }
#if TONY_GRAPH
	  currentVisualizationEventCP[index] =
	    saArrayPhysicalStatus[index]->currentVisualizationEvent;
#endif
	}
#if TONY_GRAPH
	if (currentVisualizationEventCP[index] !=
	    lastVisualizationEventCP[index]) {
	  if (visualization != NULL) {
	    visualization->addEventCP(index, 
				      currentVisualizationEventCP[index],
				      *simCycle);
	  }
	}
#endif
      }

      // run the step() in each of the CMB

      for (index = 0; index < numPhysicalSegments; index++) {
#if TONY_GRAPH
	lastVisualizationEventCMB[index] = currentVisualizationEventCMB[index];
	currentVisualizationEventCMB[index] = VISUALIZATION_EVENT_IDLE;
#endif
	if (cmbArray[index] != NULL) {
	  segment = cmbArray[index];

#if GET_FEEDBACK
	  if ((gFeedbackMode == SAMPLERATES) && segment->feedbackNode) {
	    if (((*simCycle) % gFeedbackSampleFreq) == 0) {
	      segment->feedbackNode->
		recordConsumption(segment->getConsumptionVector(),
				  segment->getInputs());
	      segment->feedbackNode->
		recordProduction(segment->getProductionVector(),
				 segment->getOutputs());
	      segment->feedbackNode->
		recordFireCount(segment->getFire());
	    }
	  }
#endif

	  currRunCycle = cmbArrayPhysicalStatus[index]->getRunCycle();
	  if (currRunCycle == RUNNABLE) {
/*
	    if (VERBOSEDEBUG || DEBUG) {
	      cout << "   SIM: segment " << index << "is runnable - " 
		   << *simCycle << endl;
	    }
*/
	    if (cmbArrayPhysicalStatus[index]->isDone != DONE) {
#if TRACK_LAST_VISITED_NODE
	      assert(!lastVisitedSimNode);
	      lastVisitedSimNode = segment;
#endif
	      status = segment->step();
#if TRACK_LAST_VISITED_NODE
	      lastVisitedSimNode = 0;
#endif
	      cmbArrayPhysicalStatus[index]->update(segment,status);
	    }
#if TONY_GRAPH
	    if (cmbArrayPhysicalStatus[index]->isStalled) {
	      cmbArrayPhysicalStatus[index]->currentVisualizationEvent =
		VISUALIZATION_EVENT_IDLE;
	    } else {
	      cmbArrayPhysicalStatus[index]->currentVisualizationEvent =
		VISUALIZATION_EVENT_FIRING;
	    }
#endif
	  } else if (currRunCycle != IDLE) {
	    cmbArrayPhysicalStatus[index]->setRunCycle(currRunCycle - 1);
	  } else {
#if TONY_GRAPH
	    cmbArrayPhysicalStatus[index]->currentVisualizationEvent =
	      VISUALIZATION_EVENT_IDLE;
#endif
	  }
	} else {
	  if (cmbArrayPhysicalStatus[index] != NULL) {
	    currRunCycle = cmbArrayPhysicalStatus[index]->getRunCycle();
	    if (currRunCycle != IDLE) {
	      cmbArrayPhysicalStatus[index]->setRunCycle(currRunCycle - 1);
	    } else {
#if TONY_GRAPH
	      cmbArrayPhysicalStatus[index]->currentVisualizationEvent =
		VISUALIZATION_EVENT_IDLE;
#endif
	    }
	  }
	}
#if TONY_GRAPH
	currentVisualizationEventCMB[index] =
	  cmbArrayPhysicalStatus[index]->currentVisualizationEvent;

	if (currentVisualizationEventCMB[index] !=
	    lastVisualizationEventCMB[index]) {
	  if (visualization != NULL) {
	    visualization->addEventCMB(index, 
				       currentVisualizationEventCMB[index],
				       *simCycle);
	  }
	}
#endif
      }
    }

#else
    assert(0);
    // FIX ME! CHECK THE GLOBAL CHECK ARRAY!!!

    // FIX ME! RECALCULATE THE UNSTALL TIMES ON THE STALLEDNODEQUEUE!

    // move nodes on the stalledNodeQueue to the runnableNodeList if they
    // have matured.
    pq_item pqItem = stalledNodeQueue->find_min();
    while (pqItem != nil_item) {
      ScoreGraphNode *currentNode = stalledNodeQueue->inf(pqItem);
      unsigned int currentLoc = currentNode->sched_residentLoc;

      if (currentNode->sim_unstallTime <= (*simCycle)) {
	stalledNodeQueue->del_item(pqItem);
	currentNode->sim_stalledNodeQueueItem = NULL;
	
	currentNode->sim_runnableNodeListItem =
	  runnableNodeList.append(currentNode);

	pqItem = stalledNodeQueue->find_min();

	if (currentNode->isPage()) {
	  saArrayPhysicalStatus[currentLoc]->consecutiveStallCount = 0;
	} else if (currentNode->isSegment()) {
	  cmbArrayPhysicalStatus[currentLoc]->consecutiveStallCount = 0;
	}
      } else {
	break;
      }
    }

    // run the nodes on the runnable node list.
    runnableNodeList.append(NULL);
    {
      ScoreGraphNode *currentNode = runnableNodeList.pop();

      while (currentNode != NULL) {
	unsigned int currentLoc = currentNode->sched_residentLoc;
	char moveToStallList = 0;

	currentNode->sim_runnableNodeListItem = NULL;

	if (currentNode->isPage()) {
	  ScorePage *currentPage = (ScorePage *) currentNode;
	  
	  status = currentPage->pagestep();
	  saArrayPhysicalStatus[currentLoc]->update(currentPage,status);
	  
	  if (saArrayPhysicalStatus[currentLoc]->consecutiveStallCount >=
	      SIM_RUNNABLETOSTALLED_THRESHOLD) {
	    moveToStallList = 1;
	  }
	} else if (currentNode->isSegment()) {
	  ScoreSegment *currentSegment = (ScoreSegment *) currentNode;
	  
	  status = currentSegment->step();
	  cmbArrayPhysicalStatus[currentLoc]->update(currentSegment,status);
	  
	  if (cmbArrayPhysicalStatus[currentLoc]->consecutiveStallCount >=
	      SIM_RUNNABLETOSTALLED_THRESHOLD) {
	    moveToStallList = 1;
	  }
	}

	if (moveToStallList) {
	  currentNode->sim_stalledNodeQueueItem =
	    
	} else {
	  currentNode->sim_runnableNodeListItem =
	    runnableNodeList.append(currentNode);
	}
      }
    }

    // FIX ME! WHAT DO WE DO ABOUT VIS STUFF, ETC. THAT USED TO BE IN
    //         THE FOR LOOPS!??
#endif

    issue(token);

    if (VERBOSEDEBUG || DEBUG) {
      cout << "   SIM: run() issued token - " << *simCycle << endl;
    }

    if (needToken) {

      // need to insert timeslice token to output stream
      ScoreSyncEvent *outToken = new ScoreSyncEvent();
      outToken->command = outputTokenType;
      outToken->currentTime = *simCycle;
      if (outputTokenType == SCORE_EVENT_RETURNARRAYSTATUS) {
	outToken->cpStatus = saArrayStatus;
	outToken->cmbStatus = cmbArrayStatus;
      } else {
	outToken->cpStatus = NULL;
	outToken->cmbStatus = NULL;
      }

#if 0
      if (outToken->command == SCORE_EVENT_TIMESLICE) {
	fprintf(stderr, "SCORE_EVENT_TIMESLICE: %d\n", outToken->currentTime);
      }
#endif

      STREAM_WRITE_RT(outputStream, outToken);

      if (VERBOSEDEBUG || DEBUG) {
	cout << "   SIM: current time slice is up on cycle - " 
	     << *simCycle << endl;
      }

      needToken = 0;
    } else {
      ScoreSyncEvent *outToken = new ScoreSyncEvent();
      outToken->command = SCORE_EVENT_ACK;
      outToken->currentTime = *simCycle;
      STREAM_WRITE_RT(outputStream,outToken);
    }

    delete(token);

  }
    
}


//////////////////////////////////////////////////////////////////////////////
// ScoreSimulator::issue
//   intrepret command issued from scheduler to simulator
//   and then called appropriate function
//
// Parameters: ScoreSyncEvent pointer token
//
// Return value: None.
//////////////////////////////////////////////////////////////////////////////
void ScoreSimulator::issue(ScoreSyncEvent *input) {

  /* collect statistics on Scheduler to Simulator commands */
  if (input->command < NumberOf_SCORE_EVENT)
    statsOfSchedToSimCommands[input->command]++;
  else {
    cerr << "   SIMERR: unknown command " << input->command 
	 << " - " << currTimeSlice << endl;
    exit(-1);
  }

  switch (input->command) {

    case SCORE_EVENT_IDLE:

#if RECONFIG_ACCT
      reconfigAcct.dumpToFile(gReconfigAcctFile);
      gReconfigAcctFile = 0;
#endif
#if GET_FEEDBACK
      if (gFeedbackMode == SAMPLERATES) {
	delete gFeedbackObj;
	gFeedbackObj = 0;
      }
#endif
      break;



    case SCORE_EVENT_RUNUNTIL:
      if (VERBOSEDEBUG || DEBUG) {
	cout << "   SIM: receive token SCORE_EVENT_RUNUNTIL - " 
	     << currTimeSlice << endl; 
      }
      runUntil();
      
#if RECONFIG_ACCT
      reconfigAcct.recordNextTS(statsOfSchedToSimCommands);
#endif

      break;
    case SCORE_EVENT_GETARRAYSTATUS:
      if (VERBOSEDEBUG || DEBUG) {
	cout << "   SIM: receive token SCORE_EVENT_GETARRAYSTATUS - " 
	     << currTimeSlice << endl; 
     }
      getArrayStatus(input->cpMask, input->cmbMask);
      break;
    case SCORE_EVENT_STOPPAGE:
      if (VERBOSEDEBUG || DEBUG) {
	cout << "   SIM: receive token SCORE_EVENT_STOPPAGE - " 
	     << currTimeSlice << endl; 
      }
      stopPage(((ScorePage *)(input->node))->sched_residentLoc);
      break;
    case SCORE_EVENT_STARTPAGE:
      if (VERBOSEDEBUG || DEBUG) {
	cout << "   SIM: receive token SCORE_EVENT_STARTPAGE - " 
	     << currTimeSlice << endl; 
      }
      startPage(((ScorePage *)(input->node))->sched_residentLoc);
      break;
    case SCORE_EVENT_STOPSEGMENT:
      if (VERBOSEDEBUG || DEBUG) {
	cout << "   SIM: receive token SCORE_EVENT_STOPSEGMENT - " 
	     << currTimeSlice << endl; 
      }
      stopSegment(((ScoreSegment *)(input->node))->sched_residentLoc);
      break;
    case SCORE_EVENT_STARTSEGMENT:
      if (VERBOSEDEBUG || DEBUG) {
	cout << "   SIM: receive token SCORE_EVENT_STARTSEGMENT - " 
	     << currTimeSlice << endl; 
      }
      startSegment(((ScoreSegment *)(input->node))->sched_residentLoc);
      break;
    case SCORE_EVENT_DUMPPAGESTATE:
      if (VERBOSEDEBUG || DEBUG) {
	cout << "   SIM: receive token SCORE_EVENT_DUMPPAGESTATE - " 
	     << currTimeSlice << endl; 
      }
      dumpPageState(((ScorePage *)(input->node))->sched_residentLoc,
		    ((ScorePage *)(input->node))->sched_dumpSegmentBlock->parentTable->loc);
      break;
    case SCORE_EVENT_DUMPPAGEFIFO:
      if (VERBOSEDEBUG || DEBUG) {
	cout << "   SIM: receive token SCORE_EVENT_DUMPPAGEFIFO - " 
	     << currTimeSlice << endl; 
      }
      dumpPageFIFO(((ScorePage *)(input->node))->sched_residentLoc,
		   ((ScorePage *)(input->node))->sched_dumpSegmentBlock->parentTable->loc);
      break;
    case SCORE_EVENT_LOADPAGECONFIG:
      if (VERBOSEDEBUG || DEBUG) {
	cout << "   SIM: receive token SCORE_EVENT_LOADPAGECONFIG - " 
	     << currTimeSlice << endl; 
      }
      loadPageConfig(((ScorePage *)(input->node))->sched_residentLoc,
		     ((ScorePage *)(input->node))->sched_cachedSegmentBlock->parentTable->loc,
		     (ScorePage *)(input->node));
      break;
    case SCORE_EVENT_LOADPAGESTATE:
      if (VERBOSEDEBUG || DEBUG) {
	cout << "   SIM: receive token SCORE_EVENT_LOADPAGESTATE - " 
	     << currTimeSlice << endl; 
      }
      loadPageState(((ScorePage *)(input->node))->sched_residentLoc,
		    ((ScorePage *)(input->node))->sched_cachedSegmentBlock->parentTable->loc);
      break;
    case SCORE_EVENT_LOADPAGEFIFO:
      if (VERBOSEDEBUG || DEBUG) {
	cout << "   SIM: receive token SCORE_EVENT_LOADPAGEFIFO - " 
	     << currTimeSlice << endl; 
      }
      loadPageFIFO(((ScorePage *)(input->node))->sched_residentLoc,
		   ((ScorePage *)(input->node))->sched_cachedSegmentBlock->parentTable->loc);
      break;
    case SCORE_EVENT_GETSEGMENTPOINTERS:
      if (VERBOSEDEBUG || DEBUG) {
	cout << "   SIM: receive token SCORE_EVENT_GETSEGMENTPOINTERS - " 
	     << currTimeSlice << endl; 
      }
      getSegmentPointers(((ScoreSegment *)(input->node))->sched_residentLoc);
      break;
    case SCORE_EVENT_DUMPSEGMENTFIFO:
      if (VERBOSEDEBUG || DEBUG) {
	cout << "   SIM: receive token SCORE_EVENT_DUMPSEGMENTFIFO - " 
	     << currTimeSlice << endl; 
      }
      dumpSegmentFIFO(((ScoreSegment *)(input->node))->sched_residentLoc,
		      ((ScoreSegment *)(input->node))->sched_dumpSegmentBlock->parentTable->loc);
      break;
    case SCORE_EVENT_SETSEGMENTCONFIGPOINTERS:
      if (VERBOSEDEBUG || DEBUG) {
	cout << "   SIM: receive token SCORE_EVENT_SETSEGMENTCONFIGPOINTERS - " 
	     << currTimeSlice << endl; 
      }
      setSegmentConfigPointers(((ScoreSegment *)(input->node))->sched_residentLoc,
			       (ScoreSegment *)(input->node));
      break;
    case SCORE_EVENT_CHANGESEGMENTMODE:
      if (VERBOSEDEBUG || DEBUG) {
        cout << "   SIM: receive token SCORE_EVENT_CHANGESEGMENTMODE - "
             << currTimeSlice << endl;
      }
      changeSegmentMode(((ScoreSegment *)(input->node))->sched_residentLoc,
		        (ScoreSegment *)(input->node));
      break;
    case SCORE_EVENT_CHANGESEGMENTTRAANDPBOANDMAX:
      if (VERBOSEDEBUG || DEBUG) {
        cout << "   SIM: receive token SCORE_EVENT_CHANGESEGMENTTRAANDPBOANDMAX - "
             << currTimeSlice << endl;
      }
      changeSegmentTRAandPBOandMAX(((ScoreSegment *)(input->node))->sched_residentLoc,
		              (ScoreSegment *)(input->node));
      break;
    case SCORE_EVENT_RESETSEGMENTDONEFLAG:
      if (VERBOSEDEBUG || DEBUG) {
        cout << "   SIM: receive token SCORE_EVENT_RESETSEGMENTDONEFLAG - "
             << currTimeSlice << endl;
      }
      resetSegmentDoneFlag(((ScoreSegment *)(input->node))->sched_residentLoc,
			   (ScoreSegment *)(input->node));
      break;
    case SCORE_EVENT_LOADSEGMENTFIFO:
      if (VERBOSEDEBUG || DEBUG) {
	cout << "   SIM: receive token SCORE_EVENT_LOADSEGMENTFIFO - " 
	     << currTimeSlice << endl; 
      }
      loadSegmentFIFO(((ScoreSegment *)(input->node))->sched_residentLoc,
		      ((ScoreSegment *)(input->node))->sched_cachedSegmentBlock->parentTable->loc);
      break;
    case SCORE_EVENT_MEMXFERPRIMARYTOCMB:
      if (VERBOSEDEBUG || DEBUG) {
	cout << "   SIM: receive token SCORE_EVENT_MEMXFERPRIMARYTOCMB - " 
	     << currTimeSlice << endl; 
      }
      memXferPrimaryToCMB(input->sinkBlockLoc, input->xferSize);
      break;
    case SCORE_EVENT_MEMXFERCMBTOPRIMARY:
      if (VERBOSEDEBUG || DEBUG) {
	cout << "   SIM: receive token SCORE_EVENT_MEMXFERCMBTOPRIMARY - " 
	     << currTimeSlice << endl; 
      }
      memXferCMBToPrimary(input->srcBlockLoc, input->xferSize);
      break;
    case SCORE_EVENT_MEMXFERCMBTOCMB:
      if (VERBOSEDEBUG || DEBUG) {
	cout << "   SIM: receive token SCORE_EVENT_MEMXFERCMBTOCMB - " 
	     << currTimeSlice << endl; 
      }
      memXferCMBToCMB(input->srcBlockLoc,input->sinkBlockLoc, input->xferSize);
      break;
    case SCORE_EVENT_CONNECTSTREAM:
      if (VERBOSEDEBUG || DEBUG) {
	cout << "   SIM: receive token SCORE_EVENT_CONNECTSTREAM - " 
	     << currTimeSlice << endl; 
      }
      connectStream(input->srcNode, input->sinkNode);

      if (input->srcNode != NULL) {
	input->srcNode->getSchedOutput(input->outputNum)->syncSchedToReal();

	SCORE_MARKSTREAM(input->srcNode->getSchedOutput(input->outputNum),
			 threadCounter);
        SCORE_UNMARKREADSTREAM(input->srcNode->getSchedOutput(input->outputNum));
        SCORE_UNMARKWRITESTREAM(input->srcNode->getSchedOutput(input->outputNum));
      } else if (input->sinkNode != NULL) {
	input->sinkNode->getSchedInput(input->inputNum)->syncSchedToReal();

	SCORE_MARKSTREAM(input->sinkNode->getSchedInput(input->inputNum),
			 threadCounter);
        SCORE_UNMARKREADSTREAM(input->sinkNode->getSchedInput(input->inputNum));
        SCORE_UNMARKWRITESTREAM(input->sinkNode->getSchedInput(input->inputNum));
      }

      break;
    default:
      if (VERBOSEDEBUG || DEBUG) {
	cout << "   SIMERR: unknown command " << input->command 
	     << " - " << currTimeSlice << endl;
	exit(-1);
      }
  }
}

//////////////////////////////////////////////////////////////////////////////
// ScoreSimulator::printStats
//   print the statistics of commands from scheduler to simulator
//   for debugging
//
// Parameters: None
//
// Return value: None.
//////////////////////////////////////////////////////////////////////////////
void ScoreSimulator::printStats() {
  cout << "SCORE_EVENT_RUNUNTIL                    : " << statsOfSchedToSimCommands[0] << endl;
  cout << "SCORE_EVENT_GETARRAYSTATUS              : " << statsOfSchedToSimCommands[1] << endl;
  cout << "SCORE_EVENT_STOPPAGE                    : " << statsOfSchedToSimCommands[2] << endl;
  cout << "SCORE_EVENT_STARTPAGE                   : " << statsOfSchedToSimCommands[3] << endl;
  cout << "SCORE_EVENT_STOPSEGMENT                 : " << statsOfSchedToSimCommands[4] << endl;
  cout << "SCORE_EVENT_STARTSEGMENT                : " << statsOfSchedToSimCommands[5] << endl;
  cout << "SCORE_EVENT_DUMPPAGESTATE               : " << statsOfSchedToSimCommands[6] << endl;
  cout << "SCORE_EVENT_DUMPPAGEFIFO                : " << statsOfSchedToSimCommands[7] << endl;
  cout << "SCORE_EVENT_LOADPAGECONFIG              : " << statsOfSchedToSimCommands[8] << endl;
  cout << "SCORE_EVENT_LOADPAGESTATE               : " << statsOfSchedToSimCommands[9] << endl;
  cout << "SCORE_EVENT_LOADPAGEFIFO                : " << statsOfSchedToSimCommands[10] << endl;
  cout << "SCORE_EVENT_GETSEGMENTPOINTERS          : " << statsOfSchedToSimCommands[11] << endl;
  cout << "SCORE_EVENT_DUMPSEGMENTFIFO             : " << statsOfSchedToSimCommands[12] << endl;
  cout << "SCORE_EVENT_SETSEGMENTCONFIGPOINTERS    : " << statsOfSchedToSimCommands[13] << endl;
  cout << "SCORE_EVENT_CHANGESEGMENTMODE           : " << statsOfSchedToSimCommands[14] << endl;
  cout << "SCORE_EVENT_CHANGESEGMENTTRAANDPBOANDMAX: " << statsOfSchedToSimCommands[15] << endl;
  cout << "SCORE_EVENT_RESETSEGMENTDONEFLAG        : " << statsOfSchedToSimCommands[16] << endl;
  cout << "SCORE_EVENT_LOADSEGMENTFIFO             : " << statsOfSchedToSimCommands[17] << endl;
  cout << "SCORE_EVENT_MEMXFERPRIMARYTOCMB         : " << statsOfSchedToSimCommands[18] << endl;
  cout << "SCORE_EVENT_MEMXFERCMBTOPRIMARY         : " << statsOfSchedToSimCommands[19] << endl;
  cout << "SCORE_EVENT_MEMXFERCMBTOCMB             : " << statsOfSchedToSimCommands[20] << endl;
  cout << "SCORE_EVENT_CONNECTSTREAM               : " << statsOfSchedToSimCommands[21] << endl;
}
