// cctdfc autocompiled header file
// tdfc version 1.160
// Sun Sep 27 19:18:33 2009

#include "Score.h"
#include <pthread.h>
#ifdef __cplusplus
typedef struct {
   int i0;
   int i1;
   int i2;
} add8_arg;

class nonfunc_add8: public ScoreOperator {
public: 
  nonfunc_add8(UNSIGNED_SCORE_STREAM i0,UNSIGNED_SCORE_STREAM i1);
  UNSIGNED_SCORE_STREAM getResult() { return result;}
  void *proc_run();
private: 
  pthread_t rpt;
  UNSIGNED_SCORE_STREAM result;
  static ScoreOperatorElement *instances;
};
UNSIGNED_SCORE_STREAM add8(UNSIGNED_SCORE_STREAM cc_a,UNSIGNED_SCORE_STREAM cc_b);
typedef nonfunc_add8* OPERATOR_nonfunc_add8;
#define NEW_nonfunc_add8 new nonfunc_add8
#else
typedef void* OPERATOR_nonfunc_add8;
void *NEW_nonfunc_add8(UNSIGNED_SCORE_STREAM i0,UNSIGNED_SCORE_STREAM i1);
#endif
