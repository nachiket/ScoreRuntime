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
// $Revision: 1.20 $
//
//////////////////////////////////////////////////////////////////////////////
#ifndef _ScoreSegmentStitch_H

#define _ScoreSegmentStitch_H

#include <unistd.h>
#include "ScoreSegment.h"



class ScoreSegmentStitch : public ScoreSegment {
 public:
  void *operator new(size_t size);
  ScoreSegmentStitch(int nlength, int nwidth, SCORE_STREAM in, 
		     SCORE_STREAM out);
  virtual ~ScoreSegmentStitch();

  void reset() {
    int i;

    sched_stitchBufferNum = 0;
    sched_prevStitch = NULL;
    sched_nextStitch = NULL;
    hasReceivedEOS = 0;
    sched_isStitch = 1;
    sched_isNewStitch = 1;
    sched_isOnlyFeedProcessor = 0;

    traAddr = 0;
    pboAddr = 0;
    readAddr = 0;
    writeAddr = 0;

    sched_traAddr = 0;
    sched_pboAddr = 0;
    sched_readAddr = 0;
    sched_writeAddr = 0;

    sched_cachedSegmentBlock = NULL;
    sched_dumpSegmentBlock = NULL;
    sched_fifoBuffer = NULL;
    sched_isFIFOBufferValid = 0;
    sched_isFaulted = 0;
    sched_faultedAddr = 0;
    sim_isFaulted = 0;
    sim_faultedAddr = 0;
    sched_dumpOnDone = 0;

    readCount = 0;
    writeCount = 0;

    sched_readCount = 0;
    sched_writeCount = 0;

    sched_isDone = 0;
    sched_isResident = 0;
    sched_isScheduled = 0;
    sched_residentLoc = 0;

    mode = SCORE_CMB_SEQSRCSINK;
    sched_mode = SCORE_CMB_SEQSRCSINK;

    for (i = 0; i < inputs; i++) {
      inConsumption[i] = 0;
      sched_dependentOnInputBuffer[i] = 0;
      sched_inputConsumption[i] = 0;
      sched_inputConsumptionOffset[i] = 0;
      sched_lastKnownInputFIFONumTokens[i] = 0;
      sched_expectedInputConsumption[i] = 0;
    }
    for (i = 0; i < outputs; i++) {
      outProduction[i] = 0;
      sched_dependentOnOutputBuffer[i] = 0;
      sched_outputProduction[i] = 0;
      sched_outputProductionOffset[i] = 0;
      sched_expectedOutputProduction[i] = 0;
    }

    sim_unstallTime = 0;
    sim_unstallTimeIsInfinity = 0;
    sim_runnableNodeListItem = NULL; // nil_item
#if 0
    sim_stalledNodeQueueItem = nil_item;
#endif
    sim_checkNodeListItem = NULL; //nil_item;

    // set the input mask.
    sim_segmentInputMask = 0;

    this_segment_is_done = 0;
    sched_this_segment_is_done = 0;

    input_freed = 0;
    output_closed = 0;

    shouldUseUSECOUNT = 0;

    sched_residentStart = 0;
    sched_residentLength = 0;

    sched_potentiallyDidNotFireLastResident = 0;

    sched_mustBeInDataFlow = 0;
    sched_isEmptyAndWillBeRemoved = 0;

    sched_parentCluster = 0;

#ifdef RANDOM_SCHEDULER
    sched_lastTimesliceConfigured = (unsigned int)(-2);
    sched_residentStitchListItem = SCORECUSTOMLINKEDLIST_NULL;
    setLastTimesliceScheduled((unsigned int)(-2));
#endif
  }

  void recycle(int nlength, int nwidth, 
	       ScoreStream* in, ScoreStream* out) {
    // FIX ME! REALLY SHOULD PAY ATTENTION TO NLENGTH AND NWIDTH IN CASE THEY
    //         CHANGED!

    segLength = nlength;
    segWidth = nwidth;
    segSize = segLength * 8; 
    // make everything long long integer sized for now.

    if (in != NULL) {
      bindSchedInput(SCORE_CMB_STITCH_DATAW_INNUM,
		     in, in_types[SCORE_CMB_STITCH_DATAW_INNUM]);
    }
    if (out != NULL) {
      bindSchedOutput(SCORE_CMB_STITCH_DATAR_OUTNUM,
		      out, out_types[SCORE_CMB_STITCH_DATAR_OUTNUM]);
    }

    maxAddr = nlength;
    sched_maxAddr = nlength;

    sched_parentCluster = 0;

#ifdef RANDOM_SCHEDULER
    sched_lastTimesliceConfigured = (unsigned int)(-2);
    sched_residentStitchListItem = SCORECUSTOMLINKEDLIST_NULL;
    setLastTimesliceScheduled((unsigned int)(-2));
#endif
  }

  virtual int step();
  ScoreStream *getInStream() {return in[0];}
  ScoreStream *getOutStream() {return out[0];}
  ScoreStream *getSchedInStream() {return sched_in[0];}
  ScoreStream *getSchedOutStream() {return sched_out[0];}

  // this is used if a stitch CMB is in SEQSINK mode and receives an EOS
  // token. when it become a SEQSRC and the CMB is emptied, then the
  // EOS will be forwarded.
  char hasReceivedEOS;

  virtual NodeTags getTag() { return ScoreSegmentStitchTag; }

  //////////////////////////////////////////////////////
  // BEGIN SCHEDULER VARIABLES
  //////////////////////////////////////////////////////

  // if this is a stitch buffer, these pointers store pointers to the previous
  // and next stitch buffers in a stitch buffer chain.
  unsigned int sched_stitchBufferNum;
  ScoreSegment *sched_prevStitch, *sched_nextStitch;

  // the first time a stitch buffer is instantiated, this flags that the
  // stitch buffer is new.
  char sched_isNewStitch;

  // this indicates whether or not this stitch buffer is only feeding the
  // processor (i.e. its source is done, but sink is the processor).
  char sched_isOnlyFeedProcessor;

  // this indicates whether or not this stitch buffer must always be in the
  // dataflow graph.
  char sched_mustBeInDataFlow;

  // this is used to indicate that this stitch buffer is empty and will be
  // removed from the dataflow graph.
  char sched_isEmptyAndWillBeRemoved;

#ifdef RANDOM_SCHEDULER
  unsigned int sched_lastTimesliceConfigured;
  SCORECUSTOMLINKEDLISTITEM sched_residentStitchListItem;
#endif

  //////////////////////////////////////////////////////
  // END SCHEDULER VARIABLES  
  //////////////////////////////////////////////////////

    char input_freed, output_closed;
 private:

};

// needed by LEDA for use with lists/etc.
int compare(ScoreSegmentStitch * const & left, 
            ScoreSegmentStitch * const & right);

#endif

