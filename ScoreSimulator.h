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
// $Revision: 1.32 $
//
//////////////////////////////////////////////////////////////////////////////


#ifndef _ScoreSimulator_H

#define _ScoreSimulator_H

#include <iostream>
#include "LEDA/core/list.h"
#include "ScoreRuntime.h"
#include "ScoreStream.h"
#include "ScoreSegment.h"
#include "ScoreSyncEvent.h"
#include "ScoreGraphNode.h"
#include "ScoreArrayPhysicalStatus.h"
#include "ScoreThreadCounter.h"
#include "ScoreAccountingReconfigCmds.h"


#define RUNNABLE                                       -1
#define IDLE                                            0
#define DONE                                            1


// this defines how long a CP/CMB must have been stalled before it is moved
// from the runnable list to the stalled queue.
#define SIM_RUNNABLETOSTALLED_THRESHOLD       20000

extern unsigned int numPhysicalPages;
extern unsigned int numPhysicalSegments;
extern unsigned int physicalSegmentSize;

class ScoreSimulator {

 public:
  ScoreSimulator(ScoreStream *, ScoreStream *);
  ~ScoreSimulator();
  void run();
  void printStats();

  ScoreAccountingReconfigCmds reconfigAcct;

 private:
  ScoreThreadCounter *threadCounter;
  unsigned int *simCycle;
  unsigned int currTimeSlice;
  unsigned outputTokenType;
  ScoreStream *inputStream;
  ScoreStream *outputStream;
  ScorePage **saArray;
  ScoreSegment **cmbArray;
  ScoreArrayCPStatus *saArrayStatus;
  ScoreArrayCMBStatus *cmbArrayStatus;
  ScoreArrayPhysicalStatus **saArrayPhysicalStatus;
  ScoreArrayPhysicalStatus **cmbArrayPhysicalStatus;
  int needToken;
  int *lastVisualizationEventCP;
  int *currentVisualizationEventCP;
  int *lastVisualizationEventCMB;
  int *currentVisualizationEventCMB;

  int statsOfSchedToSimCommands[NumberOf_SCORE_EVENT];

  list<ScoreGraphNode *> runnableNodeList;
#if 0
  p_queue<ScoreGraphNode *, ScoreGraphNode *> *stalledNodeQueue;
#endif
  list<ScoreGraphNode *> checkNodeList;

  void issue(ScoreSyncEvent *);
  void runUntil();
  void getArrayStatus(char *cpMask, char *cmbMask);
  void stopPage(int);
  void startPage(int);
  void stopSegment(int);
  void startSegment(int);
  void dumpPageState(int, int);
  void dumpPageFIFO(int, int);
  void loadPageConfig(int, int, ScorePage *);
  void loadPageState(int, int);
  void loadPageFIFO(int, int);
  void getSegmentPointers(int);
  void dumpSegmentFIFO(int, int);
  void setSegmentConfigPointers(int, ScoreSegment *);
  void changeSegmentMode(int, ScoreSegment *);
  void changeSegmentTRAandPBOandMAX(int, ScoreSegment *);
  void resetSegmentDoneFlag(int, ScoreSegment *);
  void loadSegmentFIFO(int, int);
  void memXferCMBToCMB(int, int, unsigned int);
  void memXferCMBToPrimary(int, unsigned int);
  void memXferPrimaryToCMB(int, unsigned int);
  void connectStream(ScoreGraphNode *, ScoreGraphNode *);
};

#endif
