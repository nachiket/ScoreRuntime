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
// SCORE runtime support
// $Revision: 1.54 $
//
//////////////////////////////////////////////////////////////////////////////

#include <math.h>
#include <string.h>
#include "LEDA/core/list.h"
#include "ScorePage.h"
#include "ScoreSegment.h"
#include "ScoreGraphNode.h"
#include "ScoreSegmentTable.h"
#include "ScoreStream.h"
#include "ScoreRuntime.h"
#include "ScoreSyncEvent.h"
#include "ScoreSimulator.h"
#include "ScoreHardwareAPI.h"
#include "ScoreConfig.h"


// this stores the virtual time of the scheduler.
unsigned int schedulerVirtualTime = 0;


// this is used internally to batch up commands.
list<ScoreSyncEvent *> *commandBatch = NULL;


///////////////////////////////////////////////////////////////////////////////
// getArrayInfo:
//   Returns the number of physical pages and segments, and the physical
//     size of a CMB.
//
// Parameters:
//   numPhysicalCP: pointer to where the number of physical pages should be
//                  stored.
//   numPhysicalCMB: pointer to where the number of physical segment should be
//                   stored.
//   cmbSize: pointer to where the physical size of a CMB should be stored.
//
// Return value:
//   0 if successful; -1 if not successful.
///////////////////////////////////////////////////////////////////////////////
int getArrayInfo(unsigned int *numPhysicalCP, unsigned int *numPhysicalCMB,
		 unsigned int *cmbSize) {
  // return the array parameters.
  (*numPhysicalCP) = numPhysicalPages;
  (*numPhysicalCMB) = numPhysicalSegments;
  (*cmbSize) = physicalSegmentSize;

  return(0);
}


///////////////////////////////////////////////////////////////////////////////
// getArrayStatus:
//   Return the current array status.
//
// Parameters:
//   cpStatus: pointer to location to store physical page status.
//   cmbStatus: pointer to location to store physical segment status.
//   cpMask: mask to specify which CPs we care about.
//   cmbMask: mask to specify which CMBs we care about.
//
// Return value:
//   0 if successful; -1 if not successful.
///////////////////////////////////////////////////////////////////////////////
int getArrayStatus(ScoreArrayCPStatus *cpStatus,
		   ScoreArrayCMBStatus *cmbStatus,
		   char *cpMask, char *cmbMask) {
  ScoreSyncEvent *event;


  // create the event.
  event = new ScoreSyncEvent();
  if (event == NULL) {
    cerr << "SCHEDERR: Insufficient memory to instantiate event!" << endl;
    return(-1);
  }
  event->currentTime = schedulerVirtualTime;
  event->command = SCORE_EVENT_GETARRAYSTATUS;
  event->cpStatus = cpStatus;
  event->cmbStatus = cmbStatus;
  event->cpMask = cpMask;
  event->cmbMask = cmbMask;

  if (batchCommandBegin() != 0) {
    return(-1);
  }
  commandBatch->append(event);
  if (batchCommandEnd() != 0) {
    return(-1);
  }

  return(0);
}


///////////////////////////////////////////////////////////////////////////////
// batchCommandBegin:
//   Begin a command batch.
//
// Parameters: None.
//
// Return value:
//   0 if successful; -1 if not successful.
///////////////////////////////////////////////////////////////////////////////
int batchCommandBegin() {
  // make sure that there is not a command batch already.
  if (commandBatch != NULL) {
    cerr << "SCHEDERR: Trying to start another command batch before " <<
      "ending current batch!" << endl;
    return(-1);
  }

  // instantiate a new command batch.
  commandBatch = new list<ScoreSyncEvent *>();
  if (commandBatch == NULL) {
    cerr << "SCHEDERR: Insufficient memory to instantiate command batch!" << 
      endl;
    return(-1);
  }

  return(0);
}


