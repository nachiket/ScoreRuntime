// cctdfc autocompiled header file
// tdfc version 1.160
// Mon Dec 14 04:25:22 2009

#include "Score.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include "add8.h"
char * add8_name="add8";
void * add8_proc_run(void *obj) {
  return(((nonfunc_add8 *)obj)->proc_run()); }
ScoreOperatorElement *nonfunc_add8init_instances() {
  return(ScoreOperator::addOperator(add8_name,0,2,0));  }
ScoreOperatorElement *nonfunc_add8::instances=nonfunc_add8init_instances();

nonfunc_add8::nonfunc_add8(UNSIGNED_SCORE_STREAM n_a,UNSIGNED_SCORE_STREAM n_b)
{
  int *params=(int *)malloc(0*sizeof(int));
  addInstance(instances,params);
  char * name=mangle(add8_name,0,params);
  char * instance_fn=resolve(name);
  assert (instance_fn==(char *)NULL);
  result=NEW_UNSIGNED_SCORE_STREAM(9);
    declareIO(2,1);
    bindOutput(0,result,new ScoreStreamType(0,9));
    bindInput(0,n_a,new ScoreStreamType(0,8));
    SCORE_MARKREADSTREAM(n_a,globalCounter->threadCounter);
    bindInput(1,n_b,new ScoreStreamType(0,8));
    SCORE_MARKREADSTREAM(n_b,globalCounter->threadCounter);
    pthread_attr_t *a_thread_attribute=(pthread_attr_t *)malloc(sizeof(pthread_attr_t));
    pthread_attr_init(a_thread_attribute);
    pthread_attr_setdetachstate(a_thread_attribute,PTHREAD_CREATE_DETACHED);
    pthread_create(&rpt,a_thread_attribute,&add8_proc_run, this);
}

void *nonfunc_add8::proc_run() {
  enum state_syms {STATE_only};
  state_syms state=STATE_only;
  unsigned long a;
    a=0;
  unsigned long b;
    b=0;
  int *input_free=new int[2];
  for (int i=0;i<2;i++)
    input_free[i]=0;
  int *output_close=new int[1];
  for (int i=0;i<1;i++)
    output_close[i]=0;
  int done=0;
  while (!done) {
        {
        int eos_0=STREAM_EOS(in[0]);
        int eofr_0=STREAM_EOFR(in[0]);
        int eos_1=STREAM_EOS(in[1]);
        int eofr_1=STREAM_EOFR(in[1]);
        if (1&&!eos_0&&!eofr_0&&!eos_1&&!eofr_1) {
          a=STREAM_READ_NOACC(in[0]);
          b=STREAM_READ_NOACC(in[1]);
