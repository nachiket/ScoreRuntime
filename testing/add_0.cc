// cctdfc autocompiled instance file
// tdfc version 1.160
// Sun Sep 27 19:18:33 2009

#include "Score.h"
#include <stdlib.h>
#include <iostream>
#include <string.h>
#include "add.h"
enum add_0_state_syms {add_0_state_only=0,add_0_state_this_page_is_done};
extern ScoreIOMaskType add_0_consumes[];
extern ScoreIOMaskType add_0_produces[];
class Page_add_0: public ScorePage {
public:
  Page_add_0(UNSIGNED_SCORE_STREAM n_result,UNSIGNED_SCORE_STREAM n_cc_a,UNSIGNED_SCORE_STREAM n_cc_b) {
    retime_length_0=0;
    cc_a_retime=new unsigned long long [retime_length_0+1];
    for (int j=retime_length_0;j>=0;j--)
      cc_a_retime[j]=0;
    retime_length_1=0;
    cc_b_retime=new unsigned long long [retime_length_1+1];
    for (int j=retime_length_1;j>=0;j--)
      cc_b_retime[j]=0;
    declareIO(2,1);
    bindOutput(0,n_result,new ScoreStreamType(0,0));
    bindInput(0,n_cc_a,new ScoreStreamType(0,0));
    bindInput(1,n_cc_b,new ScoreStreamType(0,0));
    state=add_0_state_only;
    states=2;
    produces=add_0_produces;
    consumes=add_0_consumes;
    source="add in add.tdf";
    // cc_n = 0
    loc=NO_LOCATION;
    input_rates=new int[2];
    output_rates=new int[1];
    input_rates[0]=-1;
    input_rates[1]=-1;
    output_rates[0]=-1;
    input_free=new int[2];
    for (int i=0;i<2;i++)
      input_free[i]=0;
    output_close=new int[1];
    for (int i=0;i<1;i++)
      output_close[i]=0;
  } // constructor 
  int pagestep() { 
    unsigned long long cc_a;
    unsigned long long cc_b;
    int done=0;
    int canfire=1;
    switch(state) {
      case add_0_state_only: { 
        {
        int data_0=STREAM_DATA_ARRAY(in[0]);
        int eos_0=0;
        if (data_0) eos_0=STREAM_EOS_ARRAY(in[0]);
        int data_1=STREAM_DATA_ARRAY(in[1]);
        int eos_1=0;
        if (data_1) eos_1=STREAM_EOS_ARRAY(in[1]);
        if (1 && data_0 && !eos_0 && data_1 && !eos_1) {
          if (1 && !STREAM_FULL_ARRAY(out[0])) {
            cc_a=STREAM_READ_ARRAY(in[0]);
            for (int j=retime_length_0;j>0;j--)
              cc_a_retime[j]=cc_a_retime[j-1];
            cc_a_retime[0]=cc_a;
            cc_b=STREAM_READ_ARRAY(in[1]);
            for (int j=retime_length_1;j>0;j--)
              cc_b_retime[j]=cc_b_retime[j-1];
            cc_b_retime[0]=cc_b;
            STREAM_WRITE_ARRAY(out[0],(cc_a_retime[0]+cc_b_retime[0]));
          }
        }
        else
         if (0 || (data_0 && eos_0) || (data_1 && eos_1)) done=1; else canfire=0;
        }
        break; 
      } // end case add_0_state_only
      case add_0_state_this_page_is_done: {
        doneCount++;
        return(0);
      } // end case add_0_state_this_page_is_done
      default: cerr << "ERROR unknown state [" << state << "] encountered in add_0::pagestep" << endl;
        abort();
    }
    if (canfire) fire++; else stall++;
    if (done) {
      STREAM_CLOSE(out[0]);
      STREAM_FREE(in[0]);
      STREAM_FREE(in[1]);
      state=add_0_state_this_page_is_done;
      return(0);
    }
    else return(1);
  } // pagestep
private:
  int retime_length_0;
  unsigned long long *cc_a_retime;
  int retime_length_1;
  unsigned long long *cc_b_retime;
  int *input_free;
  int *output_close;
};
ScoreIOMaskType add_0_consumes[]={3,0};
ScoreIOMaskType add_0_produces[]={1,0};
class add_0: public ScoreOperatorInstance {
public:
  add_0(UNSIGNED_SCORE_STREAM add_0,UNSIGNED_SCORE_STREAM cc_a,UNSIGNED_SCORE_STREAM cc_b) {
    pages=1;
    segments=0;
    page=new (ScorePage *)[1];
    segment=new (ScoreSegment *)[0];
    page_group=new int[1];
    segment_group=new int[0];
    ScorePage *add_0_inst=new Page_add_0(add_0,cc_a,cc_b);
    page[0]=add_0_inst;
    page_group[0]=NO_GROUP;
  }
}; // add_0
extern "C" ScoreOperatorInstance *construct(char *argbuf) {
  add_arg *data;
  data=(add_arg *)malloc(sizeof(add_arg));
  memcpy(data,argbuf,sizeof(add_arg));
  return(new add_0(((UNSIGNED_SCORE_STREAM)STREAM_ID_TO_OBJ(data->i0)),((UNSIGNED_SCORE_STREAM)STREAM_ID_TO_OBJ(data->i1)),((UNSIGNED_SCORE_STREAM)STREAM_ID_TO_OBJ(data->i2))));
}
