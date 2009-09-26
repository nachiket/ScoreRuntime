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
// $Revision: 1.36 $
//
//////////////////////////////////////////////////////////////////////////////

#include "ScoreStream.h"
#include "ScorePage.h"
#include "ScoreSegment.h"
#include "ScoreGraphNode.h"
#include "ScoreCluster.h"
#include "ScoreCustomList.h"
#include "ScoreCustomLinkedList.h"
#include "ScoreCustomPriorityList.h"
#include "ScoreConfig.h"


///////////////////////////////////////////////////////////////////////////////
// ScoreCluster::ScoreCluster:
//   Constructor for ScoreCluster.
//   Initializes all internal structures.
//
// Parameters: None.
//
// Return value: None.
///////////////////////////////////////////////////////////////////////////////
ScoreCluster::ScoreCluster() {
  // initialize variables.
  clusterWaitingListItem = SCORECUSTOMLINKEDLIST_NULL;
  clusterResidentListItem = SCORECUSTOMLINKEDLIST_NULL;
#if FRONTIERLIST_USEPRIORITY
  clusterFrontierListItem = SCORECUSTOMPRIORITYLIST_NULL;
#else
  clusterFrontierListItem = SCORECUSTOMLINKEDLIST_NULL;
#endif
  isResident = 0;
  isScheduled = 0;
  isFreeable = 0;
  isFrontier = 0;
  isHead = 0;
  lastResidentTimeslice = 0;
  numPages = 0;
  numSegments = 0;
  shouldNotBeFreed = 0;
  lastCalculatedPriority = 0;
  lastFrontierTraversal = 0;
  nodeList = 
    new ScoreCustomList<ScoreGraphNode *>(SCORE_CLUSTERNODELIST_BOUND);
  inputList =
    new ScoreCustomList<SCORE_STREAM>(SCORE_CLUSTERINPUTLIST_BOUND);
  outputList =
    new ScoreCustomList<SCORE_STREAM>(SCORE_CLUSTEROUTPUTLIST_BOUND);
#if ASPLOS2000
  clusterSpecs = NULL;
  numClusterSpecNotDone = NULL;
#endif

#ifdef RANDOM_SCHEDULER
  // this is to indicate that this node was never scheduled before
  // the reason, why -1 is not appropriate is that on the first
  // timeslice (currentTimeslice == 0), the value -1 (=currentTimeslice-1)
  // indicates that this node was scheduled on the previous timeslice
  // and thus must be removed from the array (which is not accurate).
  sched_lastTimesliceScheduled = (unsigned int)(-2);
  sched_isDone = 0;
  sched_residentClusterListItem = SCORECUSTOMLINKEDLIST_NULL;
  sched_stitchAdded = 0;
#endif

}


///////////////////////////////////////////////////////////////////////////////
// ScoreCluster::~ScoreCluster:
//   Destructor for ScoreCluster.
//   Cleans up all internal structures.
//
// Parameters: None.
//
// Return value: None.
///////////////////////////////////////////////////////////////////////////////
ScoreCluster::~ScoreCluster() {
  delete(nodeList);
  delete(inputList);
  delete(outputList);

#if ASPLOS2000
  if (clusterSpecs != NULL) {
    delete(clusterSpecs);
  }
  if (numClusterSpecNotDone != NULL) {
    delete(numClusterSpecNotDone);
  }
#endif
}


///////////////////////////////////////////////////////////////////////////////
// ScoreCluster::addNode:
//   Adds a node to the cluster. This will automatically adjust the input
//     and output stream IOs for the cluster.
//
// Parameters:
//   newNode: a pointer to the node.
//
// Return value: None.
///////////////////////////////////////////////////////////////////////////////
void ScoreCluster::addNode(ScoreGraphNode *newNode) {
  int numInputs = newNode->getInputs();
  int numOutputs = newNode->getOutputs();
  int i;


  // set the parent cluster of the node.
  newNode->sched_parentCluster = this;

  // determine if the node is a page or segment and add to the appropriate
  // list.
  SCORECUSTOMLIST_APPEND(nodeList, newNode);
  if (newNode->isPage()) {
    numPages++;
  } else {
    numSegments++;
  }

  // check all of the inputs and outputs of the node.
  // FIX ME! This seems like a little bit inefficient manner to
  //         maintain the input/output stream links from a cluster.
  //         Perhaps examine only updating stream IO from a cluster
  //         after all nodes are added.
  for (i = 0; i < numInputs; i++) {
    SCORE_STREAM inStream = newNode->getSchedInput(i);

    if (inStream != NULL) {
      // make sure this is not a self-loop.
      if (inStream->sched_src != newNode) {
	// if this is a stream to a processor operator, then go ahead and
	// place it on the input stream list.
	// otherwise, check to see if we should add/remove it.
	if (inStream->sched_srcFunc == STREAM_OPERATOR_TYPE) {
	  SCORECUSTOMLIST_APPEND(inputList, inStream);
	  inStream->sched_isCrossCluster = 1;
	} else {
	  ScoreGraphNode *srcNode = inStream->sched_src;
	  
	  // if this node is not part of this cluster, then add the stream
	  // to the input stream list.
	  // otherwise, remove this stream from the output stream list.
	  if (srcNode->sched_parentCluster != this) {
	    SCORECUSTOMLIST_APPEND(inputList, inStream);
	    inStream->sched_isCrossCluster = 1;
	  } else {
	    SCORECUSTOMLIST_REMOVE(outputList, inStream);
	    inStream->sched_isCrossCluster = 0;
	  }
	}
      }
    }
  }
  for (i = 0; i < numOutputs; i++) {
    SCORE_STREAM outStream = newNode->getSchedOutput(i);

    if (outStream != NULL) {
      // make sure this is not a self-loop.
      if (outStream->sched_sink != newNode) {
	// if this is a stream to a processor operator, then go ahead and
	// place it on the output stream list.
	// otherwise, check to see if we should add/remove it.
	if (outStream->sched_snkFunc == STREAM_OPERATOR_TYPE) {
	  SCORECUSTOMLIST_APPEND(outputList, outStream);
	  outStream->sched_isCrossCluster = 1;
	} else {
	  ScoreGraphNode *sinkNode = outStream->sched_sink;
	  
	  // if this segment is not part of this cluster, then add the stream
	  // to the output stream list.
	  // otherwise, remove this stream from the input stream list.
	  if (sinkNode->sched_parentCluster != this) {
	    SCORECUSTOMLIST_APPEND(outputList, outStream);
	    outStream->sched_isCrossCluster = 1;
	  } else {
	    SCORECUSTOMLIST_REMOVE(inputList, outStream);
	    outStream->sched_isCrossCluster = 0;
	  }
	} 
      }
    }
  }

#ifdef RANDOM_SCHEDULER
  newNode->setLastTimesliceScheduled(sched_lastTimesliceScheduled);
#endif
}


