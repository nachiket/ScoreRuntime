// cctdfc autocompiled header file
// tdfc version 1.160
// Mon Sep 28 21:25:14 2009

#include "Score.h"
#include <pthread.h>
#ifdef __cplusplus
typedef struct {
   int i0;
   int i1;
   int i2;
   int i3;
} add_arg;

class nonfunc_add: public ScoreOperator {
public: 
  nonfunc_add(unsigned long i0,UNSIGNED_SCORE_STREAM i1,UNSIGNED_SCORE_STREAM i2,BOOLEAN_SCORE_STREAM i3);
  UNSIGNED_SCORE_STREAM getResult() { return result;}
  void *proc_run();
private: 
  unsigned long cc_n;
  pthread_t rpt;
  UNSIGNED_SCORE_STREAM result;
  static ScoreOperatorElement *instances;
};
UNSIGNED_SCORE_STREAM add(unsigned long cc_n,UNSIGNED_SCORE_STREAM cc_a,UNSIGNED_SCORE_STREAM cc_b,BOOLEAN_SCORE_STREAM cc_c);
typedef nonfunc_add* OPERATOR_nonfunc_add;
#define NEW_nonfunc_add new nonfunc_add
#else
typedef void* OPERATOR_nonfunc_add;
void *NEW_nonfunc_add(unsigned long i0,UNSIGNED_SCORE_STREAM i1,UNSIGNED_SCORE_STREAM i2,BOOLEAN_SCORE_STREAM i3);
#endif
