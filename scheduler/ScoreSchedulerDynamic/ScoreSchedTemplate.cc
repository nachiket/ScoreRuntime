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

// This file contains a little code to emit template for
// schedc and schedgenerators.

#include "ScoreGraphNode.h"
#include "ScoreStream.h"
#include "ScorePage.h"
#include "ScoreSegment.h"

#include "ScoreSimulator.h"
#include "ScoreSchedulerDynamic.h"

#include "ScoreFeedbackGraph.h"

using std::map;

#if GET_FEEDBACK

void makeSchedTemplate(FILE *f, ScoreFeedbackGraph *feedbackGraph)
{
  // make:
  //  array {
  //    CPs: 2;
  //    CMBs: 5;
  //    CMB Partitions:
  //      //L0 (L1, ...) 
  //      B0 (B0, B1, B2) :
  //      B1 (B3, B4, B5) :
  //      null (B6, B7);   
  //  }
  
  unsigned int cmbSize = physicalSegmentSize;
  
  fprintf(f, "array {\n");
  fprintf(f, "   CPs: %d;\n", numPhysicalPages);
  fprintf(f, "   CMBs: %d;   // CMB size = %d\n", numPhysicalSegments,
	  physicalSegmentSize);
  fprintf(f, "   CMB Partitions:\n");
  fprintf(f, "   //L0 (L1, ...)\n");
  
  unsigned int numLevel0InCMB = 
    cmbSize / SCORE_SEGMENTTABLE_LEVEL0SIZE;
  unsigned int sizeLevel0CruftInCMB = 
    cmbSize % SCORE_SEGMENTTABLE_LEVEL0SIZE;
  unsigned int numLevel1InLevel0 =
    SCORE_SEGMENTTABLE_LEVEL0SIZE / SCORE_SEGMENTTABLE_LEVEL1SIZE;
//    unsigned int sizeLevel1CruftInLevel0 =
//      SCORE_SEGMENTTABLE_LEVEL0SIZE % SCORE_SEGMENTTABLE_LEVEL1SIZE;
  unsigned int numLevel1InLevel0CruftInCMB =
    sizeLevel0CruftInCMB / SCORE_SEGMENTTABLE_LEVEL1SIZE;
//    unsigned int sizeLevel1CruftInLevel0CruftInCMB =
//      sizeLevel0CruftInCMB % SCORE_SEGMENTTABLE_LEVEL1SIZE;

  unsigned int l1_base = 0;

  for (unsigned int l0_index = 0; l0_index < numLevel0InCMB; l0_index++) {
    fprintf(f, "   B%d (", l0_index);
    unsigned int l1_index;
    for (l1_index = 0; l1_index < numLevel1InLevel0; l1_index++) {
      if (l1_index == 0)
	fprintf(f, "B%d", l1_base + l1_index);
      else 
	fprintf(f, ", B%d", l1_base + l1_index);
    }
    l1_base += l1_index;
    fprintf(f, ") ");

    if ((numLevel1InLevel0CruftInCMB > 0) ||
	(l0_index < numLevel0InCMB - 1)) {
      fprintf(f, ":\n");
    }
  }

  if(numLevel1InLevel0CruftInCMB > 0) {
    fprintf(f, "   null (");
    for (unsigned int l1_index = 0; l1_index < numLevel1InLevel0CruftInCMB;
	 l1_index ++) {
      if (l1_index == 0)
	fprintf(f, "B%d", l1_base + l1_index);
      else 
	fprintf(f, ", B%d", l1_base + l1_index);      
    }

    fprintf(f, ");\n");
  }

  fprintf(f, "}\n\n");

  
  // (2)--------------------
  //  design {
  //    visual: "design.vcg" ;
  //    0: page (1, 2) "mux" : 0.output0 => 1.input0, 0.output1 => 2.input0 ;
  //    1: page (1, 1) "mult1" : 1.output0 => 3.input0 ;
  //    2: page (1, 1) "mult2" : 2.output0 => 3.input1 ;
  //    3: page (2, 1) "add" : 3.output0 => cpu.input0, cpu.output0 =>0.input0;
  //  }

  fprintf(f, "design {\n");
  fprintf(f, "   visual: \"something\";\n");
  feedbackGraph->outputSchedNodes(f);
  fprintf(f, "}\n");
}


void ScoreFeedbackGraph::outputSchedNodes(FILE *f)
{
  // force _cpuInputs and _cpuOutputs counters back to zero
  ScoreFeedbackGraphNode::resetCPUio(); 

  map<size_t, ScoreFeedbackGraphNode*>::const_iterator it;
  for (it = _nodes.begin(); it != _nodes.end(); it ++) {
    it->second->outputSchedNodes(f);
  }
}

void ScoreFeedbackGraphNode::outputSchedNodes(FILE *f)
{
  assert(_kind != NODE_TYPE_ERROR);
  fprintf(f, "   %d: %s (%d, %d) ", _nodeTag,
	  (_kind == NODE_TYPE_PAGE) ? "page" : "segment", 
	  _numInputs, _numOutputs);
  if (_name) {
    if (strlen(_name) > 0) {
      fprintf(f, "\"%s\"", _name);
    }
  }

  fprintf(f, " : ");

  // (1) get all outputs
  for (int i = 0; i < _scoreNode->getOutputs(); i ++) {
    SCORE_STREAM s = _scoreNode->getOutput(i);
    fprintf(f, "%d.output%d =", _nodeTag, i);
    if (s->get_depth_hint() > 0)
      fprintf(f, "%d=> ", s->get_depth_hint());
    else 
      fprintf(f, "> ");
    switch(s->snkFunc) {
    case STREAM_OPERATOR_TYPE:
      fprintf(f, "cpu.input%d ", _cpuInputs++);
      break;
    case STREAM_PAGE_TYPE:
    case STREAM_SEGMENT_TYPE:
      fprintf(f, "%d.input%d ", s->sink->feedbackNode->getTag(), s->snkNum);
      break;
    default:
      assert(0);
    }
    if (i < _scoreNode->getOutputs() - 1)
      fprintf(f, ", ");
  }

  // (2) get inputs from CPU only
  for (int i = 0; i < _scoreNode->getInputs(); i ++) {
    SCORE_STREAM s = _scoreNode->getInput(i);
    if (s->srcFunc == STREAM_OPERATOR_TYPE) {
      fprintf(f, ", cpu.output%d =", _cpuOutputs ++);
      if (s->get_depth_hint() > 0) 
	fprintf(f, "%d=", s->get_depth_hint());
      fprintf(f, "> %d.input%d", _nodeTag, i);
    }
  }

  fprintf(f, ";\n");
}

#endif
