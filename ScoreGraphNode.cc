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
// $Revision: 1.29 $
//
//////////////////////////////////////////////////////////////////////////////

#include "ScorePage.h"
#include "ScoreSegment.h"
#include "ScoreStream.h"
#include "ScoreGraphNode.h"
#include "ScoreConfig.h"


ScoreGraphNode::ScoreGraphNode() {
  inputs = 0;
  outputs = 0;
  sim_unstallTime = 0;
  sim_unstallTimeIsInfinity = 0;
  sched_isDone = 0;
  sched_isResident = 0;
  sched_isScheduled = 0;
  sched_residentLoc = 0;
  sched_parentProcess = NULL;
  sched_parentCluster = NULL;
  sched_parentOperator = NULL;
  sched_dependentOnInputBuffer = NULL;
  sched_dependentOnOutputBuffer = NULL;
  sched_inputConsumption = NULL;
  sched_outputProduction = NULL;
  sched_inputConsumptionOffset = NULL;
  sched_outputProductionOffset = NULL;
  sched_lastKnownInputFIFONumTokens = NULL;
  sched_expectedInputConsumption = NULL;
  sched_expectedOutputProduction = NULL;
  sim_runnableNodeListItem = NULL; // no such thing as nil_item
#if 0
  sim_stalledNodeQueueItem = nil_item;
#endif
  sim_checkNodeListItem = NULL; // nil+item
  sched_potentiallyDidNotFireLastResident = 0;

#ifdef RANDOM_SCHEDULER
  // this is to indicate that this node was never scheduled before
  // the reason, why -1 is not appropriate is that on the first
  // timeslice (currentTimeslice == 0), the value -1 (=currentTimeslice-1)
  // indicates that this node was scheduled on the previous timeslice
  // and thus must be removed from the array (which is not accurate).
  sched_lastTimesliceScheduled = (unsigned int)(-2);
#endif

#if GET_FEEDBACK
  feedbackNode = 0;
  stat_consumption = 0;
  stat_production = 0;
  stat_fireCount = 0;
#endif
} 


ScoreGraphNode::~ScoreGraphNode() {
  int i;
  for (i = 0; i < inputs; i++) {
    SCORE_STREAM currentStream = in[i];

    if(errno) {
      errno=0;
      perror("Crazy mofo");
    }
    
    if (currentStream != NULL) {
    cout << "Who are you??" << endl;
      STREAM_FREE_HW(currentStream);
    }
  }
  for (i = 0; i < outputs; i++) {
    SCORE_STREAM currentStream = out[i];
    
    if (currentStream != NULL) {
    cout << "And, who are you??" << endl;
      STREAM_CLOSE_HW(currentStream);
    }
  }

  delete(in);
  delete(out);
  delete(sched_in);
  delete(sched_out);
  delete(in_types);
  delete(out_types);
  delete(sched_in_types);
  delete(sched_out_types);
  delete(sched_dependentOnInputBuffer);
  delete(sched_dependentOnOutputBuffer);
  delete(sched_inputConsumption);
  delete(sched_outputProduction);
  delete(sched_inputConsumptionOffset);
  delete(sched_outputProductionOffset);
  delete(sched_lastKnownInputFIFONumTokens);
  delete(sched_expectedInputConsumption);
  delete(sched_expectedOutputProduction);

#if GET_FEEDBACK
  if (stat_consumption)
    delete stat_consumption;
  if (stat_production)
    delete stat_production;  
#endif
}