///////////////////////////////////////////////////////////////////////////////
// batchCommandEnd:
//   Ends a command batch and issue the batch.
//
// Parameters: None.
//
// Return value:
//   0 if successful; -1 if not successful.
///////////////////////////////////////////////////////////////////////////////
int batchCommandEnd() {
  list_item listItem;
  unsigned int maxOffset = 0;
  unsigned int getArrayStatus = 0;
  unsigned int setSegmentConfigPointers = 0;
  unsigned int changeSegmentMode = 0;
  unsigned int changeSegmentTRAandPBOandMAX = 0;
  unsigned int resetSegmentDoneFlag = 0;
  unsigned int getSegmentPointers = 0;
  unsigned int memXferPrimToFromCMBs = 0;
  unsigned int issueOffset = 0;
  ScoreSegment *segmentNode = NULL;
  ScoreArrayCPStatus *cpStatus = NULL;
  ScoreArrayCMBStatus *cmbStatus = NULL;
  char *cpMask = NULL;
  char *cmbMask = NULL;
  char isRunUntilCommand = 0;
  char shouldWaitForACK;


  // make sure there is a command batch.
  if (commandBatch == NULL) {
    cerr << "SCHEDERR: Trying to end a command batch before " <<
      "starting a batch!" << endl;
    return(-1);
  }

  // make sure there is something in the command batch.
  if (commandBatch->length() == 0) {
    // reset the command batch.
    {
      list_item oldItemIter;

      forall_items(oldItemIter, (*commandBatch)) {
	ScoreSyncEvent *oldEvent = commandBatch->inf(oldItemIter);

	oldEvent->cpStatus = NULL;
	oldEvent->cmbStatus = NULL;
	delete(oldEvent);
      }
    }
    delete(commandBatch);
    commandBatch = NULL;

    return(0);
  }

  // find the maximum offset time for all of the batched commands.
  forall_items(listItem, (*commandBatch)) {
    ScoreSyncEvent *currentEvent = commandBatch->inf(listItem);
    unsigned int currentOffset = issueOffset;

    switch(currentEvent->command) {
    case SCORE_EVENT_RUNUNTIL:
      isRunUntilCommand = 1;
      currentOffset = currentOffset + 0; break;
    case SCORE_EVENT_GETARRAYSTATUS:
      currentOffset = currentOffset + SIM_COST_GETARRAYSTATUS; 
      cpStatus = currentEvent->cpStatus;
      cmbStatus = currentEvent->cmbStatus;
      currentEvent->cpStatus = NULL;
      currentEvent->cmbStatus = NULL;
      cpMask = new char[numPhysicalPages];
      bcopy(currentEvent->cpMask, cpMask, numPhysicalPages);
      cmbMask = new char[numPhysicalSegments];
      bcopy(currentEvent->cmbMask, cmbMask, numPhysicalSegments);
      getArrayStatus++;
      break;
    case SCORE_EVENT_STOPPAGE:
      currentOffset = currentOffset + SIM_COST_STOPPAGE; break;
    case SCORE_EVENT_STARTPAGE:
      currentOffset = currentOffset + SIM_COST_STARTPAGE; break;
    case SCORE_EVENT_STOPSEGMENT:
      currentOffset = currentOffset + SIM_COST_STOPSEGMENT; break;
    case SCORE_EVENT_STARTSEGMENT:
      currentOffset = currentOffset + SIM_COST_STARTSEGMENT; break;
    case SCORE_EVENT_DUMPPAGESTATE:
      // FIX ME! Really should take into account the page state size as a
      //         variable!
      currentOffset = currentOffset + SIM_COST_DUMPPAGESTATE; break;
    case SCORE_EVENT_DUMPPAGEFIFO:
      // FIX ME! Really should take into account the page FIFO size and
      //         number of inputs as a variable!
      currentOffset = currentOffset + SIM_COST_DUMPPAGEFIFO; break;
    case SCORE_EVENT_LOADPAGECONFIG:
      // FIX ME! Really should take into account the page config size as a
      //         variable!
      currentOffset = currentOffset + SIM_COST_LOADPAGECONFIG; break;
    case SCORE_EVENT_LOADPAGESTATE:
      // FIX ME! Really should take into account the page config size as a
      //         variable!
      currentOffset = currentOffset + SIM_COST_LOADPAGESTATE; break;
    case SCORE_EVENT_LOADPAGEFIFO:
      // FIX ME! Really should take into account the page FIFO size and
      //         number of inputs as a variable!
      currentOffset = currentOffset + SIM_COST_LOADPAGEFIFO; break;
    case SCORE_EVENT_GETSEGMENTPOINTERS:
      currentOffset = currentOffset + SIM_COST_GETSEGMENTPOINTERS; break;
      getSegmentPointers++; segmentNode = (ScoreSegment *) currentEvent->node;
      break;
    case SCORE_EVENT_DUMPSEGMENTFIFO:
      // FIX ME! Really should take into account the segment FIFO size and
      //         number of inputs as a variable!
      currentOffset = currentOffset + SIM_COST_DUMPSEGMENTFIFO; break;
    case SCORE_EVENT_SETSEGMENTCONFIGPOINTERS:
      currentOffset = currentOffset + SIM_COST_SETSEGMENTCONFIGPOINTERS; 
      setSegmentConfigPointers++; break;
    case SCORE_EVENT_CHANGESEGMENTMODE:
      currentOffset = currentOffset + SIM_COST_CHANGESEGMENTMODE; 
      changeSegmentMode++; break;
    case SCORE_EVENT_CHANGESEGMENTTRAANDPBOANDMAX:
      currentOffset = currentOffset + SIM_COST_CHANGESEGMENTTRAANDPBOANDMAX; 
      changeSegmentTRAandPBOandMAX++; break;
    case SCORE_EVENT_RESETSEGMENTDONEFLAG:
      currentOffset = currentOffset + SIM_COST_RESETSEGMENTDONEFLAG;
      resetSegmentDoneFlag++; break;
    case SCORE_EVENT_LOADSEGMENTFIFO:
      // FIX ME! Really should take into account the segment FIFO size and
      //         number of inputs as a variable!
      currentOffset = currentOffset + SIM_COST_LOADSEGMENTFIFO; break;
    case SCORE_EVENT_MEMXFERPRIMARYTOCMB:
      // FIX ME! Really should take into account the size of the memory to
      //         transfer.
      currentOffset = currentOffset + 
        ((unsigned int) 
	 ceil(SIM_COST_MEMXFERPRIMARYTOCMB*currentEvent->xferSize));
      memXferPrimToFromCMBs++; break;
    case SCORE_EVENT_MEMXFERCMBTOPRIMARY:
      // FIX ME! Really should take into account the size of the memory to
      //         transfer.
      currentOffset = currentOffset + 
        ((unsigned int)
	 ceil(SIM_COST_MEMXFERCMBTOPRIMARY*currentEvent->xferSize));
      memXferPrimToFromCMBs++; break;
    case SCORE_EVENT_MEMXFERCMBTOCMB:
      // FIX ME! Really should take into account the size of the memory to
      //         transfer.
      currentOffset = currentOffset + 
        ((unsigned int)
	 ceil(SIM_COST_MEMXFERCMBTOCMB*currentEvent->xferSize));
      break;
    case SCORE_EVENT_CONNECTSTREAM:
      currentOffset = currentOffset + SIM_COST_CONNECTSTREAM; break;
    default:
      cerr << "SCHEDERR: Unrecognized event in the command batch! " <<
	"(event=" << currentEvent->command << ")" << endl;

      // reset the command batch.
      {
	list_item oldItemIter;
	
	forall_items(oldItemIter, (*commandBatch)) {
	  ScoreSyncEvent *oldEvent = commandBatch->inf(oldItemIter);
	  
	  oldEvent->cpStatus = NULL;
	  oldEvent->cmbStatus = NULL;
	  delete(oldEvent);
	}
      }
      delete(commandBatch);
      commandBatch = NULL;

      return(-1);
    }

    if (currentOffset > maxOffset) {
      maxOffset = currentOffset;
    }

    if (!isPseudoIdeal) {
#if SCHED_VIRT_TIME_BREAKDOWN && NO_COST
	
#else
      issueOffset++;
#endif
    }
  }

  // if we are trying to get array status, set or get segment config/pointers, 
  // or run-until then make sure only one command is in the batch.
  if ((getArrayStatus > 0) || 
      (setSegmentConfigPointers > 0) || (getSegmentPointers > 0) ||
      (changeSegmentMode > 0) || (changeSegmentTRAandPBOandMAX > 0) ||
      (resetSegmentDoneFlag > 0) ||
      (isRunUntilCommand)) {
    if (commandBatch->length() != 1) {
      cerr << "SCHEDERR: Attempting to batch a get status, a segment " <<
	"config set/get, run-until with other commands!" << endl;

      // reset the command batch.
      {
	list_item oldItemIter;
	
	forall_items(oldItemIter, (*commandBatch)) {
	  ScoreSyncEvent *oldEvent = commandBatch->inf(oldItemIter);
	  
	  oldEvent->cpStatus = NULL;
	  oldEvent->cmbStatus = NULL;
	  delete(oldEvent);
	}
      }
      delete(commandBatch);
      commandBatch = NULL;

      return(-1);
    }
  }

  // only flag that we need to wait for an ACK if we are not getting a
  // return value already.
  if ((getArrayStatus > 0) || 
      (getSegmentPointers > 0) ||
      (isRunUntilCommand)) {
    shouldWaitForACK = 0;
  } else {
    shouldWaitForACK = 1;
  }

  // make sure not more than 1 primary<->CMB transfer is happening.
  if (memXferPrimToFromCMBs > 1) {
    cerr << "SCHEDERR: Attempting to batch more than 1 primary<->CMB " <<
      "transfer at a time!" << endl;

    // reset the command batch.
    {
      list_item oldItemIter;

      forall_items(oldItemIter, (*commandBatch)) {
	ScoreSyncEvent *oldEvent = commandBatch->inf(oldItemIter);

	oldEvent->cpStatus = NULL;
	oldEvent->cmbStatus = NULL;
	delete(oldEvent);
      }
    }
    delete(commandBatch);
    commandBatch = NULL;

    return(-1);
  }

  // FIX ME! SHOULD CHECK FOR CONFLICTS IN USAGES! (USING SAME RESOURCE
  // TWICE!).

  // write the command batch to the stream sequentially.
  issueOffset = 0;
  forall_items(listItem, (*commandBatch)) {
    ScoreSyncEvent *currentEvent = commandBatch->inf(listItem);
    
    currentEvent->currentTime = currentEvent->currentTime + issueOffset;
    if (!isPseudoIdeal) {
#if SCHED_VIRT_TIME_BREAKDOWN && NO_COST
#else
      issueOffset++;
#endif
    }

    if (VERBOSEDEBUG || DEBUG) {
      cerr << "SCHED: SENDING COMMAND " << eventNameMap[currentEvent->command] 
	   << " WITH TIME " << currentEvent->currentTime << endl;
    }

    STREAM_WRITE_RT(toSimulator, currentEvent);

    // if we are supposed to wait for acknowledgement, then wait for it.
    if (shouldWaitForACK) {
      ScoreSyncEvent *nextEvent = 0;
      STREAM_READ_RT(fromSimulator,nextEvent);

      // make sure got an acknowledgement.
      if (nextEvent->command != SCORE_EVENT_ACK) {
	cerr << "SCHEDERR: Did not get SCORE_EVENT_ACK! " <<
	  "(event=" << eventNameMap[nextEvent->command] << ")" << endl;
	{
	  list_item oldItemIter;
	  
	  forall_items(oldItemIter, (*commandBatch)) {
	    ScoreSyncEvent *oldEvent = commandBatch->inf(oldItemIter);
	    
	    oldEvent->cpStatus = NULL;
	    oldEvent->cmbStatus = NULL;
	    delete(oldEvent);
	  }
	}
	delete(commandBatch);
	commandBatch = NULL;
	return(-1);
      }
    }
  }

  schedulerVirtualTime = schedulerVirtualTime + maxOffset;

  // reset the command batch.
  delete(commandBatch);
  commandBatch = NULL;

  // if we are waiting for a reply, then wait for the reply.
  if ((getArrayStatus > 0) || (getSegmentPointers > 0)) {
    ScoreSyncEvent *nextEvent = 0;
    STREAM_READ_RT(fromSimulator,nextEvent);

    // if we are trying to get array status, then wait for the return value.
    if (getArrayStatus > 0) {
      unsigned int i;
      ScoreArrayCPStatus *newCPStatus = nextEvent->cpStatus;
      ScoreArrayCMBStatus *newCMBStatus = nextEvent->cmbStatus;

      // make sure we got back the right event.
      if (nextEvent->command != SCORE_EVENT_RETURNARRAYSTATUS) {
	cerr << "SCHEDERR: Did not get SCORE_EVENT_RETURNARRAYSTATUS! " <<
	  "(event=" << eventNameMap[nextEvent->command] << ")" << endl;
	return(-1);
      }

      // copy the results over.
      for (i = 0; i < numPhysicalPages; i++) {
	if (cpMask[i]) {
	  cpStatus[i].isDone = newCPStatus[i].isDone;
	  cpStatus[i].isStalled = newCPStatus[i].isStalled;
	  cpStatus[i].stallCount = newCPStatus[i].stallCount;
	  cpStatus[i].currentState = newCPStatus[i].currentState;
	  cpStatus[i].emptyInputs = newCPStatus[i].emptyInputs;
	  cpStatus[i].fullOutputs = newCPStatus[i].fullOutputs;

	  // if there is a previous inputConsumption/outputProduction,
	  // free them first.
	  if (cpStatus[i].inputConsumption != NULL) {
	    delete(cpStatus[i].inputConsumption);
	    cpStatus[i].inputConsumption = NULL;
	  }
	  if (cpStatus[i].outputProduction != NULL) {
	    delete(cpStatus[i].outputProduction);
	    cpStatus[i].outputProduction = NULL;
	  }

	  cpStatus[i].inputConsumption = newCPStatus[i].inputConsumption;
	  cpStatus[i].outputProduction = newCPStatus[i].outputProduction;
	  
	  // the pointers are reset so they will not be freed.
	  newCPStatus[i].inputConsumption = NULL;
	  newCPStatus[i].outputProduction = NULL;
	  
	  // if there is a previous inputFIFONumTokens
	  // free them first.
	  if (cpStatus[i].inputFIFONumTokens != NULL) {
	    delete(cpStatus[i].inputFIFONumTokens);
	    cpStatus[i].inputFIFONumTokens = NULL;
	  }
	  
	  cpStatus[i].inputFIFONumTokens = newCPStatus[i].inputFIFONumTokens;
	  
	  // the pointers are reset so they will not be freed.
	  newCPStatus[i].inputFIFONumTokens = NULL;
	}
      }
      for (i = 0; i < numPhysicalSegments; i++) {
	if (cmbMask[i]) {
	  cmbStatus[i].isDone = newCMBStatus[i].isDone;
	  cmbStatus[i].isStalled = newCMBStatus[i].isStalled;
	  cmbStatus[i].isFaulted = newCMBStatus[i].isFaulted;
	  cmbStatus[i].stallCount = newCMBStatus[i].stallCount;
	  cmbStatus[i].faultedAddr = newCMBStatus[i].faultedAddr;
	  cmbStatus[i].readAddr = newCMBStatus[i].readAddr;
	  cmbStatus[i].writeAddr = newCMBStatus[i].writeAddr;
	  cmbStatus[i].readCount = newCMBStatus[i].readCount;
	  cmbStatus[i].writeCount = newCMBStatus[i].writeCount;
	  cmbStatus[i].emptyInputs = newCMBStatus[i].emptyInputs;
	  cmbStatus[i].fullOutputs = newCMBStatus[i].fullOutputs;
	  
	  // if there is a previous inputFIFONumTokens/outputFIFONumTokens,
	  // free them first.
	  if (cmbStatus[i].inputFIFONumTokens != NULL) {
	    delete(cmbStatus[i].inputFIFONumTokens);
	    cmbStatus[i].inputFIFONumTokens = NULL;
	  }
	  
	  cmbStatus[i].inputFIFONumTokens = newCMBStatus[i].inputFIFONumTokens;
	  
	  // the pointers are reset so they will not be freed.
	  newCMBStatus[i].inputFIFONumTokens = NULL;
	}
      }

      delete(cpMask);
      cpMask = NULL;
      delete(cmbMask);
      cmbMask = NULL;
    }

    // if we are trying to get segment pointers, then wait for the return
    // value.
    if (getSegmentPointers > 0) {
      // make sure we got back the right event.
      if (nextEvent->command != SCORE_EVENT_RETURNSEGMENTPOINTERS) {
	cerr << "SCHEDERR: Did not get SCORE_EVENT_RETURNSEGMENTPOINTERS! " <<
	  "(event=" << eventNameMap[nextEvent->command] << ")" << endl;
	return(-1);
      }

      // set the values.
      segmentNode->readAddr = nextEvent->readAddr;
      segmentNode->writeAddr = nextEvent->writeAddr;
    }

    delete(nextEvent);
  }

  return(0);
}


