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
// $Revision: 1.8 $
//
//////////////////////////////////////////////////////////////////////////////
#ifndef _ScoreSegmentReadWrite_H

#define _ScoreSegmentReadWrite_H

#include <unistd.h>
#include "ScoreSegment.h"

#define READTOKEN 0
#define WRITETOKEN 1
#define NOT_READ 2

class ScoreSegmentReadWrite : public ScoreSegment {
 public:
  void *operator new(size_t size);

  // typed signature with TYPED_SCORE_SEGMENT

  ScoreSegmentReadWrite(unsigned int dwidth,unsigned int awidth, 
			size_t nelems,
			UNSIGNED_SCORE_SEGMENT segPtr,
			UNSIGNED_SCORE_STREAM addr,
			UNSIGNED_SCORE_STREAM dataR,
			UNSIGNED_SCORE_STREAM dataW,
			BOOLEAN_SCORE_STREAM write) {
    constructorHelper(dwidth,awidth,nelems,segPtr,addr,dataR,dataW,write);
  }

  ScoreSegmentReadWrite(unsigned int dwidth,unsigned int awidth, 
			size_t nelems,
			SIGNED_SCORE_SEGMENT segPtr,
			UNSIGNED_SCORE_STREAM addr,
			SIGNED_SCORE_STREAM dataR,
			SIGNED_SCORE_STREAM dataW,
			BOOLEAN_SCORE_STREAM write) {
    constructorHelper(dwidth,awidth,nelems,segPtr,addr,dataR,dataW,write);
  }

  ScoreSegmentReadWrite(unsigned int dwidth,unsigned int awidth, 
			size_t nelems,
			BOOLEAN_SCORE_SEGMENT segPtr,
			UNSIGNED_SCORE_STREAM addr,
			BOOLEAN_SCORE_STREAM dataR,
			BOOLEAN_SCORE_STREAM dataW,
			BOOLEAN_SCORE_STREAM write) {
    constructorHelper(dwidth,awidth,nelems,segPtr,addr,dataR,dataW,write);
  }

  ScoreSegmentReadWrite(unsigned int dwidth,unsigned int awidth, 
			size_t nelems,
			SIGNED_FIXED_SCORE_SEGMENT segPtr,
			UNSIGNED_SCORE_STREAM addr,
			SIGNED_FIXED_STREAM dataR,
			SIGNED_FIXED_STREAM dataW,
			BOOLEAN_SCORE_STREAM write) {
    constructorHelper(dwidth,awidth,nelems,segPtr,addr,dataR,dataW,write);
  }

  ScoreSegmentReadWrite(unsigned int dwidth,unsigned int awidth, 
			size_t nelems,
			UNSIGNED_FIXED_SCORE_SEGMENT segPtr,
			UNSIGNED_SCORE_STREAM addr,
			UNSIGNED_FIXED_STREAM dataR,
			UNSIGNED_FIXED_STREAM dataW,
			BOOLEAN_SCORE_STREAM write) {
    constructorHelper(dwidth,awidth,nelems,segPtr,addr,dataR,dataW,write);
  }

  //
  // untyped signature with no checking
  //
  ScoreSegmentReadWrite(ScoreSegment *segPtr, 
			ScoreStream *addr,
			ScoreStream *dataR,
			ScoreStream *dataW,
			ScoreStream *write) {
    constructorHelper(NOCHECK,NOCHECK,NOCHECK,segPtr,addr,dataR,dataW,write);
  }

  ~ScoreSegmentReadWrite();

  int step();

 private:

  void constructorHelper(unsigned int ,unsigned int, size_t,
			 ScoreSegment *,ScoreStream *,
			 ScoreStream *,ScoreStream *,ScoreStream *);

  int writeToken;
  int sim_faultedMode;

};

#endif

