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
// SCORE ArrayPhysicalStatus
// $Revision: 1.17 $
//
//////////////////////////////////////////////////////////////////////////////

#include "ScoreArrayPhysicalStatus.h"
#include "ScoreSegment.h"
#include "ScoreVisualization.h"
#include "ScoreConfig.h"


ScoreArrayPhysicalStatus::ScoreArrayPhysicalStatus(int numIns, int numOuts) {

  isDone = 0;
  isStalled = 0;
  oldStallCount = 0;
  stallCount = 0;
  consecutiveStallCount = 0;
  isFaulted = 0;
  faultedAddr = 0;
  numOfInputs = numIns;
  numOfOutputs = numOuts;
  runCycle = 0;
  currentVisualizationEvent = VISUALIZATION_EVENT_IDLE;

  tag = (size_t)(-1);
}

void ScoreArrayPhysicalStatus::update(ScoreGraphNode *node, int status) {

  tag = node->uniqTag;

  isDone = (char)(status == 0);
  if (isDone) {
    runCycle = IDLE;
  } 
  oldStallCount = stallCount;
  stallCount = node->getStall();
  isStalled = (char)(stallCount - oldStallCount);

  if (isStalled) {
    consecutiveStallCount++;
  } else {
    consecutiveStallCount = 0;
  }

  if (node->isSegment()) {
    ScoreSegment *currentSegment = (ScoreSegment *) node;

    if (currentSegment->sim_isFaulted) {
      isFaulted = 1;
      faultedAddr = currentSegment->sim_faultedAddr;

      if (currentSegment->mode != SCORE_CMB_SEQSRCSINK) {
        runCycle = IDLE;
      }
    } else {
      isFaulted = 0;
      faultedAddr = 0;
    }
  }
}