///////////////////////////////////////////////////////////////////////////////
// stopPage:
//   Stops a physical page.
//
// Parameters:
//   page: a pointer to the page to stop.
//
// Return value:
//   0 if successful; -1 if not successful.
///////////////////////////////////////////////////////////////////////////////
int stopPage(ScorePage *page) {
  ScoreSyncEvent *event;


  // make sure there is a command batch.
  if (commandBatch == NULL) {
    cerr << "SCHEDERR: Trying to issue a command before starting a batch!" << 
      endl;
    return(-1);
  }

  // create the event.
  event = new ScoreSyncEvent();
  if (event == NULL) {
    cerr << "SCHEDERR: Insufficient memory to instantiate event!" << endl;
    return(-1);
  }
  event->currentTime = schedulerVirtualTime;
  event->command = SCORE_EVENT_STOPPAGE;
  event->node = page;

  commandBatch->append(event);

  return(0);
}


///////////////////////////////////////////////////////////////////////////////
// startPage:
//   Starts a physical page.
//
// Parameters:
//   page: a pointer to the page to start.
//
// Return value:
//   0 if successful; -1 if not successful.
///////////////////////////////////////////////////////////////////////////////
int startPage(ScorePage *page) {
  ScoreSyncEvent *event;


  // make sure there is a command batch.
  if (commandBatch == NULL) {
    cerr << "SCHEDERR: Trying to issue a command before starting a batch!" << 
      endl;
    return(-1);
  }

  // create the event.
  event = new ScoreSyncEvent();
  if (event == NULL) {
    cerr << "SCHEDERR: Insufficient memory to instantiate event!" << endl;
    return(-1);
  }
  event->currentTime = schedulerVirtualTime;
  event->command = SCORE_EVENT_STARTPAGE;
  event->node = page;

  commandBatch->append(event);

  return(0);
}


