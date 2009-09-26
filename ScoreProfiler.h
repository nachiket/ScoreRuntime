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
// $Id: ScoreProfiler.h,v 2.3 2001/10/26 04:01:49 yurym Exp $
// 
//   supports all profiling
//
//////////////////////////////////////////////////////////////////////////////

#ifndef _ScoreProfiler_h__
#define _ScoreProfiler_h__

#include <stdio.h>
#include <values.h>
#include <stdlib.h>
#include <iostream>
#include <assert.h>

using std::ostream;

typedef unsigned long long statDataType;

class CategoryRecord {
 public:
  CategoryRecord() { reset(); }
  
  void print(ostream &s, statDataType overall_total = 0ull,
	     bool summarize = false, bool totalOnly = false);

  void addSample(statDataType v);

  void reset();

  bool empty() const { return (_count == 0); }

  /* accessors */
  statDataType total() const { return _total; }
  statDataType min() const { return _min; }
  statDataType max() const { return _max; }
  unsigned int count() const { return _count; }

  statDataType lastVal() const { return _lastVal; }
  
 private:
  statDataType _total;
  statDataType _min;
  statDataType _max;
  unsigned int _count;

  statDataType _lastVal;
};


class ScoreProfiler {
 public:
  ScoreProfiler(size_t num_cats, const char *str[], bool add_perItem = false,
		const char *greeting_str = 0);
  ~ScoreProfiler();

  void addSample(unsigned int ts,
		 size_t index, statDataType v, bool verbose = false);

  void addSample_perItem(size_t index, statDataType v, unsigned int count,
			 bool verbose);


  void print(ostream &s, bool summarize = true, bool totalOnly = false);

  void finishTS(bool verbose = false);

  statDataType aggregate() const { return _aggregate; }

  statDataType lastVal(size_t index);
  
 private:
  size_t _count;
  const char **_str;
  CategoryRecord *_recs;
  CategoryRecord *_recs_perItem;

  CategoryRecord _summary_rec;
  statDataType _aggregate;
  unsigned int _curr_ts;

  const char *_greeting_str;
};

#endif

