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
// $Revision: 1.6 $
//
//////////////////////////////////////////////////////////////////////////////
#ifndef _ScoreSchedulerRandom_H

#define _ScoreSchedulerRandom_H

#include <unistd.h>
#include <pthread.h>
#include "LEDA/core/list.h"
#include "LEDA/p_queue.h"
#include "ScoreCluster.h"
#include "ScoreProcess.h"
#include "ScoreArray.h"
#include "ScorePage.h"
#include "ScoreSegment.h"
#include "ScoreSegmentStitch.h"
#include "ScoreStream.h"
#include "ScoreStreamStitch.h"
#include "ScoreHardwareAPI.h"
#include "ScoreThreadCounter.h"
#include "ScoreProcessorNode.h"
#include "ScoreCustomStack.h"
#include "ScoreCustomList.h"
#include "ScoreCustomQueue.h"
#include "ScoreCustomLinkedList.h"
#include "ScoreCustomPriorityList.h"

#include "ScoreScheduler.h"

class ScoreStream;


// ScoreSchedulerRandom: the scheduler class.
class ScoreSchedulerRandom : public ScoreScheduler {
 public:
  ScoreSchedulerRandom(char exitOnIdle, char noDeadlockDetection,
                 char noImplicitDoneNodes, char stitchBufferDontCare);
  virtual ~ScoreSchedulerRandom();

  virtual int addOperator(char *sharedObject, char *argbuf, pid_t pid);
  virtual void doSchedule();
  virtual unsigned int getCurrentTimeslice();

  void calculateClusterPriority(ScoreCluster *currentCluster);

 protected:
  // flag for whether or not to exit when idle.
  char doExitOnIdle;

  char doNoDeadlockDetection;

  char doNotMakeNodesImplicitlyDone;

  char noCareStitchBufferInClusters;

  // thread counter for time accounting purpose
  ScoreThreadCounter *threadCounter;

  // this counts the number of timeslices that have occured.
  unsigned int currentTimeslice;

  // this is a stack of "spare" ScoreSegmentStitch and ScoreStreamStitch
  // that can be used when providing stitch buffers.
  ScoreCustomStack<ScoreSegmentStitch *> *spareSegmentStitchList;
  ScoreCustomStack<ScoreStream *> *spareStreamStitchList;

  // since methods in the ScoreScheduler class are called by multiple
  // threads, the following are mutexes used to control access to
  // scheduler data variables.
  pthread_mutex_t schedulerDataMutex;

  // stores the current parameters of the physical array.
  unsigned int numPhysicalCP, numPhysicalCMB;
  unsigned int cmbSize;

  // stores the current number of free CPs and CMBs.
  unsigned int currentNumFreeCPs, currentNumFreeCMBs;

  // stores information about what is in the current physical array.
  ScoreArrayCP *arrayCP;
  ScoreArrayCMB *arrayCMB;

  // stores the current physical array status.
  ScoreArrayCPStatus *cpStatus;
  ScoreArrayCMBStatus *cmbStatus;

  // stores the list of processes which currently are using the array.
  ScoreCustomList<ScoreProcess *> *processList;

  // stores the list of pages and memory segments that are newly scheduled and
  // newly removed.
  // also, stitch memory segments that have their configuration changed.
  ScoreCustomList<ScorePage *> *scheduledPageList;
  ScoreCustomList<ScoreSegment *> *scheduledMemSegList;
  ScoreCustomList<ScorePage *> *removedPageList;
  ScoreCustomList<ScoreSegment *> *removedMemSegList;
  ScoreCustomList<ScorePage *> *doneNotRemovedPageList;
  ScoreCustomList<ScoreSegment *> *doneNotRemovedMemSegList;
  ScoreCustomList<ScoreSegment *> *configChangedStitchSegList;

  // keep this list in case it becomes useful in the future
  ScoreCustomList<ScoreCluster *> *headClusterList;

  // all nonresident clusters reside here (list will permit choosing by number)
  ScoreCustomList<ScoreCluster *> *waitingClusterList;

  // all resident clusters reside here
  ScoreCustomLinkedList<ScoreCluster *> *residentClusterList;
  ScoreCustomLinkedList<ScoreSegmentStitch *> *residentStitchList;

  // all clusters that are to be scheduled in this round are here
  ScoreCustomLinkedList<ScoreCluster *> *scheduledClusterList;
  ScoreCustomLinkedList<ScoreSegmentStitch *> *scheduledStitchList;

  // stores the list of streams which go to/from the processor operators.
  ScoreCustomList<ScoreStream *> *processorIStreamList;
  ScoreCustomList<ScoreStream *> *processorOStreamList;