///////////////////////////////////////////////////////////////////////////////
// stopSegment:
//   Stops a physical segment.
//
// Parameters:
//   segment: a pointer to the segment to stop.
//
// Return value:
//   0 if successful; -1 if not successful.
///////////////////////////////////////////////////////////////////////////////
int stopSegment(ScoreSegment *segment) {
  ScoreSyncEvent *event;


  // make sure there is a command batch.
  if (commandBatch == NULL) {
    cerr << "SCHEDERR: Trying to issue a command before starting a batch!" << 
      endl;
    return(-1);
  }

  // create the event.
  event = new ScoreSyncEvent();
  if (event == NULL) {
    cerr << "SCHEDERR: Insufficient memory to instantiate event!" << endl;
    return(-1);
  }
  event->currentTime = schedulerVirtualTime;
  event->command = SCORE_EVENT_STOPSEGMENT;
  event->node = segment;

  commandBatch->append(event);

  return(0);
}


///////////////////////////////////////////////////////////////////////////////
// startSegment:
//   Starts a physical segment.
//
// Parameters:
//   segment: a pointer to the segment to start.
//
// Return value:
//   0 if successful; -1 if not successful.
///////////////////////////////////////////////////////////////////////////////
int startSegment(ScoreSegment *segment) {
  ScoreSyncEvent *event;


  // make sure there is a command batch.
  if (commandBatch == NULL) {
    cerr << "SCHEDERR: Trying to issue a command before starting a batch!" << 
      endl;
    return(-1);
  }

  // create the event.
  event = new ScoreSyncEvent();
  if (event == NULL) {
    cerr << "SCHEDERR: Insufficient memory to instantiate event!" << endl;
    return(-1);
  }
  event->currentTime = schedulerVirtualTime;
  event->command = SCORE_EVENT_STARTSEGMENT;
  event->node = segment;

  commandBatch->append(event);

  return(0);
}


