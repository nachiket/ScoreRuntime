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
// SCORE Segment Operator (Sequential Read-only)
// $Revision: 1.5 $
//
//////////////////////////////////////////////////////////////////////////////
#ifndef _ScoreSegmentOperatorSeqReadOnly_H

#define _ScoreSegmentOperatorSeqReadOnly_H

#include <unistd.h>
#include <pthread.h>


#ifdef __cplusplus

#include "ScoreOperator.h"
#include "ScoreSegment.h"
#include "ScoreStream.h"
#include "ScoreConfig.h"


typedef struct {
  int segPtrID;
  int dataID;
} ScoreSegmentOperatorSeqReadOnly_arg;


class ScoreSegmentOperatorSeqReadOnly : public ScoreOperator {
 public:
  ScoreSegmentOperatorSeqReadOnly(unsigned int dwidth, unsigned int awidth, 
				  size_t nelems,
				  UNSIGNED_SCORE_SEGMENT segPtr,
				  UNSIGNED_SCORE_STREAM data);

  ScoreSegmentOperatorSeqReadOnly(unsigned int dwidth, unsigned int awidth, 
				  size_t nelems,
				  SIGNED_SCORE_SEGMENT segPtr,
				  SIGNED_SCORE_STREAM data);

  ScoreSegmentOperatorSeqReadOnly(unsigned int dwidth, unsigned int awidth, 
				  size_t nelems,
				  BOOLEAN_SCORE_SEGMENT segPtr,
				  BOOLEAN_SCORE_STREAM data);

  ScoreSegmentOperatorSeqReadOnly(unsigned int dwidth, unsigned int awidth, 
				  size_t nelems,
				  SIGNED_FIXED_SCORE_SEGMENT segPtr,
				  SIGNED_FIXED_STREAM data);

  ScoreSegmentOperatorSeqReadOnly(unsigned int dwidth, unsigned int awidth, 
				  size_t nelems,
				  UNSIGNED_FIXED_SCORE_SEGMENT segPtr,
				  UNSIGNED_FIXED_STREAM data);

  ScoreSegmentOperatorSeqReadOnly(unsigned int dwidth, unsigned int awidth, 
				  size_t nelems,
				  DOUBLE_SCORE_SEGMENT segPtr,
				  DOUBLE_SCORE_STREAM data);

  void *proc_run();


 private:
  void constructorHelper(unsigned int dwidth, unsigned int awidth, 
			 size_t nelems,
			 ScoreSegment *segPtr,
			 ScoreStream *data);

  ScoreSegment *segment;
  ScoreStream *dataStream;

  pthread_t rpt;
};

typedef ScoreSegmentOperatorSeqReadOnly* \
   OPERATOR_ScoreSegmentOperatorSeqReadOnly;
#define NEW_ScoreSegmentOperatorSeqReadOnly \
   new ScoreSegmentOperatorSeqReadOnly

#else

typedef void* OPERATOR_ScoreSegmentOperatorSeqReadOnly;
void *NEW_ScoreSegmentOperatorSeqReadOnly(unsigned int dwidth, 
					  unsigned int awidth, 
					  size_t nelems,
					  void *segPtr,
					  void *data);

#endif

#endif