void ScoreGraphNode::recalculateUnstallTime() {
  int i;
  ScoreIOMaskType currentConsumes = 0;
  unsigned long long mostFutureInputTime = 0;
  char missingInputs = 0;
  

  if (isPage()) {
    ScorePage *currentPage = (ScorePage *) this;
    int currentState = currentPage->get_state();
  
    currentConsumes = currentPage->inputs_consumed(currentState);
  } else if (isSegment()) {
    ScoreSegment *currentSegment = (ScoreSegment *) this;

    currentConsumes = currentSegment->sim_segmentInputMask;
  }

  for (i = 0; i < inputs; i++) {
    ScoreStream *currentInput = in[i];
    char needInput = (currentConsumes>>i)&1;

    if (needInput) {
      if (currentInput->stream_data_any()) {
	unsigned long long currentHeadTokenTime = 
	  currentInput->stream_head_futuretime();
	
	if (currentHeadTokenTime > mostFutureInputTime) {
	  mostFutureInputTime = currentHeadTokenTime;
	}
      } else {
	  missingInputs = 1;
	  break;
      }
    }
  }

  if (missingInputs) {
    sim_unstallTime = 0;
    sim_unstallTimeIsInfinity = 1;
  } else {
    sim_unstallTime = mostFutureInputTime;
    sim_unstallTimeIsInfinity = 0;
  }
}


char ScoreGraphNode::isStalledOnInput() {
  int i;
  ScoreIOMaskType currentConsumes = 0;
  

  if (isPage()) {
    ScorePage *currentPage = (ScorePage *) this;
    int currentState = currentPage->get_state();
  
    currentConsumes = currentPage->inputs_consumed(currentState);
  } else if (isSegment()) {
    ScoreSegment *currentSegment = (ScoreSegment *) this;

    currentConsumes = currentSegment->sim_segmentInputMask;
  }

  for (i = 0; i < inputs; i++) {
    ScoreStream *currentInput = in[i];
    char needInput = (currentConsumes>>i)&1;

    if (needInput) {
      if (!(currentInput->stream_data())) {
	return(1);
      }
    }
  }

  return(0);
}


void ScoreGraphNode::syncSchedToReal() {
  int i;

  for (i = 0; i < inputs; i++) {
    in[i] = sched_in[i];
    in_types[i] = sched_in_types[i];
  }
  for (i = 0; i < outputs; i++) {
    out[i] = sched_out[i];
    out_types[i] = sched_out_types[i];
  }
}


// required by LEDA for use with lists/etc.

// provides comparison operation between ScoreGraphNodes.
// NOTE: Right now, we only say 2 nodes are equal if their pointers
//       are equal. Otherwise, less than/greater than is determined
//       simply by their pointer values.
int compare(ScoreGraphNode * const & left, ScoreGraphNode * const & right) {
  if (left == right) {
    return(0);
  } else if (left < right) {
    return(-1);
  } else {
    return(1);
  }
}



void ScoreGraphNode::print(FILE *f, bool printShort)
{
  static char *node_names[] = {"Page", "Segment", "Stitch", "Operator" };
  int node_names_index = -1;
  
  if (_isPage) {
    node_names_index = 0;
  } else if (_isOperator) {
    node_names_index = 3;
  } else if (_isSegment) {
    ScoreSegment *seg = (ScoreSegment*) this;
    if (seg->sched_isStitch) {
      node_names_index = 2;
    } else {
      node_names_index = 1;
    }
  }
  
  fprintf(f, "%s %d [%d] %s:\n", node_names[node_names_index],
	  uniqTag, (long) this,
	  _isPage ? ((ScorePage*)this)->getSource() : "");
  
  if (!printShort) {
    
    for (int i = 0; i < getInputs(); i ++) {
      fprintf(f, "INPUT%d: ", i);
      ScoreGraphNode *src = getSchedInput(i)->src;
      if (src) {
	src->print(f, true);
      } else {
	fprintf(f, "NULL\n");
      }
    }

    for (int i = 0; i < getOutputs(); i ++) {
      fprintf(f, "OUTPUT%d: ", i);
      ScoreGraphNode *sink = getSchedOutput(i)->sink;
      if (sink) {
	sink->print(f, true);
      } else {
	fprintf(f, "NULL\n");
      }
    }

  }
}