///////////////////////////////////////////////////////////////////////////////
// dumpPageState:
//   Dumps a page state to a memory segment.
//
// Parameters:
//   page: a pointer to the page whose state is to be dumped.
//
// Return value:
//   0 if successful; -1 if not successful.
///////////////////////////////////////////////////////////////////////////////
int dumpPageState(ScorePage *page) {
  ScoreSyncEvent *event;


  // make sure there is a command batch.
  if (commandBatch == NULL) {
    cerr << "SCHEDERR: Trying to issue a command before starting a batch!" << 
      endl;
    return(-1);
  }

  // create the event.
  event = new ScoreSyncEvent();
  if (event == NULL) {
    cerr << "SCHEDERR: Insufficient memory to instantiate event!" << endl;
    return(-1);
  }
  event->currentTime = schedulerVirtualTime;
  event->command = SCORE_EVENT_DUMPPAGESTATE;
  event->node = page;

  commandBatch->append(event);

  return(0);
}


///////////////////////////////////////////////////////////////////////////////
// dumpPageFIFO:
//   Dumps a page FIFOs to a memory segment.
//
// Parameters:
//   page: a pointer to the page whose FIFOs is to be dumped.
//
// Return value:
//   0 if successful; -1 if not successful.
///////////////////////////////////////////////////////////////////////////////
int dumpPageFIFO(ScorePage *page) {
  ScoreSyncEvent *event;


  // make sure there is a command batch.
  if (commandBatch == NULL) {
    cerr << "SCHEDERR: Trying to issue a command before starting a batch!" << 
      endl;
    return(-1);
  }

  // create the event.
  event = new ScoreSyncEvent();
  if (event == NULL) {
    cerr << "SCHEDERR: Insufficient memory to instantiate event!" << endl;
    return(-1);
  }
  event->currentTime = schedulerVirtualTime;
  event->command = SCORE_EVENT_DUMPPAGEFIFO;
  event->node = page;

  commandBatch->append(event);

  return(0);
}


///////////////////////////////////////////////////////////////////////////////
// loadPageConfig:
//   Loads a page config from a memory segment.
//
// Parameters:
//   page: a pointer to the page whose config is to be loaded.
//
// Return value:
//   0 if successful; -1 if not successful.
///////////////////////////////////////////////////////////////////////////////
int loadPageConfig(ScorePage *page) {
  ScoreSyncEvent *event;


  // make sure there is a command batch.
  if (commandBatch == NULL) {
    cerr << "SCHEDERR: Trying to issue a command before starting a batch!" << 
      endl;
    return(-1);
  }

  // create the event.
  event = new ScoreSyncEvent();
  if (event == NULL) {
    cerr << "SCHEDERR: Insufficient memory to instantiate event!" << endl;
    return(-1);
  }
  event->currentTime = schedulerVirtualTime;
  event->command = SCORE_EVENT_LOADPAGECONFIG;
  event->node = page;

  commandBatch->append(event);

  return(0);
}


///////////////////////////////////////////////////////////////////////////////
// loadPageState:
//   Loads a page state from a memory segment.
//
// Parameters:
//   page: a pointer to the page whose state is to be loaded.
//
// Return value:
//   0 if successful; -1 if not successful.
///////////////////////////////////////////////////////////////////////////////
int loadPageState(ScorePage *page) {
  ScoreSyncEvent *event;


  // make sure there is a command batch.
  if (commandBatch == NULL) {
    cerr << "SCHEDERR: Trying to issue a command before starting a batch!" << 
      endl;
    return(-1);
  }

  // create the event.
  event = new ScoreSyncEvent();
  if (event == NULL) {
    cerr << "SCHEDERR: Insufficient memory to instantiate event!" << endl;
    return(-1);
  }
  event->currentTime = schedulerVirtualTime;
  event->command = SCORE_EVENT_LOADPAGESTATE;
  event->node = page;

  commandBatch->append(event);

  return(0);
}


///////////////////////////////////////////////////////////////////////////////
// loadPageFIFO:
//   Loads a page FIFOs from a memory segment.
//
// Parameters:
//   page: a pointer to the page whose FIFOs is to be loaded.
//
// Return value:
//   0 if successful; -1 if not successful.
///////////////////////////////////////////////////////////////////////////////
int loadPageFIFO(ScorePage *page) {
  ScoreSyncEvent *event;


  // make sure there is a command batch.
  if (commandBatch == NULL) {
    cerr << "SCHEDERR: Trying to issue a command before starting a batch!" << 
      endl;
    return(-1);
  }

  // create the event.
  event = new ScoreSyncEvent();
  if (event == NULL) {
    cerr << "SCHEDERR: Insufficient memory to instantiate event!" << endl;
    return(-1);
  }
  event->currentTime = schedulerVirtualTime;
  event->command = SCORE_EVENT_LOADPAGEFIFO;
  event->node = page;

  commandBatch->append(event);

  return(0);
}