  // stores the list of done pages/segments.
  ScoreCustomList<ScoreGraphNode *> *doneNodeList;

  // stores the list of clusters that are considered freeable.
  ScoreCustomList<ScoreCluster *> *freeableClusterList;

  // stores the list of clusters that are considered completely done.
  ScoreCustomList<ScoreCluster *> *doneClusterList;

  // stores the list of memory segments that have faulted on their address.
  ScoreCustomList<ScoreSegment *> *faultedMemSegList;

  // stores the list of stitch buffers added to resolve buffer lock.
  // this lets scheduleClusters() know which stitch buffers it must schedule.
  ScoreCustomList<ScoreSegmentStitch *> *addedBufferLockStitchBufferList;

  // used for cleanup of stitch lists
  ScoreCustomList<ScoreSegmentStitch *> *emptyStitchList;

  // stores the list of stitch buffers.
  ScoreCustomList<ScoreSegmentStitch *> *stitchBufferList;

  // used by the scheduler to keep a list of physical CPs and CMBs which
  // are currently unused and can be used to schedule virtual CPs and CMBs.
  ScoreCustomQueue<unsigned int> *unusedPhysicalCPs;
  ScoreCustomLinkedList<unsigned int> *unusedPhysicalCMBs;

  // used by the scheduler to mark which physical CPs and CMBs are busy
  // during reconfiguration.
  char *arrayCPBusy, *arrayCMBBusy;

  // used by addOperator().
  ScoreCustomList<ScoreStream *> *operatorStreamsFromProcessor;
  ScoreCustomList<ScoreStream *> *operatorStreamsToProcessor;

  // used by findDonePagesSegments().
  ScoreCustomQueue<ScoreGraphNode *> *doneNodeCheckList;

  // used by findPotentiallyDeadLockedProcesses().
  ScoreCustomList<ScoreProcess *> *deadLockedProcesses;

  // used by findDeadLock().
  ScoreProcessorNode *processorNode;

  // flags whether or not the scheduler is currently paused due to
  // lack of jobs. (for simulation use only!).
  char isIdle;

  // flags whether or not the scheduler is reawakening from idle state.
  char isReawakening;

  // indicates when the last time it was reawakened.
  unsigned int lastReawakenTime;

  virtual void getCurrentStatus();
  virtual void gatherStatusInfo();
  virtual void findDonePagesSegments();
  virtual int checkImplicitDonePagesSegments(ScoreGraphNode *currentNode);
  virtual void findFaultedMemSeg();
#if RANDOM_SCHEDULER_VERSION == 1
  virtual void findFreeableClusters();
#endif
  ScoreCustomList<ScoreSegmentStitch*> *stitchListToMarkScheduled;
  ScoreCustomList<ScoreCluster*> *clusterListToAddProcessorStitch;
  virtual void dealWithDeadLock();
  virtual void scheduleClusters();
  virtual void performPlacement();
  virtual void issueReconfigCommands();
  virtual void performCleanup();

  ScoreSegmentStitch* insertStitchBufferOnInput(SCORE_STREAM attachedStream, 
						ScoreCluster *cluster,
						unsigned int clusterInputNum);

  ScoreSegmentStitch* insertStitchBufferOnOutput(ScoreCluster *cluster,
						 unsigned int clusterOutputNum,
						 SCORE_STREAM attachedStream);

  bool ScoreSchedulerRandom::recomputeDeltaCMB(int func,
					       ScoreGraphNode *mynode,
					       int *deltaCMB,
					       ScoreCluster *currentCluster);

  void findPotentiallyDeadLockedProcesses();
  void findDeadLock(ScoreProcess *currentProcess,
		    list<ScoreStream *> *bufferLockedStreams,
		    list<list <ScoreStream *> *> *deadLockedCycles);
  void findDeadLockedStreams_traverse_helper(
    ScoreGraphNode *currentNode,
    list<ScoreStream *> *traversedStreams,
    list<list<ScoreStream *> *> *bufferLockedCycles,
    ScoreGraphNode *processorNode,
    list<list<ScoreStream *> *> *deadLockedCycles);
  void resolveBufferLockedStreams(list<ScoreStream *> *bufferLockedStreams);
  void resolveDeadLockedCycles(list<list<ScoreStream *> *> *deadLockedCycles);

  void printCurrentState();
  void printSchedState();
  void printPlacementState();
  void visualizeCurrentState();
#if GET_FEEDBACK
  void makeFeedback();
#endif

};

#endif




