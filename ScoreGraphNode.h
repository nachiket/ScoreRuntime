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
// $Revision: 1.35 $
//
//////////////////////////////////////////////////////////////////////////////
#ifndef _ScoreGraphNode_H
#define _ScoreGraphNode_H

#include "LEDA/graph/graph.h"
#include "LEDA/core/list.h"
#include "ScoreStreamType.h"
#include "ScoreStream.h"
#include <assert.h>

using leda::node;
using leda::list_item;

#if GET_FEEDBACK
class ScoreFeedbackGraphNode;
#endif

typedef enum {
  ScoreGraphNodeTag,
  ScoreOperatorTag,
  ScorePageTag,
  ScoreProcessorNodeTag,
  ScoreSegmentTag,
  ScoreSegmentStitchTag
} NodeTags;

// the definition of the type for the mask of IOs produced or consumed
typedef unsigned long long ScoreIOMaskType;

// prototypes to avoid circular includes.
class ScoreProcess;
class ScoreCluster;
class ScoreOperatorInstance;


class ScoreGraphNode {

public: 
  ScoreGraphNode();
  virtual ~ScoreGraphNode();

  int getInputs() {return(inputs);}
  int getOutputs() {return(outputs);}
  SCORE_STREAM getInput(int which) { return(in[which]); }
  SCORE_STREAM getOutput(int which) { return(out[which]); }
  SCORE_STREAM getSchedInput(int which) { return(sched_in[which]); }
  SCORE_STREAM getSchedOutput(int which) { return(sched_out[which]); }
  ScoreStreamType *inputType(int which) { return(in_types[which]); }
  ScoreStreamType *outputType(int which) { return(out_types[which]); }
  ScoreStreamType *schedInputType(int which) { return(sched_in_types[which]); }
  ScoreStreamType *schedOutputType(int which) { return(sched_out_types[which]); }
  
  void declareIO(int new_inputs, int new_outputs) {
    int i;

    inputs=new_inputs;
    if (inputs > 0) {
      in=new SCORE_STREAM[inputs];
      sched_in=new SCORE_STREAM[inputs];
      inConsumption=new unsigned int[inputs];
      // Nachiket: gcc4 rules
      in_types=new ScoreStreamType*[inputs];
      sched_in_types=new ScoreStreamType*[inputs];
      sched_dependentOnInputBuffer=new char[inputs];
      sched_inputConsumption=new unsigned int[inputs];
      sched_inputConsumptionOffset=new int[inputs];
      sched_lastKnownInputFIFONumTokens=new unsigned int[inputs];
      sched_expectedInputConsumption=new unsigned int[inputs];
      for (i = 0; i < inputs; i++) {
	in[i] = NULL;
	sched_in[i] = NULL;
	inConsumption[i] = 0;
	in_types[i] = NULL;
	sched_in_types[i] = NULL;
	sched_dependentOnInputBuffer[i] = 0;
	sched_inputConsumption[i] = 0;
	sched_inputConsumptionOffset[i] = 0;
	sched_lastKnownInputFIFONumTokens[i] = 0;
	sched_expectedInputConsumption[i] = 0;
      }
    } else {
      in=NULL;
      sched_in=NULL;
      inConsumption=NULL;
      in_types=NULL;
      sched_in_types=NULL;
      sched_dependentOnInputBuffer=NULL;
      sched_inputConsumption=NULL;
      sched_inputConsumptionOffset=NULL;
      sched_lastKnownInputFIFONumTokens=NULL;
      sched_expectedInputConsumption=NULL;
    }

    outputs=new_outputs;
    if (outputs > 0) {
      out=new SCORE_STREAM[outputs];
      sched_out=new SCORE_STREAM[outputs];
      outProduction=new unsigned int[outputs];
      out_types=new ScoreStreamType*[outputs];
      sched_out_types=new ScoreStreamType*[outputs];
      sched_dependentOnOutputBuffer=new char[outputs];
      sched_outputProduction=new unsigned int[outputs];
      sched_outputProductionOffset=new int[outputs];
      sched_expectedOutputProduction=new unsigned int[outputs];
      for (i = 0; i < outputs; i++) {
	out[i] = NULL;
	sched_out[i] = NULL;
	outProduction[i] = 0;
	out_types[i] = NULL;
	sched_out_types[i] = NULL;
	sched_dependentOnOutputBuffer[i] = 0;
	sched_outputProduction[i] = 0;
	sched_outputProductionOffset[i] = 0;
	sched_expectedOutputProduction[i] = 0;
      }
    } else {
      out=NULL;
      sched_out=NULL;
      outProduction=NULL;
      out_types=NULL;
      sched_out_types=NULL;
      sched_dependentOnOutputBuffer=NULL;
      sched_outputProduction=NULL;
      sched_outputProductionOffset=NULL;
      sched_expectedOutputProduction=NULL;
    }
  }