///////////////////////////////////////////////////////////////////////////////
// getSegmentPointers:
//   Saves segment pointers to the processor.
//
// Parameters:
//   segment: a pointer to the segment where the pointers should be saved.
//
// Return value:
//   0 if successful; -1 if not successful.
///////////////////////////////////////////////////////////////////////////////
int getSegmentPointers(ScoreSegment *segment) {
  ScoreSyncEvent *event;


  // make sure there is a command batch.
  if (commandBatch == NULL) {
    cerr << "SCHEDERR: Trying to issue a command before starting a batch!" << 
      endl;
    return(-1);
  }

  // create the event.
  event = new ScoreSyncEvent();
  if (event == NULL) {
    cerr << "SCHEDERR: Insufficient memory to instantiate event!" << endl;
    return(-1);
  }
  event->currentTime = schedulerVirtualTime;
  event->command = SCORE_EVENT_GETSEGMENTPOINTERS;
  event->node = segment;

  commandBatch->append(event);

  return(0);
}


///////////////////////////////////////////////////////////////////////////////
// dumpSegmentFIFO:
//   Dumps a segment FIFOs to a physical memory segment.
//
// Parameters:
//   segment: a pointer to the segment whose FIFOs is to be dumped.
//
// Return value:
//   0 if successful; -1 if not successful.
///////////////////////////////////////////////////////////////////////////////
int dumpSegmentFIFO(ScoreSegment *segment) {
  ScoreSyncEvent *event;


  // make sure there is a command batch.
  if (commandBatch == NULL) {
    cerr << "SCHEDERR: Trying to issue a command before starting a batch!" << 
      endl;
    return(-1);
  }

  // create the event.
  event = new ScoreSyncEvent();
  if (event == NULL) {
    cerr << "SCHEDERR: Insufficient memory to instantiate event!" << endl;
    return(-1);
  }
  event->currentTime = schedulerVirtualTime;
  event->command = SCORE_EVENT_DUMPSEGMENTFIFO;
  event->node = segment;

  commandBatch->append(event);

  return(0);
}


///////////////////////////////////////////////////////////////////////////////
// setSegmentConfigPointers:
//   Sets a segment configuration and pointers from the processor.
//
// Parameters:
//   segment: a pointer to the segment whose configuration and pointers are
//            to be set.
//
// Return value:
//   0 if successful; -1 if not successful.
///////////////////////////////////////////////////////////////////////////////
int setSegmentConfigPointers(ScoreSegment *segment) {
  ScoreSyncEvent *event;


  // make sure there is a command batch.
  if (commandBatch == NULL) {
    cerr << "SCHEDERR: Trying to issue a command before starting a batch!" << 
      endl;
    return(-1);
  }

  // create the event.
  event = new ScoreSyncEvent();
  if (event == NULL) {
    cerr << "SCHEDERR: Insufficient memory to instantiate event!" << endl;
    return(-1);
  }
  event->currentTime = schedulerVirtualTime;
  event->command = SCORE_EVENT_SETSEGMENTCONFIGPOINTERS;
  event->node = segment;

  commandBatch->append(event);

  return(0);
}


///////////////////////////////////////////////////////////////////////////////
// changeSegmentMode:
//   Changes the mode of a segment.
//
// Parameters:
//   segment: a pointer to the segment whose mode is to be changed.
//
// Return value:
//   0 if successful; -1 if not successful.
///////////////////////////////////////////////////////////////////////////////
int changeSegmentMode(ScoreSegment *segment) {
  ScoreSyncEvent *event;


  // make sure there is a command batch.
  if (commandBatch == NULL) {
    cerr << "SCHEDERR: Trying to issue a command before starting a batch!" << 
      endl;
    return(-1);
  }

  // create the event.
  event = new ScoreSyncEvent();
  if (event == NULL) {
    cerr << "SCHEDERR: Insufficient memory to instantiate event!" << endl;
    return(-1);
  }
  event->currentTime = schedulerVirtualTime;
  event->command = SCORE_EVENT_CHANGESEGMENTMODE;
  event->node = segment;

  commandBatch->append(event);

  return(0);
}


///////////////////////////////////////////////////////////////////////////////
// changeSegmentTRAandPBOandMAX:
//   Changes the TRA and PBO and MAX of a segment.
//
// Parameters:
//   segment: a pointer to the segment whose TRA and MAX is to be changed.
//
// Return value:
//   0 if successful; -1 if not successful.
///////////////////////////////////////////////////////////////////////////////
int changeSegmentTRAandPBOandMAX(ScoreSegment *segment) {
  ScoreSyncEvent *event;


  // make sure there is a command batch.
  if (commandBatch == NULL) {
    cerr << "SCHEDERR: Trying to issue a command before starting a batch!" << 
      endl;
    return(-1);
  }

  // create the event.
  event = new ScoreSyncEvent();
  if (event == NULL) {
    cerr << "SCHEDERR: Insufficient memory to instantiate event!" << endl;
    return(-1);
  }
  event->currentTime = schedulerVirtualTime;
  event->command = SCORE_EVENT_CHANGESEGMENTTRAANDPBOANDMAX;
  event->node = segment;

  commandBatch->append(event);

  return(0);
}


