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
// $Revision: 1.33 $
//
//////////////////////////////////////////////////////////////////////////////

#include <unistd.h>
#include "ScoreSegmentStitch.h"
#include "ScoreProcess.h"
#include "ScoreCustomList.h"
#include "ScoreConfig.h"

#if GET_FEEDBACK
#include <assert.h>
#include <string.h>
extern ScoreFeedbackMode gFeedbackMode;
#include "ScoreFeedbackGraph.h"
ScoreFeedbackGraph *gFeedbackObj;
#endif

///////////////////////////////////////////////////////////////////////////////
// ScoreProcess::ScoreProcess:
//   Constructor for ScoreProcess.
//   Initializes all internal structures.
//
// Parameters:
//   newPid: the process id for this process.
//
// Return value: None.
///////////////////////////////////////////////////////////////////////////////
ScoreProcess::ScoreProcess(pid_t newPid) {
  ScoreProcessInit(newPid);
}

///////////////////////////////////////////////////////////////////////////////
// ScoreProcess::ScoreProcessInit:
//   Constructor for ScoreProcess.
//   Initializes all internal structures.
//
// Parameters:
//   newPid: the process id for this process.
//
// Return value: None.
///////////////////////////////////////////////////////////////////////////////
void ScoreProcess::ScoreProcessInit(pid_t newPid) {
  // set the process id.
  pid = newPid;

  operatorList = 
    new ScoreCustomList<ScoreOperatorInstance *>(SCORE_PROCESSOPERATORLIST_BOUND);
  nodeList =
    new ScoreCustomList<ScoreGraphNode *>(SCORE_PROCESSNODELIST_BOUND);
  processorIStreamList =
    new ScoreCustomList<ScoreStream *>(SCORE_PROCESSPROCESSORISTREAMLIST_BOUND);
  processorOStreamList =
    new ScoreCustomList<ScoreStream *>(SCORE_PROCESSPROCESSOROSTREAMLIST_BOUND);
  clusterList =
    new ScoreCustomList<ScoreCluster *>(SCORE_PROCESSCLUSTERLIST_BOUND);
  stitchBufferList =
    new ScoreCustomList<ScoreSegmentStitch *>(SCORE_PROCESSSTITCHBUFFERLIST_BOUND);

  numPages = 0;
  numSegments = 0;
  numPotentiallyNonFiringPages = 0;
  numPotentiallyNonFiringSegments = 0;
  numConsecutiveDeadLocks = 0;
}

#if GET_FEEDBACK
///////////////////////////////////////////////////////////////////////////////
// ScoreProcess::ScoreProcess:
//   Constructor for ScoreProcess. 
//   Initializes all internal structures.
//
// Parameters:
//   newPid: the process id for this process.
//   sharedObject: the name of the .so file containing the instance code of
//                 one of the operators
//
// Return value: None.
///////////////////////////////////////////////////////////////////////////////
ScoreProcess::ScoreProcess(pid_t newPid, char *sharedObject) {
  ScoreProcessInit(newPid);

  // break the name of the sharedObject (assume fully qualified name in a
  // form /dir/dir1/dir2/something.so) into /dir/dir1/dir2/ and something.so
  // copy the path part into the userDir.
  int len;
  for (len = strlen (sharedObject); len > 0; len --) {
    if (sharedObject[len] == '/') // look for the last /
      break;
  }

  assert(len > 1); // there has to be at least one /
  
  len ++; // include the last /

  userDir = new char[len + 1];
  strncpy(userDir, sharedObject, len);
  userDir[len] = '\0';

  // initialize nextTag
  nextTag = 0;

  // create the feedback graph object that will be used to collect info
  if (gFeedbackMode != NOTHING) 
    gFeedbackObj = feedbackGraph = new ScoreFeedbackGraph(userDir);
}
#endif

