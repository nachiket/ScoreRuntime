// cctdfc autocompiled header file
// tdfc version 1.160
// Sun Sep 27 19:18:33 2009

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

nonfunc_add8::nonfunc_add8(UNSIGNED_SCORE_STREAM n_cc_a,UNSIGNED_SCORE_STREAM n_cc_b)
{
  int *params=(int *)malloc(0*sizeof(int));
  addInstance(instances,params);
  char * name=mangle(add8_name,0,params);
  char * instance_fn=resolve(name);
  if (instance_fn!=(char *)NULL) {

	// Nachiket add
	cout << "Array Simulator Invoked" << endl;

    long slen;
    long alen;
    long blen;
    add8_arg *data;
    struct msgbuf *msgp;
    data=(add8_arg *)malloc(sizeof(add8_arg));
  result=NEW_UNSIGNED_SCORE_STREAM(9);
    data->i0=STREAM_OBJ_TO_ID(result);
    data->i1=STREAM_OBJ_TO_ID(n_cc_a);
    data->i2=STREAM_OBJ_TO_ID(n_cc_b);
    alen=sizeof(add8_arg);
    slen=strlen(instance_fn);
    blen=sizeof(long)+sizeof(long)+slen+alen;
    msgp=(struct msgbuf *)malloc(sizeof(msgbuf)+sizeof(char)*(blen-1));
    int sid=schedulerId();
    memcpy(msgp->mtext,&alen,sizeof(long));
    memcpy(msgp->mtext+sizeof(long),&slen,sizeof(long));
    memcpy(msgp->mtext+sizeof(long)*2,instance_fn,slen);
    memcpy(msgp->mtext+sizeof(long)*2+slen,data,alen);
    msgp->mtype=SCORE_INSTANTIATE_MESSAGE_TYPE;
    int res=msgsnd(sid, msgp, blen, 0) ;  
    if (res==-1) {
       cerr <<"nonfunc_add8 msgsnd call failed with errno=" << errno << endl;
       exit(2);    }
  }
  else {
	// Nachiket add
	cout << "Created a thread for operator" << endl;

  result=NEW_UNSIGNED_SCORE_STREAM(9);
    declareIO(2,1);
    bindOutput(0,result,new ScoreStreamType(0,9));
    bindInput(0,n_cc_a,new ScoreStreamType(0,8));
    SCORE_MARKREADSTREAM(n_cc_a,globalCounter->threadCounter);
    bindInput(1,n_cc_b,new ScoreStreamType(0,8));
    SCORE_MARKREADSTREAM(n_cc_b,globalCounter->threadCounter);
    pthread_attr_t *a_thread_attribute=(pthread_attr_t *)malloc(sizeof(pthread_attr_t));
    pthread_attr_init(a_thread_attribute);
    pthread_attr_setdetachstate(a_thread_attribute,PTHREAD_CREATE_DETACHED);
    pthread_create(&rpt,a_thread_attribute,&add8_proc_run, this);
  }
}

void *nonfunc_add8::proc_run() {
	// Nachiket add
	cout << "proc_run invoked" << endl;
  enum state_syms {STATE_only};
  state_syms state=STATE_only;
  unsigned long cc_a;
  int retime_length_0=0;
  unsigned long *cc_a_retime=new unsigned long [retime_length_0+1];
  for (int j=retime_length_0;j>=0;j--)
    cc_a_retime[j]=0;
  unsigned long cc_b;
  int retime_length_1=0;
  unsigned long *cc_b_retime=new unsigned long [retime_length_1+1];
  for (int j=retime_length_1;j>=0;j--)
    cc_b_retime[j]=0;
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
        int eos_1=STREAM_EOS(in[1]);
        if (1&&!eos_0&&!eos_1) {
          cc_a=STREAM_READ_NOACC(in[0]);
          for (int j=retime_length_0;j>0;j--)
            cc_a_retime[j]=cc_a_retime[j-1];
          cc_a_retime[0]=cc_a;
          cc_b=STREAM_READ_NOACC(in[1]);
          for (int j=retime_length_1;j>0;j--)
            cc_b_retime[j]=cc_b_retime[j-1];
          cc_b_retime[0]=cc_b;
//	  cout << "We recognize input0=" << cc_a_retime[0] << " and input1=" << cc_b_retime[0] << endl;
          STREAM_WRITE_NOACC(out[0],(cc_a_retime[0]+cc_b_retime[0]));
        }
        else
         done=1;
        }  }
  STREAM_CLOSE(out[0]);
  STREAM_FREE(in[0]);
  STREAM_FREE(in[1]);
  return((void*)NULL); }

UNSIGNED_SCORE_STREAM add8(UNSIGNED_SCORE_STREAM cc_a,UNSIGNED_SCORE_STREAM cc_b) {
   nonfunc_add8 *res=new nonfunc_add8(cc_a,cc_b);
 return(res->getResult()); }
#undef NEW_nonfunc_add8
extern "C" void *NEW_nonfunc_add8(UNSIGNED_SCORE_STREAM i0,UNSIGNED_SCORE_STREAM i1) {
  return((void *) (new nonfunc_add8( i0, i1)));
}
