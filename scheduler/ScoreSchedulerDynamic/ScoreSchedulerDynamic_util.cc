/////////////////////////////////////////////////////////////////////////////
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
// $Revision: 1.1 $
//
//////////////////////////////////////////////////////////////////////////////

#include <iostream>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <values.h>
#include <dlfcn.h>
#include "LEDA/core/list.h"
#include "LEDA/graph/graph.h"
//#include "LEDA/basic_graph_alg.h"
//#include "Kernel/graph/graph_alg.h"
#include "ScoreOperatorInstance.h"
#include "ScoreStream.h"
#include "ScoreStreamStitch.h"
#include "ScorePage.h"
#include "ScoreSegment.h"
#include "ScoreSegmentStitch.h"
#include "ScoreCluster.h"
#include "ScoreProcess.h"
#include "ScoreProcessorNode.h"
#include "ScoreArray.h"
#include "ScoreType.h"
#include "ScoreHardwareAPI.h"
#include "ScoreSchedulerDynamic.h"
#include "ScoreRuntime.h"
#include "ScoreSimulator.h"
#include "ScoreConfig.h"
#include "ScoreStateGraph.h"
#include "ScoreDummyDonePage.h"
#include "ScoreCustomStack.h"
#include "ScoreCustomList.h"
#include "ScoreCustomQueue.h"
#include "ScoreCustomLinkedList.h"

void printGraphNodeCustomList(const char *msg,
			      ScoreCustomList<ScoreGraphNode*> *l)
{
  cerr << "SCHED: " << msg << ": " 
       << SCORECUSTOMLIST_LENGTH(l) << endl;

  for (unsigned int i = 0; i < SCORECUSTOMLIST_LENGTH(l); i ++) {
    ScoreGraphNode *currentNode;

    SCORECUSTOMLIST_ITEMAT(l, i, currentNode);
    
    cerr << "SCHED: " << (unsigned int) currentNode << endl;
  }
}

void printStitchCustomList(const char *msg,
			   ScoreCustomList<ScoreSegmentStitch *> *l)
{
  cerr << "SCHED: " << msg << ": " << SCORECUSTOMLIST_LENGTH(l) << endl;
  for (unsigned int i = 0; i < SCORECUSTOMLIST_LENGTH(l); i++) {
    ScoreSegmentStitch *currentStitch;
    
    SCORECUSTOMLIST_ITEMAT(l, i, currentStitch);
    
    cerr << "SCHED: " << (unsigned int) currentStitch << endl;
  }
}

void printFaultedMemSeg(ScoreCustomList<ScoreSegment*> *l)
{
  cerr << "SCHED: NUMBER OF FAULTED SEGMENTS: " << 
    SCORECUSTOMLIST_LENGTH(l) << endl;
  for (unsigned int i = 0; i < SCORECUSTOMLIST_LENGTH(l); i++) {
    ScoreSegment *currentSegment;

    SCORECUSTOMLIST_ITEMAT(l, i, currentSegment);

    cerr << "SCHED:    FAULTED MEMSEG: " << (unsigned int) currentSegment << 
      "(TRA: " << currentSegment->traAddr << 
      " FAULTADDR: " << currentSegment->sched_faultedAddr << ")" << endl;
  }
}

void printClusterCustomList(const char *msg,
			    ScoreCustomList<ScoreCluster*> *l)
{
  cerr << "SCHED: " << msg << ": " << SCORECUSTOMLIST_LENGTH(l) << endl;
  for (unsigned int i = 0; i < SCORECUSTOMLIST_LENGTH(l); i++) {
    ScoreCluster *currentCluster;
    
    SCORECUSTOMLIST_ITEMAT(l, i, currentCluster);
    
    cerr << "SCHED: " << (unsigned int) currentCluster << endl;
  }
}

