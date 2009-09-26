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
// $Revision: 1.6 $
//
//////////////////////////////////////////////////////////////////////////////
#ifndef _ScoreSegmentWriteOnly_H

#define _ScoreSegmentWriteOnly_H

#include <unistd.h>
#include "ScoreSegment.h"

class ScoreSegmentWriteOnly : public ScoreSegment {
 public:
  void *operator new(size_t size);

  // typed signature with TYPED_SCORE_SEGMENT

  ScoreSegmentWriteOnly(unsigned int dwidth,unsigned int awidth, 
			unsigned int nelems,
			UNSIGNED_SCORE_SEGMENT segPtr,
			UNSIGNED_SCORE_STREAM addr,
			UNSIGNED_SCORE_STREAM data) {
    constructorHelper(dwidth,awidth,nelems,segPtr,addr,data);
  }

  ScoreSegmentWriteOnly(unsigned int dwidth,unsigned int awidth, 
			unsigned int nelems,
			SIGNED_SCORE_SEGMENT segPtr,
			UNSIGNED_SCORE_STREAM addr,
			SIGNED_SCORE_STREAM data) {
    constructorHelper(dwidth,awidth,nelems,segPtr,addr,data);
  }

  ScoreSegmentWriteOnly(unsigned int dwidth,unsigned int awidth, 
			unsigned int nelems,
			BOOLEAN_SCORE_SEGMENT segPtr,
			UNSIGNED_SCORE_STREAM addr,
			BOOLEAN_SCORE_STREAM data) {
    constructorHelper(dwidth,awidth,nelems,segPtr,addr,data);
  }

  ScoreSegmentWriteOnly(unsigned int dwidth,unsigned int awidth, 
			unsigned int nelems,
			SIGNED_FIXED_SCORE_SEGMENT segPtr,
			UNSIGNED_SCORE_STREAM addr,
			SIGNED_FIXED_STREAM data) {
    constructorHelper(dwidth,awidth,nelems,segPtr,addr,data);
  }

  ScoreSegmentWriteOnly(unsigned int dwidth,unsigned int awidth, 
			unsigned int nelems,
			UNSIGNED_FIXED_SCORE_SEGMENT segPtr,
			UNSIGNED_SCORE_STREAM addr,
			UNSIGNED_FIXED_STREAM data) {
    constructorHelper(dwidth,awidth,nelems,segPtr,addr,data);
  }

  //
  // untyped signature with no checking
  //

  ScoreSegmentWriteOnly(ScoreSegment *segPtr,
			ScoreStream *addr, 
			ScoreStream *data) {
    constructorHelper(NOCHECK,NOCHECK,NOCHECK,segPtr,addr,data);
  }

  ~ScoreSegmentWriteOnly();

  int step();

private:

  void constructorHelper(unsigned int ,unsigned int , unsigned int,
			 ScoreSegment *,ScoreStream *,ScoreStream *);
};

#endif

