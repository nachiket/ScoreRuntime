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
// $Revision: 1.26 $
//
//////////////////////////////////////////////////////////////////////////////
#ifndef _ScoreCluster_H

#define _ScoreCluster_H

#if ASPLOS2000
#include "LEDA/core/list.h"
#endif
#if FRONTIERLIST_USEPRIORITY
#include "ScoreCustomPriorityList.h"
#endif
#include "LEDA/graph/graph.h"
#include "ScoreStream.h"
#include "ScoreGraphNode.h"
#include "ScoreCustomList.h"
#include "ScoreCustomLinkedList.h"


// prototypes to avoid circular includes.
class ScoreProcess;


// ScoreCluster: stores information about a cluster of ScorePages.
class ScoreCluster {
 public:
  ScoreCluster();
  ~ScoreCluster();

  void addNode(ScoreGraphNode *newNode);
  void addNode_noAddIO(ScoreGraphNode *newNode);
  void removeNode(ScoreGraphNode *oldNode);
  void removeNode_noRemoveIO(ScoreGraphNode *oldNode);
  unsigned int getNumPagesRequired();
  unsigned int getNumMemSegRequired();

  // stores the list of pages/segments in the cluster.
  ScoreCustomList<ScoreGraphNode *> *nodeList;

  // stores the number of pages/segments in this cluster.
  unsigned int numPages, numSegments;

  // stores the list of stream IOs from the cluster (both not done and done).
  ScoreCustomList<SCORE_STREAM> *inputList;
  ScoreCustomList<SCORE_STREAM> *outputList;

  // stores a point to the parent process.
  ScoreProcess *parentProcess;

  // flags whether or not this cluster is resident/scheduled.
  char isResident;
  char isScheduled;

  char isFreeable;
  char isFrontier;
  char isHead;

  // stores when the cluster was last resident on the array.
  unsigned int lastResidentTimeslice;

  unsigned int lastFrontierTraversal;

  // flag used by scheduler to indicate that this cluster should not be
  // freed.
  char shouldNotBeFreed;

  // stores the list_item of where this cluster is stored (i.e. waiting
  // list, resident list, etc).
  SCORECUSTOMLINKEDLISTITEM clusterWaitingListItem;
  SCORECUSTOMLINKEDLISTITEM clusterResidentListItem;
#if FRONTIERLIST_USEPRIORITY
  SCORECUSTOMPRIORITYLISTITEM clusterFrontierListItem;
#else
  SCORECUSTOMLINKEDLISTITEM clusterFrontierListItem;
#endif

  // used by graph algorithms to mark this cluster visited.
  char visited;

  // used by scheduler for clustering and graph operations.
  node graphNode;

  // used by scheduler to store the last calculated priority.
  int lastCalculatedPriority;


#ifdef RANDOM_SCHEDULER
  // contains the timeslice when this node was scheduled last
  // i.e. if (sched_lastTimesliceScheduled == currentTimeslice) then
  //            this node was selected to be scheduled during this timeslice
 private:
  unsigned int sched_lastTimesliceScheduled;
 public:
  unsigned int getLastTimesliceScheduled() const {
    return sched_lastTimesliceScheduled;
  }
  void setLastTimesliceScheduled(unsigned int val) {
    unsigned int numNodes = SCORECUSTOMLIST_LENGTH(nodeList);
    for (unsigned int i = 0; i < numNodes; i ++) {
      ScoreGraphNode *currentNode;
      SCORECUSTOMLIST_ITEMAT(nodeList, i, currentNode);
      currentNode->setLastTimesliceScheduled(val);
    }
    sched_lastTimesliceScheduled = val;
  }
  // used to mark done clusters
  char sched_isDone;
  
  SCORECUSTOMLINKEDLISTITEM sched_residentClusterListItem;

  // used to mark the clusters that have output stream connected
  // to the processor.
  char sched_stitchAdded;
#endif


#if ASPLOS2000
  // FIX ME! JUST FOR ASPLOS2000!
  list<ScorePage *> **clusterSpecs;
  unsigned int *numClusterSpecNotDone;
#endif

 private:
};


// needed by LEDA for use with lists/etc.
int compare(ScoreCluster * const & left, ScoreCluster * const & right);

#endif