void ScoreCluster::addNode_noAddIO(ScoreGraphNode *newNode) {
  // set the parent cluster of the node.
  newNode->sched_parentCluster = this;

  // determine if the node is a page or segment and add to the appropriate
  // list.
  SCORECUSTOMLIST_APPEND(nodeList, newNode);
  if (newNode->isPage()) {
    numPages++;
  } else {
    numSegments++;
  }

#ifdef RANDOM_SCHEDULER
  newNode->setLastTimesliceScheduled(sched_lastTimesliceScheduled);
#endif
}


///////////////////////////////////////////////////////////////////////////////
// ScoreCluster::removeNode:
//   Removes a node from the cluster. This will automatically adjust the input
//     and output stream IOs for the cluster.
//
// Parameters:
//   oldNode: a pointer to the node.
//
// Return value: None.
///////////////////////////////////////////////////////////////////////////////
void ScoreCluster::removeNode(ScoreGraphNode *oldNode) {
  int numInputs = oldNode->getInputs();
  int numOutputs = oldNode->getOutputs();
  int i;

  // determine whether this is a page or a segment and remove it from the
  // appropriate list.
  SCORECUSTOMLIST_REMOVE(nodeList, oldNode);
  if (oldNode->isPage()) {
    numPages--;
  } else {
    numSegments--;
  }

  // check all of the inputs and outputs of the node.
  for (i = 0; i < numInputs; i++) {
    SCORE_STREAM inStream = oldNode->getSchedInput(i);

    if (inStream != NULL) {
      if (inStream->sched_isCrossCluster) {
	SCORECUSTOMLIST_REMOVE(inputList, inStream);
      }
    }
  }
  for (i = 0; i < numOutputs; i++) {
    SCORE_STREAM outStream = oldNode->getSchedOutput(i);

    if (outStream != NULL) {
      if (outStream->sched_isCrossCluster) {
	SCORECUSTOMLIST_REMOVE(outputList, outStream);
      }
    }
  }
}


void ScoreCluster::removeNode_noRemoveIO(ScoreGraphNode *oldNode) {
  // determine whether this is a page or a segment and remove it from the
  // appropriate list.
  SCORECUSTOMLIST_REMOVE(nodeList, oldNode);
  if (oldNode->isPage()) {
    numPages--;
  } else {
    numSegments--;
  }
}


///////////////////////////////////////////////////////////////////////////////
// ScoreCluster::getNumPagesRequired:
//   Determines the number of physical pages required by the cluster.
//
// Parameters: None.
//
// Return value:
//   the number of physical pages required by the cluster.
///////////////////////////////////////////////////////////////////////////////
unsigned int ScoreCluster::getNumPagesRequired() {
  return(numPages);
}


///////////////////////////////////////////////////////////////////////////////
// ScoreCluster::getNumMemSegRequired:
//   Determines the number of physical memory segments required by the cluster.
//
// Parameters: None.
//
// Return value:
//   the number of physical memory segments required by the cluster.
///////////////////////////////////////////////////////////////////////////////
unsigned int ScoreCluster::getNumMemSegRequired() {
  unsigned int numSelfLoops;
  unsigned int i;

  numSelfLoops = 0;
  for (i = 0; i < SCORECUSTOMLIST_LENGTH(outputList); i++) {
    ScoreStream *currentStream;

    SCORECUSTOMLIST_ITEMAT(outputList, i, currentStream);

    if (!(currentStream->sched_sinkIsDone) &&
	(currentStream->sched_snkFunc != STREAM_OPERATOR_TYPE) &&
	(currentStream->sched_sink->sched_parentCluster == this)) {
      numSelfLoops++;
    }
  }

  return(SCORECUSTOMLIST_LENGTH(inputList) + 
	 SCORECUSTOMLIST_LENGTH(outputList) + numSegments - 
	 numSelfLoops);
}


// required by LEDA for use with lists/etc.

// provides comparison operation between ScoreClusters.
// NOTE: Right now, we only say 2 clusters are equal if their pointers
//       are equal. Otherwise, less than/greater than is determined
//       simply by their pointer values.
int compare(ScoreCluster * const & left, ScoreCluster * const & right) {
  if (left == right) {
    return(0);
  } else if (left < right) {
    return(-1);
  } else {
    return(1);
  }
}