///////////////////////////////////////////////////////////////////////////////
// resetSegmentDoneFlag:
//   Reset the done flag for a segment.
//
// Parameters:
//   segment: a pointer to the segment whose done flag is to be reset.
//
// Return value:
//   0 if successful; -1 if not successful.
///////////////////////////////////////////////////////////////////////////////
int resetSegmentDoneFlag(ScoreSegment *segment) {
  ScoreSyncEvent *event;


  // make sure there is a command batch.
  if (commandBatch == NULL) {
    cerr << "SCHEDERR: Trying to issue a command before starting a batch!" << 
      endl;
    return(-1);
  }

  // create the event.
  event = new ScoreSyncEvent();
  if (event == NULL) {
    cerr << "SCHEDERR: Insufficient memory to instantiate event!" << endl;
    return(-1);
  }
  event->currentTime = schedulerVirtualTime;
  event->command = SCORE_EVENT_RESETSEGMENTDONEFLAG;
  event->node = segment;

  commandBatch->append(event);

  return(0);
}


///////////////////////////////////////////////////////////////////////////////
// loadSegmentFIFO:
//   Loads a segment FIFOs from a physical memory segment.
//
// Parameters:
//   segment: a pointer to the segment whose FIFOs is to be loaded.
//
// Return value:
//   0 if successful; -1 if not successful.
///////////////////////////////////////////////////////////////////////////////
int loadSegmentFIFO(ScoreSegment *segment) {
  ScoreSyncEvent *event;


  // make sure there is a command batch.
  if (commandBatch == NULL) {
    cerr << "SCHEDERR: Trying to issue a command before starting a batch!" << 
      endl;
    return(-1);
  }

  // create the event.
  event = new ScoreSyncEvent();
  if (event == NULL) {
    cerr << "SCHEDERR: Insufficient memory to instantiate event!" << endl;
    return(-1);
  }
  event->currentTime = schedulerVirtualTime;
  event->command = SCORE_EVENT_LOADSEGMENTFIFO;
  event->node = segment;

  commandBatch->append(event);

  return(0);
}


///////////////////////////////////////////////////////////////////////////////
// memXferPrimaryToCMB:
//   Transfers memory from primary memory to a physical memory segment.
//
// Parameters:
//   srcPrimMem: pointer to memory in primary memory.
//   cmbloc: location of the physical CMB.
//   start: starting address in CMB.
//   size: size of the block to transfer.
//
// Return value:
//   0 if successful; -1 if not successful.
///////////////////////////////////////////////////////////////////////////////
int memXferPrimaryToCMB(void *srcPrimMem, 
			unsigned int cmbloc, 
			unsigned int start, unsigned int size) {
  ScoreSyncEvent *event;


  // make sure there is a command batch.
  if (commandBatch == NULL) {
    cerr << "SCHEDERR: Trying to issue a command before starting a batch!" << 
      endl;
    return(-1);
  }

  // create the event.
  event = new ScoreSyncEvent();
  if (event == NULL) {
    cerr << "SCHEDERR: Insufficient memory to instantiate event!" << endl;
    return(-1);
  }
  event->currentTime = schedulerVirtualTime;
  event->command = SCORE_EVENT_MEMXFERPRIMARYTOCMB;
  event->sinkBlockLoc = cmbloc;
  event->sinkBlockStart = start;
  event->xferSize = size;

  commandBatch->append(event);

  return(0);
}


///////////////////////////////////////////////////////////////////////////////
// memXferCMBToPrimary:
//   Transfers memory from a physical memory segment to primary memory.
//
// Parameters:
//   cmbloc: location of the physical CMB.
//   start: starting address in CMB.
//   size: size of the block to transfer.
//   sinkPrimMem: pointer to memory in primary memory.
//
// Return value:
//   0 if successful; -1 if not successful.
///////////////////////////////////////////////////////////////////////////////
int memXferCMBToPrimary(unsigned int cmbloc,
			unsigned int start, unsigned int size, 
			void *sinkPrimMem) {
  ScoreSyncEvent *event;


  // make sure there is a command batch.
  if (commandBatch == NULL) {
    cerr << "SCHEDERR: Trying to issue a command before starting a batch!" << 
      endl;
    return(-1);
  }

  // create the event.
  event = new ScoreSyncEvent();
  if (event == NULL) {
    cerr << "SCHEDERR: Insufficient memory to instantiate event!" << endl;
    return(-1);
  }
  event->currentTime = schedulerVirtualTime;
  event->command = SCORE_EVENT_MEMXFERCMBTOPRIMARY;
  event->srcBlockLoc = cmbloc;
  event->srcBlockStart = start;
  event->xferSize = size;

  commandBatch->append(event);

  return(0);
}


///////////////////////////////////////////////////////////////////////////////
// memXferCMBToCMB:
//   Transfers memory from a physical memory segment to another
//     physical memory segment.
//
// Parameters:
//   srccmbloc: location of the source physical CMB.
//   srcstart: starting address in source CMB.
//   sinkcmbloc: location of the sink physical CMB.
//   sinkstart: starting address in sink CMB.
//   size: size of the block to transfer.
//
// Return value:
//   0 if successful; -1 if not successful.
///////////////////////////////////////////////////////////////////////////////
int memXferCMBToCMB(unsigned int srccmbloc,
		    unsigned int srcstart,
		    unsigned int sinkcmbloc,
		    unsigned int sinkstart, unsigned int size) {
  ScoreSyncEvent *event;


  // make sure there is a command batch.
  if (commandBatch == NULL) {
    cerr << "SCHEDERR: Trying to issue a command before starting a batch!" << 
      endl;
    return(-1);
  }

  // create the event.
  event = new ScoreSyncEvent();
  if (event == NULL) {
    cerr << "SCHEDERR: Insufficient memory to instantiate event!" << endl;
    return(-1);
  }
  event->currentTime = schedulerVirtualTime;
  event->command = SCORE_EVENT_MEMXFERCMBTOCMB;
  event->srcBlockLoc = srccmbloc;
  event->sinkBlockLoc = sinkcmbloc;
  event->srcBlockStart = srcstart;
  event->sinkBlockStart = sinkstart;
  event->xferSize = size;

  commandBatch->append(event);

  return(0);
}