  void bindInput(int which, SCORE_STREAM strm,
		 ScoreStreamType *stype) {
    in[which]=strm;
    sched_in[which]=strm;
    strm->snkNum = which; // input of a graph node is a stram sink
    strm->sched_snkNum = which; // input of a graph node is a stram sink
    in_types[which]=stype;
    sched_in_types[which]=stype;
    if (_isPage) {
      STREAM_BIND_SINK(strm,this,stype,STREAM_PAGE_TYPE);
    } else if (_isSegment) {
      STREAM_BIND_SINK(strm,this,stype,STREAM_SEGMENT_TYPE);
    } else if (_isOperator) {
      STREAM_BIND_SINK(strm,this,stype,STREAM_OPERATOR_TYPE);
    }
  }

  void bindOutput(int which, SCORE_STREAM strm,
		  ScoreStreamType *stype) {
    out[which]=strm;
    sched_out[which]=strm;
    strm->srcNum = which; // output of a graph node is a stream source
    strm->sched_srcNum = which; // output of a graph node is a stream source
    out_types[which]=stype;
    sched_out_types[which]=stype;
    if (_isPage) {
      STREAM_BIND_SRC(strm,this,stype,STREAM_PAGE_TYPE);
    } else if (_isSegment) {
      STREAM_BIND_SRC(strm,this,stype,STREAM_SEGMENT_TYPE);
    } else if (_isOperator) {
      STREAM_BIND_SRC(strm,this,stype,STREAM_OPERATOR_TYPE);
    }
  }

  // this is used by the scheduler when building a processor node which does
  // not really want to have streams point to it.
  void bindInput_localbind(int which, SCORE_STREAM strm) {
    in[which]=strm;
    sched_in[which]=strm;
  }

  // this is used by the scheduler when building a processor node which does
  // not really want to have streams point to it.
  void bindOutput_localbind(int which, SCORE_STREAM strm) {
    out[which]=strm;
    sched_out[which]=strm;
  }

  void unbindInput(int which) {
    STREAM_UNBIND_SINK(in[which]);
    in[which]->snkNum = -1; 
    sched_in[which]->sched_snkNum = -1; 
    in[which]=NULL;
    sched_in[which]=NULL;
    in_types[which]=NULL;
    sched_in_types[which]=NULL;
  }

  void unbindOutput(int which) {
    STREAM_UNBIND_SRC(out[which]);
    out[which]->srcNum = -1; 
    sched_out[which]->srcNum = -1; 
    out[which]=NULL;
    sched_out[which]=NULL;
    out_types[which]=NULL;
    sched_out_types[which]=NULL;
  }

  void bindSchedInput(int which, SCORE_STREAM strm,
		      ScoreStreamType *stype) {
    sched_in[which]=strm;
    strm->sched_snkNum = which; // input of a graph node is a stram sink
    sched_in_types[which]=stype;
    if (_isPage) {
      STREAM_SCHED_BIND_SINK(strm,this,stype,STREAM_PAGE_TYPE);
    } else if (_isSegment) {
      STREAM_SCHED_BIND_SINK(strm,this,stype,STREAM_SEGMENT_TYPE);
    } else if (_isOperator) {
      STREAM_SCHED_BIND_SINK(strm,this,stype,STREAM_OPERATOR_TYPE);
    }
  }

  void bindSchedOutput(int which, SCORE_STREAM strm,
		       ScoreStreamType *stype) {
    sched_out[which]=strm;
    strm->sched_srcNum = which; // output of a graph node is a stream source
    sched_out_types[which]=stype;
    if (_isPage) {
      STREAM_SCHED_BIND_SRC(strm,this,stype,STREAM_PAGE_TYPE);
    } else if (_isSegment) {
      STREAM_SCHED_BIND_SRC(strm,this,stype,STREAM_SEGMENT_TYPE);
    } else if (_isOperator) {
      STREAM_SCHED_BIND_SRC(strm,this,stype,STREAM_OPERATOR_TYPE);
    }
  }

  // this is used by the scheduler when building a processor node which does
  // not really want to have streams point to it.
  void bindSchedInput_localbind(int which, SCORE_STREAM strm) {
    sched_in[which]=strm;
  }

  // this is used by the scheduler when building a processor node which does
  // not really want to have streams point to it.
  void bindSchedOutput_localbind(int which, SCORE_STREAM strm) {
    sched_out[which]=strm;
  }

  void unbindSchedInput(int which) {
    STREAM_SCHED_UNBIND_SINK(sched_in[which]);
    sched_in[which]->sched_snkNum = -1; 
    sched_in[which]=NULL;
    sched_in_types[which]=NULL;
  }

  void unbindSchedOutput(int which) {
    STREAM_SCHED_UNBIND_SRC(sched_out[which]);
    sched_out[which]->sched_srcNum = -1; 
    sched_out[which]=NULL;
    sched_out_types[which]=NULL;
  }

  int getFire() {return(fire);}
  int getStall() {return(stall);}
  void setStall(int stall_t) {stall=stall_t;}

