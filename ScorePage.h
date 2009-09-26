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
// $Revision: 1.25 $
//
//////////////////////////////////////////////////////////////////////////////
#ifndef _ScorePage_H

#define _ScorePage_H

#include "ScoreStreamType.h"
#include "ScoreStream.h"
#include "ScoreGraphNode.h"
#include "ScoreSegmentTable.h"


#define NO_LOCATION -1
#define NO_GROUP -1 


class ScorePage: public ScoreGraphNode {

public: 
  // no constructor, should be in derived class
  ScorePage(): grp(NO_GROUP), loc(NO_LOCATION), state(0) {
     sched_cachedSegmentBlock = NULL;
     sched_dumpSegmentBlock = NULL;
     sched_fifoBuffer = NULL;
     sched_isFIFOBufferValid = 0;
     sched_lastKnownState = 0;
     _isPage = 1;
     _isOperator = 0;
     _isSegment = 0;
  }
  int *bitstream() {return(bits);}
  // could do bounds checking
  ScoreIOMaskType inputs_consumed(int state)
    {return(consumes[state]);}
  ScoreIOMaskType outputs_produced(int state)
    {return(produces[state]);}
  int get_state() { return (state);}
  int group() { return(grp);}
  int location() { return(loc);}
  void setGroup(int ngrp) {grp=ngrp; }
  void setLocation(int nloc) {loc=nloc;}
  virtual int pagestep() {return(0);}// should be overridden
  char *getSource() {return(source);}

  virtual NodeTags getTag() { return ScorePageTag; }

  //////////////////////////////////////////////////////
  // BEGIN SCHEDULER VARIABLES
  //////////////////////////////////////////////////////

  // stores the pointers to the memory segment block for the config and
  // state and FIFOs.
  // FIX ME! REALLY SHOULD ALLOW MULTIPLE CACHING OF CONFIG AND STATE!
  // FOR NOW, WE ASSUME THAT EACH PAGE IS STORED IN A CMB ONLY WHEN IT
  // IS RESIDENT (ONLY IN ONE PLACE), AND IS UNCACHED WHEN PAGE IS UNLOADED!
  // dumpSegmentBlock is there so that a page still knows where to dump its
  //   resident state/fifo even if it is getting evicted.
  ScoreSegmentBlock *sched_cachedSegmentBlock;
  ScoreSegmentBlock *sched_dumpSegmentBlock;

  // provides a place to store dumped FIFO information.
  void *sched_fifoBuffer;
  char sched_isFIFOBufferValid;

  // stores the last known state of the page.
  int sched_lastKnownState;

  //////////////////////////////////////////////////////
  // END SCHEDULER VARIABLES  
  //////////////////////////////////////////////////////

  
 protected:
  int *bits;
  int grp;
  int loc;
  int states;
  ScoreIOMaskType *produces;
  ScoreIOMaskType *consumes;
 public:
  char *source;
 protected:
  int *input_rates;
  int *output_rates;
  int state;

};


// needed by LEDA for use with lists/etc.
int compare(ScorePage * const & left, ScorePage * const & right);

#endif
