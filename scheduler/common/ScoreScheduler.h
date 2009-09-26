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


#ifndef _ScoreScheduler_h__
#define _ScoreScheduler_h__


#include <unistd.h>
#include <pthread.h>
#include "LEDA/core/list.h"
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

// This class is an interface to/from ScoreScheduler to the rest of the system

class ScoreScheduler {
 public:

  ScoreScheduler() {}
  virtual ~ScoreScheduler() {} 

  virtual int addOperator(char *sharedObject, char *argbuf, pid_t pid) = 0;
  virtual void doSchedule() = 0;
  virtual unsigned int getCurrentTimeslice() = 0;

 protected:

};



#endif
