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
// $Revision: 1.15 $
//
//////////////////////////////////////////////////////////////////////////////

#include "ScoreSyncEvent.h"
#include "ScoreConfig.h"


// maps events to string names.
char *eventNameMap[] = {
  "SCORE_EVENT_RUNUNTIL",
  "SCORE_EVENT_GETARRAYSTATUS",
  "SCORE_EVENT_STOPPAGE",
  "SCORE_EVENT_STARTPAGE",
  "SCORE_EVENT_STOPSEGMENT",
  "SCORE_EVENT_STARTSEGMENT",
  "SCORE_EVENT_DUMPPAGESTATE",
  "SCORE_EVENT_DUMPPAGEFIFO",
  "SCORE_EVENT_LOADPAGECONFIG",
  "SCORE_EVENT_LOADPAGESTATE",
  "SCORE_EVENT_LOADPAGEFIFO",
  "SCORE_EVENT_GETSEGMENTPOINTERS",
  "SCORE_EVENT_DUMPSEGMENTFIFO",
  "SCORE_EVENT_SETSEGMENTCONFIGPOINTERS",
  "SCORE_EVENT_CHANGESEGMENTMODE",
  "SCORE_EVENT_CHANGESEGMENTTRAANDPBOANDMAX",
  "SCORE_EVENT_RESETSEGMENTDONEFLAG",
  "SCORE_EVENT_LOADSEGMENTFIFO",
  "SCORE_EVENT_MEMXFERPRIMARYTOCMB",
  "SCORE_EVENT_MEMXFERCMBTOPRIMARY",
  "SCORE_EVENT_MEMXFERCMBTOCMB",
  "SCORE_EVENT_CONNECTSTREAM",
  "SCORE_EVENT_ACK",
  "SCORE_EVENT_TIMESLICE",
  "SCORE_EVENT_RETURNARRAYSTATUS",
  "SCORE_EVENT_RETURNSEGMENTPOINTERS",
  "SCORE_EVENT_IDLE",
};


///////////////////////////////////////////////////////////////////////////////
// ScoreSyncEvent::ScoreSyncEvent:
//   Constructor for ScoreSyncEvent.
//   Initializes all internal structures.
//
// Parameters: None.
//
// Return value: None.
///////////////////////////////////////////////////////////////////////////////
ScoreSyncEvent::ScoreSyncEvent() {
  currentTime = 0;
  command = 0;
  node = NULL;
  srcNode = NULL;
  sinkNode = NULL;
  outputNum = 0;
  inputNum = 0;
  stream = 0;
  srcBlockLoc = 0;
  sinkBlockLoc = 0;
  srcBlockStart = 0;
  sinkBlockStart = 0;
  xferSize = 0;
  cpStatus = NULL;
  cmbStatus = NULL;
  cpMask = NULL;
  cmbMask = NULL;
  readAddr = 0;
  writeAddr = 0;
}


///////////////////////////////////////////////////////////////////////////////
// ScoreSyncEvent::~ScoreSyncEvent:
//   Destructor for ScoreSyncEvent.
//   Cleans up all internal structures.
//
// Parameters: None.
//
// Return value: None.
///////////////////////////////////////////////////////////////////////////////
ScoreSyncEvent::~ScoreSyncEvent() {
  if (cpStatus != NULL) {
    delete(cpStatus);
  }
  if (cmbStatus != NULL) {
    delete(cmbStatus);
  }
  if (cpMask != NULL) {
    delete(cpMask);
  }
  if (cmbMask != NULL) {
    delete(cmbMask);
  }
}

void ScoreSyncEvent::print(FILE *f) {
  assert((command >= 0) && (command < NumberOf_SCORE_EVENT));
  fprintf(f, "%s [%u]\n", eventNameMap[command], currentTime);
}