void printSchedStateBeforeScheduleClusters
(ScoreCustomList<ScoreCluster*> *headClusterList,
#if FRONTIERLIST_USEPRIORITY
  ScoreCustomPriorityList<ScoreCluster *> *frontierClusterList,
#else
  ScoreCustomLinkedList<ScoreCluster *> *frontierClusterList,
#endif
 ScoreCustomLinkedList<ScoreCluster *> *waitingClusterList)
{
  unsigned int i, j;
  SCORECUSTOMLINKEDLISTITEM listItem;

  cerr << "SCHED: ===> BEFORE SCHEDULECLUSTERS()" << endl;
  cerr << "SCHED: HEAD CLUSTER LIST HAS " << 
    SCORECUSTOMLIST_LENGTH(headClusterList) << " CLUSTERS" << endl;
  for (i = 0; i < SCORECUSTOMLIST_LENGTH(headClusterList); i++) {
    ScoreCluster *currentCluster;

    SCORECUSTOMLIST_ITEMAT(headClusterList, i, currentCluster);
      
    cerr << "SCHED:    CLUSTER " << (unsigned int) currentCluster << endl;
    for (j = 0; j < SCORECUSTOMLIST_LENGTH(currentCluster->nodeList);
	 j++) {
      ScoreGraphNode *currentNode;

      SCORECUSTOMLIST_ITEMAT(currentCluster->nodeList, j, currentNode);

      if (currentNode->isPage()) {
	cerr << "SCHED:       PAGE ";
      } else if (currentNode->isSegment()) {
	cerr << "SCHED:       SEGMENT ";
      } else {
	cerr << "SCHED:       UNKNOWN ";
      }

      cerr << (unsigned int) currentNode << endl;
    }
  }
#if FRONTIERLIST_USEPRIORITY
  // NOTE: Due to the nature of the heap, I cannot output it in priority
  // order!
  cerr << "SCHED: FRONTIER CLUSTER LIST HAS " << 
    SCORECUSTOMPRIORITYLIST_LENGTH(frontierClusterList) << 
    " CLUSTERS" << endl;
  for (i = 1; i <= SCORECUSTOMPRIORITYLIST_LENGTH(frontierClusterList); i++) {
    ScoreCluster *currentCluster;

    SCORECUSTOMPRIORITYLIST_ITEMATMAPINDEX(frontierClusterList,
					   i, currentCluster);
      
    cerr << "SCHED:    CLUSTER " << (unsigned int) currentCluster << endl;
    for (j = 0; j < SCORECUSTOMLIST_LENGTH(currentCluster->nodeList);
	 j++) {
      ScoreGraphNode *currentNode;

      SCORECUSTOMLIST_ITEMAT(currentCluster->nodeList, j, currentNode);

      if (currentNode->isPage()) {
	cerr << "SCHED:       PAGE ";
      } else if (currentNode->isSegment()) {
	cerr << "SCHED:       SEGMENT ";
      } else {
	cerr << "SCHED:       UNKNOWN ";
      }

      cerr << (unsigned int) currentNode << endl;
    }
  }
#else
  cerr << "SCHED: FRONTIER CLUSTER LIST HAS " << 
    SCORECUSTOMLINKEDLIST_LENGTH(frontierClusterList) << " CLUSTERS" << endl;
  SCORECUSTOMLINKEDLIST_HEAD(frontierClusterList, listItem);
  while (listItem != SCORECUSTOMLINKEDLIST_NULL) {
    ScoreCluster *currentCluster;

    SCORECUSTOMLINKEDLIST_ITEMAT(frontierClusterList, 
				 listItem, currentCluster);
      
    cerr << "SCHED:    CLUSTER " << (unsigned int) currentCluster << endl;
    for (i = 0; i < SCORECUSTOMLIST_LENGTH(currentCluster->nodeList);
	 i++) {
      ScoreGraphNode *currentNode;

      SCORECUSTOMLIST_ITEMAT(currentCluster->nodeList, i, currentNode);

      if (currentNode->isPage()) {
	cerr << "SCHED:       PAGE ";
      } else if (currentNode->isSegment()) {
	cerr << "SCHED:       SEGMENT ";
      } else {
	cerr << "SCHED:       UNKNOWN ";
      }

      cerr << (unsigned int) currentNode << endl;
    }

    SCORECUSTOMLINKEDLIST_GOTONEXT(frontierClusterList, listItem);
  }
#endif
  cerr << "SCHED: WAITING CLUSTER LIST HAS " << 
    SCORECUSTOMLINKEDLIST_LENGTH(waitingClusterList) <<
    " CLUSTERS" << endl;
  SCORECUSTOMLINKEDLIST_HEAD(waitingClusterList, listItem);
  while (listItem != SCORECUSTOMLINKEDLIST_NULL) {
    ScoreCluster *currentCluster;

    SCORECUSTOMLINKEDLIST_ITEMAT(waitingClusterList, 
				 listItem, currentCluster);
      
    cerr << "SCHED:    CLUSTER " << (unsigned int) currentCluster << endl;
    for (i = 0; i < SCORECUSTOMLIST_LENGTH(currentCluster->nodeList);
	 i++) {
      ScoreGraphNode *currentNode;

      SCORECUSTOMLIST_ITEMAT(currentCluster->nodeList, i, currentNode);

      if (currentNode->isPage()) {
	cerr << "SCHED:       PAGE ";
      } else if (currentNode->isSegment()) {
	cerr << "SCHED:       SEGMENT ";
      } else {
	cerr << "SCHED:       UNKNOWN ";
      }

      cerr << (unsigned int) currentNode << endl;
    }

    SCORECUSTOMLINKEDLIST_GOTONEXT(waitingClusterList, listItem);
  }
}

