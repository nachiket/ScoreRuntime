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
// SCORE Segment Operator (Write-only)
// $Revision: 1.4 $
//
//////////////////////////////////////////////////////////////////////////////
#ifndef _ScoreSegmentOperatorWriteOnly_H

#define _ScoreSegmentOperatorWriteOnly_H

#include <unistd.h>
#include <pthread.h>


#ifdef __cplusplus

#include "ScoreOperator.h"
#include "ScoreSegment.h"
#include "ScoreStream.h"
#include "ScoreConfig.h"


typedef struct {
  int segPtrID;
  int addrID;
  int dataID;
} ScoreSegmentOperatorWriteOnly_arg;


class ScoreSegmentOperatorWriteOnly : public ScoreOperator {
 public:
  ScoreSegmentOperatorWriteOnly(unsigned int dwidth, unsigned int awidth, 
				size_t nelems,
				UNSIGNED_SCORE_SEGMENT segPtr,
				UNSIGNED_SCORE_STREAM addr,
				UNSIGNED_SCORE_STREAM data);

  ScoreSegmentOperatorWriteOnly(unsigned int dwidth, unsigned int awidth, 
				size_t nelems,
				SIGNED_SCORE_SEGMENT segPtr,
				UNSIGNED_SCORE_STREAM addr,
				SIGNED_SCORE_STREAM data);

  ScoreSegmentOperatorWriteOnly(unsigned int dwidth, unsigned int awidth, 
				size_t nelems,
				BOOLEAN_SCORE_SEGMENT segPtr,
				UNSIGNED_SCORE_STREAM addr,
				BOOLEAN_SCORE_STREAM data);

  ScoreSegmentOperatorWriteOnly(unsigned int dwidth, unsigned int awidth, 
				size_t nelems,
				SIGNED_FIXED_SCORE_SEGMENT segPtr,
				UNSIGNED_SCORE_STREAM addr,
				SIGNED_FIXED_STREAM data);

  ScoreSegmentOperatorWriteOnly(unsigned int dwidth, unsigned int awidth, 
				size_t nelems,
				UNSIGNED_FIXED_SCORE_SEGMENT segPtr,
				UNSIGNED_SCORE_STREAM addr,
				UNSIGNED_FIXED_STREAM data);

  void *proc_run();


 private:
  void constructorHelper(unsigned int dwidth, unsigned int awidth, 
			 size_t nelems,
			 ScoreSegment *segPtr,
			 ScoreStream *addr,
			 ScoreStream *data);

  ScoreSegment *segment;
  ScoreStream *addrStream, *dataStream;

  pthread_t rpt;
};

typedef ScoreSegmentOperatorWriteOnly* OPERATOR_ScoreSegmentOperatorWriteOnly;
#define NEW_ScoreSegmentOperatorWriteOnly new ScoreSegmentOperatorWriteOnly

#else

typedef void* OPERATOR_ScoreSegmentOperatorWriteOnly;
void *NEW_ScoreSegmentOperatorWriteOnly(unsigned int dwidth, 
					unsigned int awidth, 
					size_t nelems,
					void *segPtr,
					void *addr,
					void *data);

#endif

#endif