///////////////////////////////////////////////////////////////////////////////
// ScoreProcess::~ScoreProcess:
//   Destructor for ScoreProcess.
//   Cleans up all internal structures.
//
// Parameters: None.
//
// Return value: None.
///////////////////////////////////////////////////////////////////////////////
ScoreProcess::~ScoreProcess() {
  unsigned int i;

  // clean up allocate fifo buffer space.
  for (i = 0; i < SCORECUSTOMLIST_LENGTH(nodeList); i++) {
    ScoreGraphNode *currentNode;

    SCORECUSTOMLIST_ITEMAT(nodeList, i, currentNode);

    if (currentNode->isPage()) {
      ScorePage *currentPage = (ScorePage *) currentNode;

      if (currentPage->sched_fifoBuffer != NULL) {
	free(currentPage->sched_fifoBuffer);
      }
    } else {
      ScoreSegment *currentSegment = (ScoreSegment *) currentNode;

      if (currentSegment->sched_fifoBuffer != NULL) {
	free(currentSegment->sched_fifoBuffer);
      }
    }
  }
  delete(nodeList);

  delete(processorIStreamList);
  delete(processorOStreamList);

  // go through and free the operator instances and clusters.
  // also clean up stitch buffer list.
  // NOTE: We assume that the operator takes care of freeing ScorePage
  //       and ScoreSegments.
  for (i = 0; i < SCORECUSTOMLIST_LENGTH(operatorList); i++) {
    ScoreOperatorInstance *node;

    SCORECUSTOMLIST_ITEMAT(operatorList, i, node);

    delete node;
  }
  delete(operatorList);
  for (i = 0; i < SCORECUSTOMLIST_LENGTH(clusterList); i++) {
    ScoreCluster *node;

    SCORECUSTOMLIST_ITEMAT(clusterList, i, node);

    delete node;
  }
  delete(clusterList);
  for (i = 0; i < SCORECUSTOMLIST_LENGTH(stitchBufferList); i++) {
    ScoreSegmentStitch *node;

    SCORECUSTOMLIST_ITEMAT(stitchBufferList, i, node);

    delete node;
  }
  delete(stitchBufferList);

#if GET_FEEDBACK
  if (gFeedbackMode != NOTHING && gFeedbackMode != SAMPLERATES)
    delete feedbackGraph;
  assert(userDir);
  if (gFeedbackMode != SAMPLERATES)
    delete userDir;
#endif
}


///////////////////////////////////////////////////////////////////////////////
// ScoreProcess::addOperator:
//   Adds an operator instance to the process and adds the pages and memory
//     segments to the process.
//
// Parameters:
//   newOperator: a pointer to the operator instance.
//
// Return value: None.
///////////////////////////////////////////////////////////////////////////////
void ScoreProcess::addOperator(ScoreOperatorInstance *newOperator) {
  unsigned int i;


  // set the parent process pointer of the operator.
  newOperator->parentProcess = this;

  // insert the operator into the operator list.
  SCORECUSTOMLIST_APPEND(operatorList, newOperator);

  // increment the number of pages.
  numPages = numPages + newOperator->pages;
  numSegments = numSegments + newOperator->segments;

  // insert the operator pages and segments into the process.
  // also set the parent process pointers.
  // also initialize the status of each page/memory segment.
  // FIX ME! We might want to check to make sure no duplicate pages/segments
  //         were given! But, for now, assume the rest of the runtime
  //         system is being nice!
  for (i = 0; i < newOperator->pages; i++) {
    ScorePage *currentPage = newOperator->page[i];

    currentPage->sched_parentProcess = this;
    SCORECUSTOMLIST_APPEND(nodeList, currentPage);
#if GET_FEEDBACK
    currentPage->uniqTag = getNextTag();
    if (gFeedbackMode == MAKEFEEDBACK || gFeedbackMode == SAMPLERATES) {
      currentPage->feedbackNode =
	feedbackGraph->addNode(currentPage->uniqTag, currentPage);
    } else if (gFeedbackMode == READFEEDBACK) {
      if (feedbackGraph->getStatus()) { // check that the graph is valid
	currentPage->feedbackNode = 
	  feedbackGraph->getNodePtr(currentPage->uniqTag);
	currentPage->feedbackNode->getStatInfo(&currentPage->stat_consumption,
					       &currentPage->stat_production,
					       &currentPage->stat_fireCount);
      }
    }
#endif
  }
  for (i = 0; i < newOperator->segments; i++) {
    ScoreSegment *currentSegment = newOperator->segment[i];

    currentSegment->sched_parentProcess = this;
    SCORECUSTOMLIST_APPEND(nodeList, currentSegment);
#if GET_FEEDBACK
    currentSegment->uniqTag = getNextTag();
    if (gFeedbackMode == MAKEFEEDBACK || gFeedbackMode == SAMPLERATES) {
      currentSegment->feedbackNode =
	feedbackGraph->addNode(currentSegment->uniqTag, currentSegment);
    } else if (gFeedbackMode == READFEEDBACK) {
      if (feedbackGraph->getStatus()) { // check that the graph is valid
	currentSegment->feedbackNode = 
	  feedbackGraph->getNodePtr(currentSegment->uniqTag);
	currentSegment->feedbackNode->getStatInfo(&currentSegment->stat_consumption,
						  &currentSegment->stat_production,
						  &currentSegment->stat_fireCount);
      }
    }
#endif
  }
}


///////////////////////////////////////////////////////////////////////////////
// ScoreProcess::addCluster:
//   Adds a cluster to the process.
//
// Parameters:
//   newCluster: a pointer to the cluster.
//
// Return value: None.
///////////////////////////////////////////////////////////////////////////////
void ScoreProcess::addCluster(ScoreCluster *newCluster) {
  // set the parent process pointer of the cluster.
  newCluster->parentProcess = this;

  // insert the cluster into the operator list.
  SCORECUSTOMLIST_APPEND(clusterList, newCluster);
}



