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
// $Revision: 1.17 $
//
//////////////////////////////////////////////////////////////////////////////
#ifndef _ScoreSyncEvent_H

#define _ScoreSyncEvent_H

#include "ScoreGraphNode.h"
#include "ScoreStream.h"
#include "ScoreSegmentTable.h"
#include "ScoreHardwareAPI.h"


#define NumberOf_SCORE_EVENT                     26

// defines the types of events sent from scheduler->simulator.
#define SCORE_EVENT_RUNUNTIL                      0
#define SCORE_EVENT_GETARRAYSTATUS                1
#define SCORE_EVENT_STOPPAGE                      2
#define SCORE_EVENT_STARTPAGE                     3
#define SCORE_EVENT_STOPSEGMENT                   4
#define SCORE_EVENT_STARTSEGMENT                  5
#define SCORE_EVENT_DUMPPAGESTATE                 6
#define SCORE_EVENT_DUMPPAGEFIFO                  7
#define SCORE_EVENT_LOADPAGECONFIG                8
#define SCORE_EVENT_LOADPAGESTATE                 9
#define SCORE_EVENT_LOADPAGEFIFO                 10
#define SCORE_EVENT_GETSEGMENTPOINTERS           11
#define SCORE_EVENT_DUMPSEGMENTFIFO              12
#define SCORE_EVENT_SETSEGMENTCONFIGPOINTERS     13
#define SCORE_EVENT_CHANGESEGMENTMODE            14
#define SCORE_EVENT_CHANGESEGMENTTRAANDPBOANDMAX 15
#define SCORE_EVENT_RESETSEGMENTDONEFLAG         16
#define SCORE_EVENT_LOADSEGMENTFIFO              17
#define SCORE_EVENT_MEMXFERPRIMARYTOCMB          18
#define SCORE_EVENT_MEMXFERCMBTOPRIMARY          19
#define SCORE_EVENT_MEMXFERCMBTOCMB              20
#define SCORE_EVENT_CONNECTSTREAM                21


// defines the types of events sent from simulator->scheduler.
#define SCORE_EVENT_ACK                          21
#define SCORE_EVENT_TIMESLICE                    22
#define SCORE_EVENT_RETURNARRAYSTATUS            23
#define SCORE_EVENT_RETURNSEGMENTPOINTERS        24
#define SCORE_EVENT_IDLE                         25

// maps events to string names.
extern char *eventNameMap[];


// ScoreSyncEvent: used to synchronize timed events.
class ScoreSyncEvent {
 public:
  ScoreSyncEvent();
  ~ScoreSyncEvent();

  void print(FILE *f);

  unsigned int currentTime;
  int command;
  ScoreGraphNode *node;
  ScoreGraphNode *srcNode;
  ScoreGraphNode *sinkNode;
  unsigned int outputNum;
  unsigned int inputNum;
  SCORE_STREAM stream;
  unsigned int srcBlockLoc, sinkBlockLoc;
  unsigned int srcBlockStart, sinkBlockStart;
  unsigned int xferSize;

  ScoreArrayCPStatus *cpStatus;
  ScoreArrayCMBStatus *cmbStatus;
  char *cpMask, *cmbMask;
  unsigned int readAddr, writeAddr;

 private:
};

#endif
