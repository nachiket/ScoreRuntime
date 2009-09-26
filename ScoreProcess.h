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
// $Revision: 1.12 $
//
//////////////////////////////////////////////////////////////////////////////
#ifndef _ScoreProcess_H

#define _ScoreProcess_H

#include <unistd.h>
#include "ScoreOperatorInstance.h"
#include "ScorePage.h"
#include "ScoreSegmentStitch.h"
#include "ScoreCluster.h"
#include "ScoreSegment.h"
#include "ScoreCustomList.h"
#include "ScoreFeedbackGraph.h"

// ScoreProcess: stores information about operators/clusters/pages associated
//   with a particular user process.
class ScoreProcess {
 public:
  ScoreProcess(pid_t newPid);
 private:
  void ScoreProcessInit(pid_t newPid);
 public:
#if GET_FEEDBACK
  ScoreProcess(pid_t newPid, char *sharedObject);
  ScoreFeedbackGraph *feedbackGraph;
#endif 
  ~ScoreProcess();

  void addOperator(ScoreOperatorInstance *newOperator);
  void addCluster(ScoreCluster *newCluster);


  // this is the process id of the process.
  pid_t pid;

  // this is the list of operators in the process.
  ScoreCustomList<ScoreOperatorInstance *> *operatorList;

  // this is the list of pages/segments in the process.
  ScoreCustomList<ScoreGraphNode *> *nodeList;

  // this is the list of clusters in the process.
  ScoreCustomList<ScoreCluster *> *clusterList;

  // stores the list of streams which go to/from the processor operators.
  ScoreCustomList<ScoreStream *> *processorIStreamList;
  ScoreCustomList<ScoreStream *> *processorOStreamList;

  // stores the list of stitch buffers.
  ScoreCustomList<ScoreSegmentStitch *> *stitchBufferList;

  // this stores the number of pages in the process.
  unsigned int numPages;
  unsigned int numSegments;

  // this is the number of pages which have not consumed or produced ANY
  // tokens in their last timeslice that they were resident on the array.
  unsigned int numPotentiallyNonFiringPages;
  unsigned int numPotentiallyNonFiringSegments;

  // number of consecutive deadlocks detected. this is to provide some
  // hysterisis against prematurely killing a running design on deadlock
  // based on stale data.
  unsigned int numConsecutiveDeadLocks;

#if GET_FEEDBACK
  // directory name where user program resides
  char *userDir;

  // each node within the process must have a unique tag, which will be
  // used later to match it with its statistics from the feedback file.
  // the following counter contains next assignable unique tag.
  size_t nextTag;

  // use getNextTag() to obtain the next tag.
  size_t getNextTag() { return nextTag++; }

#endif
 private:
};

#endif

