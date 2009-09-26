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
// $Revision: 1.8 $
//
//////////////////////////////////////////////////////////////////////////////


#ifndef _ScoreArrayPhysicalStatus_H

#define _ScoreArrayPhysicalStatus_H

#include <iostream>
#include "ScoreStream.h"
#include "ScoreGraphNode.h"

#define IDLE 0

class ScoreArrayPhysicalStatus {
 public:
  ScoreArrayPhysicalStatus(int, int);
  int getRunCycle() {return(runCycle);}
  void setRunCycle(int cycle) {runCycle = cycle;}
  void update(ScoreGraphNode *, int);
  char isDone;
  char isStalled;
  unsigned int oldStallCount;
  unsigned int stallCount;
  unsigned int consecutiveStallCount;
  char isFaulted;
  unsigned int faultedAddr;
  int numOfInputs;
  int numOfOutputs;
  int currentVisualizationEvent;

  size_t tag;

 private:
  int runCycle;

  
};




#endif
