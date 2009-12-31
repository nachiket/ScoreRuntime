#ifndef _ScoreGraphNode_H
#define _ScoreGraphNode_H

#include "xilscorestream.h"
#include "xilscorestreamtype.h"

class ScoreGraphNode {

public: 
  ScoreGraphNode();
  virtual ~ScoreGraphNode();

  int getInputs() {return(inputs);}
  int getOutputs() {return(outputs);}
  SCORE_STREAM getInput(int which) { return(in[which]); }
  SCORE_STREAM getOutput(int which) { return(out[which]); }
  ScoreStreamType *inputType(int which) { return(in_types[which]); }
  ScoreStreamType *outputType(int which) { return(out_types[which]); }
  
  void declareIO(int new_inputs, int new_outputs) {
    int i;

    inputs=new_inputs;
    if (inputs > 0) {
      in=new SCORE_STREAM[inputs];
      in_types=new ScoreStreamType*[inputs];
      for (i = 0; i < inputs; i++) {
	in[i] = NULL;
	in_types[i] = NULL;
      }
    } else {
      in=NULL;
      in_types=NULL;
    }

    outputs=new_outputs;
    if (outputs > 0) {
      out=new SCORE_STREAM[outputs];
      out_types=new ScoreStreamType*[outputs];
      for (i = 0; i < outputs; i++) {
	out[i] = NULL;
	out_types[i] = NULL;
      }
    } else {
      out=NULL;
      out_types=NULL;
    }
  }

  void bindInput(int which, SCORE_STREAM strm, ScoreStreamType *stype) {
    in[which]=strm;
    strm->snkNum = which; // input of a graph node is a stram sink
    in_types[which]=stype;
    STREAM_BIND_SINK(strm,this,stype,STREAM_OPERATOR_TYPE);
  }

  void bindOutput(int which, SCORE_STREAM strm, ScoreStreamType *stype) {
    out[which]=strm;
    strm->srcNum = which; // output of a graph node is a stream source
    out_types[which]=stype;
    STREAM_BIND_SRC(strm,this,stype,STREAM_OPERATOR_TYPE);
  }

  void unbindInput(int which) {
    STREAM_UNBIND_SINK(in[which]);
    in[which]->snkNum = -1; 
    in[which]=NULL;
    in_types[which]=NULL;
  }

  void unbindOutput(int which) {
    STREAM_UNBIND_SRC(out[which]);
    out[which]->srcNum = -1; 
    out[which]=NULL;
    out_types[which]=NULL;
  }

  int getFire() {return(fire);}
  int getStall() {return(stall);}
  void setStall(int stall_t) {stall=stall_t;}

protected:
  int inputs; 
  int outputs;
  SCORE_STREAM *in;
  SCORE_STREAM *out;
  ScoreStreamType **in_types;
  ScoreStreamType **out_types;  
  int fire;
  int stall;
  int doneCount;
};

#endif