void printSchedStateAfterScheduleClusters
(
#if FRONTIERLIST_USEPRIORITY
 ScoreCustomPriorityList<ScoreCluster *> *frontierClusterList,
#else
 ScoreCustomLinkedList<ScoreCluster *> *frontierClusterList,
#endif
 ScoreCustomLinkedList<ScoreCluster *> *waitingClusterList,
 
 ScoreCustomList<ScorePage *> *scheduledPageList,
 ScoreCustomList<ScoreSegment *> *scheduledMemSegList,
 ScoreCustomList<ScorePage *> *removedPageList,
 ScoreCustomList<ScoreSegment *> *removedMemSegList
)
{
  SCORECUSTOMLINKEDLISTITEM listItem;
  unsigned int i;

  cerr << "SCHED: ===> AFTER SCHEDULECLUSTERS()" << endl;
#if FRONTIERLIST_USEPRIORITY
  // NOTE: Due to the nature of the heap, I cannot output it in priority
  // order!
  cerr << "SCHED: FRONTIER CLUSTER LIST HAS " << 
    SCORECUSTOMPRIORITYLIST_LENGTH(frontierClusterList) << 
    " CLUSTERS" << endl;
  for (i = 1; i <= SCORECUSTOMPRIORITYLIST_LENGTH(frontierClusterList); i++) {
    ScoreCluster *currentCluster;

    SCORECUSTOMPRIORITYLIST_ITEMATMAPINDEX(frontierClusterList,
					   i, currentCluster);
      
    cerr << "SCHED:    CLUSTER " << (unsigned int) currentCluster << endl;
    for (j = 0; j < SCORECUSTOMLIST_LENGTH(currentCluster->nodeList);
	 j++) {
      ScoreGraphNode *currentNode;

      SCORECUSTOMLIST_ITEMAT(currentCluster->nodeList, j, currentNode);

      if (currentNode->isPage()) {
	cerr << "SCHED:       PAGE ";
      } else if (currentNode->isSegment()) {
	cerr << "SCHED:       SEGMENT ";
      } else {
	cerr << "SCHED:       UNKNOWN ";
      }

      cerr << (unsigned int) currentNode << endl;
    }
  }
#else
  cerr << "SCHED: FRONTIER CLUSTER LIST HAS " << 
    SCORECUSTOMLINKEDLIST_LENGTH(frontierClusterList) << " CLUSTERS" << endl;
  SCORECUSTOMLINKEDLIST_HEAD(frontierClusterList, listItem);
  while (listItem != SCORECUSTOMLINKEDLIST_NULL) {
    ScoreCluster *currentCluster;

    SCORECUSTOMLINKEDLIST_ITEMAT(frontierClusterList, 
				 listItem, currentCluster);
      
    cerr << "SCHED:    CLUSTER " << (unsigned int) currentCluster << endl;
    for (i = 0; i < SCORECUSTOMLIST_LENGTH(currentCluster->nodeList);
	 i++) {
      ScoreGraphNode *currentNode;

      SCORECUSTOMLIST_ITEMAT(currentCluster->nodeList, i, currentNode);

      if (currentNode->isPage()) {
	cerr << "SCHED:       PAGE ";
      } else if (currentNode->isSegment()) {
	cerr << "SCHED:       SEGMENT ";
      } else {
	cerr << "SCHED:       UNKNOWN ";
      }

      cerr << (unsigned int) currentNode << endl;
    }

    SCORECUSTOMLINKEDLIST_GOTONEXT(frontierClusterList, listItem);
  }
#endif
  cerr << "SCHED: WAITING CLUSTER LIST HAS " << 
    SCORECUSTOMLINKEDLIST_LENGTH(waitingClusterList) <<
    " CLUSTERS" << endl;
  SCORECUSTOMLINKEDLIST_HEAD(waitingClusterList, listItem);
  while (listItem != SCORECUSTOMLINKEDLIST_NULL) {
    ScoreCluster *currentCluster;

    SCORECUSTOMLINKEDLIST_ITEMAT(waitingClusterList, 
				 listItem, currentCluster);
      
    cerr << "SCHED:    CLUSTER " << (unsigned int) currentCluster << endl;
    for (i = 0; i < SCORECUSTOMLIST_LENGTH(currentCluster->nodeList);
	 i++) {
      ScoreGraphNode *currentNode;

      SCORECUSTOMLIST_ITEMAT(currentCluster->nodeList, i, currentNode);

      if (currentNode->isPage()) {
	cerr << "SCHED:       PAGE ";
      } else if (currentNode->isSegment()) {
	cerr << "SCHED:       SEGMENT ";
      } else {
	cerr << "SCHED:       UNKNOWN ";
      }

      cerr << (unsigned int) currentNode << endl;
    }

    SCORECUSTOMLINKEDLIST_GOTONEXT(waitingClusterList, listItem);
  }

  if (VERBOSEDEBUG || DEBUG || PRINTSTATE) {
    cerr << "SCHED: NUMBER OF SCHEDULED PAGES: " << 
      SCORECUSTOMLIST_LENGTH(scheduledPageList) << endl;
    for (i = 0; i < SCORECUSTOMLIST_LENGTH(scheduledPageList); i++) {
      ScorePage *currentPage;

      SCORECUSTOMLIST_ITEMAT(scheduledPageList, i, currentPage);

      cerr << "SCHED:    SCHEDULED PAGE: " << 
	(unsigned int) currentPage << endl;
    }
    cerr << "SCHED: NUMBER OF SCHEDULED MEMSEG: " << 
      SCORECUSTOMLIST_LENGTH(scheduledMemSegList) << endl;
    for (i = 0; i < SCORECUSTOMLIST_LENGTH(scheduledMemSegList); i++) {
      ScoreSegment *currentMemSeg;

      SCORECUSTOMLIST_ITEMAT(scheduledMemSegList, i, currentMemSeg);

      cerr << "SCHED:    SCHEDULED MEMSEG: " << 
	(unsigned int) currentMemSeg << endl;
    }
    cerr << "SCHED: NUMBER OF REMOVED PAGES: " << 
      SCORECUSTOMLIST_LENGTH(removedPageList) << endl;
    for (i = 0; i < SCORECUSTOMLIST_LENGTH(removedPageList); i++) {
      ScorePage *currentPage;

      SCORECUSTOMLIST_ITEMAT(removedPageList, i, currentPage);

      cerr << "SCHED:    REMOVED PAGE: " << 
	(unsigned int) currentPage << endl;
    }
    cerr << "SCHED: NUMBER OF REMOVED MEMSEG: " << 
      SCORECUSTOMLIST_LENGTH(removedMemSegList) << endl;
    for (i = 0; i < SCORECUSTOMLIST_LENGTH(removedMemSegList); i++) {
      ScoreSegment *currentMemSeg;

      SCORECUSTOMLIST_ITEMAT(removedMemSegList, i, currentMemSeg);

      cerr << "SCHED:    REMOVED MEMSEG: " << 
	(unsigned int) currentMemSeg << endl;
    }
  }
}

void printArrayState(ScoreArrayCP *arrayCP, unsigned numPhysicalCP,
		     ScoreArrayCMB *arrayCMB, unsigned numPhysicalCMB)
{
  unsigned int i;
  
  for (i = 0; i < numPhysicalCP; i++) {
    cerr << "SCHED: arrayCP[" << i << "]: "
      "active " << (unsigned int) arrayCP[i].active <<
      " actual " << (unsigned int) arrayCP[i].actual << endl;
  }
  for (i = 0; i < numPhysicalCMB; i++) {
    cerr << "SCHED: arrayCMB[" << i << "]: "
      "active " << (unsigned int) arrayCMB[i].active <<
      " actual " << (unsigned int) arrayCMB[i].actual << endl;
  }
}