  int getDoneCount() { return (doneCount); }
  void incrementInputConsumption(int which) {inConsumption[which]++;}
  void incrementOutputProduction(int which) {outProduction[which]++;}
  unsigned int getInputConsumption(int which) {return(inConsumption[which]);}
  unsigned int getOutputProduction(int which) {return(outProduction[which]);}
  int isPage() {return(_isPage);}
  int isOperator() {return(_isOperator);}
  int isSegment() {return(_isSegment);}

  virtual NodeTags getTag() { return ScoreGraphNodeTag; }

  virtual void print(FILE *f, bool printShort = false);

  void recalculateUnstallTime();
  char isStalledOnInput();

  void syncSchedToReal();

  unsigned long long sim_unstallTime;
  char sim_unstallTimeIsInfinity;

  int _isPage, _isSegment, _isOperator;

  //////////////////////////////////////////////////////
  // BEGIN SCHEDULER VARIABLES
  //////////////////////////////////////////////////////

  // stores what status the node is in.
  char sched_isDone;
  char sched_isResident;
  char sched_isScheduled;

  // stores the location where the node is resident (only valid when
  // this node is resident).
  unsigned int sched_residentLoc;

  // stores a pointer to the parent process.
  ScoreProcess *sched_parentProcess;

  // stores a pointer to the parent cluster.
  ScoreCluster *sched_parentCluster;

  // stores a pointer to the parent operator.
  ScoreOperatorInstance *sched_parentOperator;

  // used by scheduler for clustering and graph operations.
  node sched_graphNode;

  // used by graph algorithms to mark this node visited.
  char sched_visited;
  char sched_visited2;

  // used by scheduler for bufferlock detection.
  char *sched_dependentOnInputBuffer;
  char *sched_dependentOnOutputBuffer;

  // stores the scheduler-known input/output consumption/production rates.
  unsigned int *sched_inputConsumption;
  unsigned int *sched_outputProduction;

  // offsets so that the scheduler can effectively reset the rates.
  // NOTE: This will be automatically applied by the scheduler.
  int *sched_inputConsumptionOffset;
  int *sched_outputProductionOffset;

  // stores the last known input number of tokens.
  unsigned int *sched_lastKnownInputFIFONumTokens;

  // stores the expected input/output consumption/production amount.
  unsigned int *sched_expectedInputConsumption;
  unsigned int *sched_expectedOutputProduction;

  SCORE_STREAM *sched_in;
  SCORE_STREAM *sched_out;
  ScoreStreamType **sched_in_types;
  ScoreStreamType **sched_out_types;

  // stores whether or not this node did not fire in its last timeslice
  // resident.
  // FIX ME! PERHAPS WE WOULD WANT TO CONSIDER ADDING SOME HYSTERISIS
  //         WITH A COUNTER THAT ALLOWS A PAGE TO NOT CONSUME/PRODUCE
  //         FOR MORE THAN 1 TIMESLICE BEFORE WE CONSIDER IT STALLED.
  //         (WE WOULD UP THE TRIGGER NUMBER AFTER WE RUN A DEADLOCK DETECTION
  //          AND DETERMINE, BY EXAMINING STATE, THAT IT IS STILL "ALIVE").
  char sched_potentiallyDidNotFireLastResident;

#ifdef RANDOM_SCHEDULER
  // contains the timeslice when this node was scheduled last
  // i.e. if (sched_lastTimesliceScheduled == currentTimeslice) then
  //            this node was selected to be scheduled during this timeslice
 private:
  unsigned int sched_lastTimesliceScheduled;
 public:
  void setLastTimesliceScheduled(unsigned int val) {
    sched_lastTimesliceScheduled = val;
  }

  unsigned int getLastTimesliceScheduled() const { 
    return sched_lastTimesliceScheduled;
  }
#endif

  //////////////////////////////////////////////////////
  // END SCHEDULER VARIABLES  
  //////////////////////////////////////////////////////

  list_item sim_runnableNodeListItem;
#if 0
  pq_item sim_stalledNodeQueueItem;
#endif
  list_item sim_checkNodeListItem;
  
#if GET_FEEDBACK
  unsigned int *getConsumptionVector() { return inConsumption; }
  unsigned int *getProductionVector() { return outProduction; }

  ScoreFeedbackGraphNode *feedbackNode;
  size_t uniqTag;
  
  // this is where statistical information will be placed
  unsigned int *stat_consumption;
  unsigned int *stat_production;
  unsigned int stat_fireCount;
#endif

protected:
  int inputs; 
  int outputs;
  SCORE_STREAM *in;
  SCORE_STREAM *out;
  unsigned int *inConsumption;
  unsigned int *outProduction;
  ScoreStreamType **in_types;
  ScoreStreamType **out_types;  
  int fire;
  int stall;
  int doneCount;
};


// needed by LEDA for use with lists/etc.
int compare(ScoreGraphNode * const & left, ScoreGraphNode * const & right);

typedef enum _feedbackMode { NOTHING, MAKEFEEDBACK, READFEEDBACK, SAMPLERATES } ScoreFeedbackMode;

#endif