///////////////////////////////////////////////////////////////////////////////
// connectStream:
//   Connect a stream between page/segment <-> page/segment.
//
// Parameters:
//   srcNode: source node.
//   srcOutputNum: source node output number.
//   sinkNode: sink node.
//   sinkInputNum: sink node input number.
//
// Return value:
//   0 if successful; -1 if not successful.
///////////////////////////////////////////////////////////////////////////////
int connectStream(ScoreGraphNode *srcNode, unsigned int srcOutputNum,
		  ScoreGraphNode *sinkNode, unsigned int sinkInputNum) {
  ScoreSyncEvent *event;


  // make sure there is a command batch.
  if (commandBatch == NULL) {
    cerr << "SCHEDERR: Trying to issue a command before starting a batch!" << 
      endl;
    return(-1);
  }

  // create the event.
  event = new ScoreSyncEvent();
  if (event == NULL) {
    cerr << "SCHEDERR: Insufficient memory to instantiate event!" << endl;
    return(-1);
  }
  event->currentTime = schedulerVirtualTime;
  event->command = SCORE_EVENT_CONNECTSTREAM;
  event->srcNode = srcNode;
  event->outputNum = srcOutputNum;
  event->sinkNode = sinkNode;
  event->inputNum = sinkInputNum;
  event->stream = srcNode->getOutput(srcOutputNum);

  commandBatch->append(event);

  return(0);
}


///////////////////////////////////////////////////////////////////////////////
// sendSimulatorRunUntil:
//   Tell simulator how far it can simulate to.
//   NOTE: This is only for the scheduler<->simulator interface.
//
// Parameters:
//   cycle: the cycle number to simulate to.
//
// Return value:
//   0 if successful; -1 if not successful.
///////////////////////////////////////////////////////////////////////////////
int sendSimulatorRunUntil(unsigned int cycle) {
  ScoreSyncEvent *event;


  // make sure the time specified is not in the past.
  if (cycle < schedulerVirtualTime) {
    cerr << "SCHEDERR: Attempting to issue a run-until command in the past! " 
	 << "(current=" << schedulerVirtualTime << ", run-until=" <<
      cycle << ")" << endl;
    return(-1);
  }

  // create the event.
  event = new ScoreSyncEvent();
  if (event == NULL) {
    cerr << "SCHEDERR: Insufficient memory to instantiate event!" << endl;
    return(-1);
  }
  schedulerVirtualTime = cycle;
  event->currentTime = schedulerVirtualTime;
  event->command = SCORE_EVENT_RUNUNTIL;

  if (batchCommandBegin() != 0) {
    return(-1);
  }
  commandBatch->append(event);
  if (batchCommandEnd() != 0) {
    return(-1);
  }

  return(0);
}


///////////////////////////////////////////////////////////////////////////////
// advanceSimulatorTime:
//   Advance simulator time.
//   NOTE: This is only for the scheduler<->simulator interface.
//
// Parameters:
//   cycle: the cycle number to advance to.
//
// Return value:
//   0 if successful; -1 if not successful.
///////////////////////////////////////////////////////////////////////////////
int advanceSimulatorTime(unsigned int cycle) {
  // make sure the time specified is not in the past.
  if (cycle < schedulerVirtualTime) {
    cerr << "SCHEDERR: Attempting to advance time into the past! " <<
      "(current=" << schedulerVirtualTime << ", advance-to=" <<
      cycle << ")" << endl;
    return(-1);
  }

  schedulerVirtualTime = cycle;

  return(0);
}


///////////////////////////////////////////////////////////////////////////////
// advanceSimulatorTimeOffset:
//   Advance simulator time by an offset.
//   NOTE: This is only for the scheduler<->simulator interface.
//
// Parameters:
//   cycle: the cycle number to offset by.
//
// Return value:
//   0 if successful; -1 if not successful.
///////////////////////////////////////////////////////////////////////////////
int advanceSimulatorTimeOffset(unsigned int cycle) {
  schedulerVirtualTime = schedulerVirtualTime + cycle;

  return(0);
}


///////////////////////////////////////////////////////////////////////////////
// requestNextTimeslice:
//   Sends a request for the next timeslice to the simulator.
//   NOTE: This is only for the scheduler<->simulator interface.
//
// Parameters: None.
//
// Return value:
//   0 if successful; -1 if not successful.
///////////////////////////////////////////////////////////////////////////////
int requestNextTimeslice() {
  unsigned int nextTimeslice;


  // calculate when the next timeslice should happen.
  nextTimeslice = schedulerVirtualTime + SCORE_TIMESLICE;

#if 1
  nextTimeslice = nextTimeslice - (nextTimeslice%SCORE_TIMESLICE);
#endif

  // send the timeslice request.
  return(sendSimulatorRunUntil(nextTimeslice));
}


///////////////////////////////////////////////////////////////////////////////
// waitForNextTimeslice:
//   Waits for the next timeslice event from the simulator.
//   NOTE: This is only for the scheduler<->simulator interface.
//
// Parameters: None.
//
// Return value:
//   0 if successful; -1 if not successful.
///////////////////////////////////////////////////////////////////////////////
int waitForNextTimeslice() {
  ScoreSyncEvent *nextEvent = 0;
  STREAM_READ_RT(fromSimulator,nextEvent);

  // make sure what we got was the timeslice event.
  if (nextEvent->command != SCORE_EVENT_TIMESLICE) {
    cerr << "SCHEDERR: Got an event other than timeslice while waiting for " <<
      "timeslice! (event=" << eventNameMap[nextEvent->command] << ")" << endl;
    return(-1);
  }

  // advance virtual time to match.
  schedulerVirtualTime = nextEvent->currentTime;

  delete(nextEvent);

  return(0);
}


