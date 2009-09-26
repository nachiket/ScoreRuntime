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
// $Revision: 1.22 $
//
//////////////////////////////////////////////////////////////////////////////
#ifndef _ScoreHardwareAPI_H

#define _ScoreHardwareAPI_H

#include "ScorePage.h"
#include "ScoreSegment.h"
#include "ScoreGraphNode.h"
#include "ScoreSegmentTable.h"
#include "ScoreStream.h"


// extern declarations.
extern unsigned int schedulerVirtualTime;


// ScoreArrayCPStatus: stores current status information about a physical
//   array CP.
class ScoreArrayCPStatus {
 public:
  // clears the information in the status.
  void clearStatus() {
    isDone = 0;
    isStalled = 0;
    stallCount = 0;
    // don't actually NULLify here since we don't want to do delete in the
    // critical path!
    // NOTE: Will get deleted next time getArrayStatus().
#if 0
    inputConsumption = NULL;
    outputProduction = NULL;
#endif
    currentState = -1;
    emptyInputs = 0;
    fullOutputs = 0;
    // don't actually NULLify here since we don't want to do delete in the
    // critical path!
    // NOTE: Will get deleted next time getArrayStatus().
#if 0
    inputFIFONumTokens = NULL;
#endif
  }

  char isDone;
  char isStalled;
  unsigned int stallCount;
  unsigned int *inputConsumption;
  unsigned int *outputProduction;
  int currentState;
  ScoreIOMaskType emptyInputs;
  ScoreIOMaskType fullOutputs;
  unsigned int *inputFIFONumTokens;

 private:
};


// ScoreArrayCMBStatus: stores current status information about a physical
//   array CMB.
class ScoreArrayCMBStatus {
 public:
  // clears the information in the status.
  void clearStatus() {
    isDone = 0;
    isStalled = 0;
    isFaulted = 0;
    stallCount = 0;
    faultedAddr = 0;
    readCount = 0;
    writeCount = 0;
    emptyInputs = 0;
    fullOutputs = 0;
    // don't actually NULLify here since we don't want to do delete in the
    // critical path!
    // NOTE: Will get deleted next time getArrayStatus().
#if 0
    inputFIFONumTokens = NULL;
#endif
  }



  char isDone;
  char isStalled;
  char isFaulted;
  unsigned int stallCount; /* cycles the segment was not actively servicing
                              a request. */
  unsigned int faultedAddr;
  unsigned int readAddr;
  unsigned int writeAddr;
  unsigned int readCount;
  unsigned int writeCount;
  ScoreIOMaskType emptyInputs;
  ScoreIOMaskType fullOutputs;
  unsigned int *inputFIFONumTokens;

 private:
};


int getArrayInfo(unsigned int *numPhysicalCP, unsigned int *numPhysicalCMB,
		 unsigned int *cmbSize);
int getArrayStatus(ScoreArrayCPStatus *cpStatus,
		   ScoreArrayCMBStatus *cmbStatus,
		   char *cpMask, char *cmbMask);

int batchCommandBegin();
int batchCommandEnd();
int stopPage(ScorePage *page);
int startPage(ScorePage *page);
int stopSegment(ScoreSegment *segment);
int startSegment(ScoreSegment *segment);
int dumpPageState(ScorePage *page);
int dumpPageFIFO(ScorePage *page);
int loadPageConfig(ScorePage *page);
int loadPageState(ScorePage *page);
int loadPageFIFO(ScorePage *page);
int getSegmentPointers(ScoreSegment *segment);
int dumpSegmentFIFO(ScoreSegment *segment);
int setSegmentConfigPointers(ScoreSegment *segment);
int changeSegmentMode(ScoreSegment *segment);
int changeSegmentTRAandPBOandMAX(ScoreSegment *segment);
int resetSegmentDoneFlag(ScoreSegment *segment);
int loadSegmentFIFO(ScoreSegment *segment);
int memXferPrimaryToCMB(void *srcPrimMem, 
			unsigned int cmbloc, 
			unsigned int start, unsigned int size);
int memXferCMBToPrimary(unsigned int cmbloc,
			unsigned int start, unsigned int size, 
			void *sinkPrimMem);
int memXferCMBToCMB(unsigned int srccmbloc,
		    unsigned int srcstart,
		    unsigned int sinkcmbloc,
		    unsigned int sinkstart, unsigned int size);
int connectStream(ScoreGraphNode *srcNode, unsigned int srcOutputNum,
		  ScoreGraphNode *sinkNode, unsigned int sinkInputNum);

int sendSimulatorRunUntil(unsigned int cycle);
int advanceSimulatorTime(unsigned int cycle);
int advanceSimulatorTimeOffset(unsigned int cycle);
int requestNextTimeslice();
int waitForNextTimeslice();

#endif






