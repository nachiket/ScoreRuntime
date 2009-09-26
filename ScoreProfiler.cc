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
// $Id: ScoreProfiler.cc,v 2.4 2003/01/29 23:25:24 yurym Exp $
// 
//   supports all profiling
//
//////////////////////////////////////////////////////////////////////////////

#include "ScoreProfiler.h"

using std::cout;
using std::cerr;
using std::endl;

////////////////////////////////////////////////////////////////////////
// CategoryRecord::print
////////////////////////////////////////////////////////////////////////
void CategoryRecord::print(ostream &s, statDataType overall_total,
			   bool summarize, bool totalOnly)
{
  if (_count > 0) {
    s << _total;

    if (!totalOnly) {
      s << " [" << _min << " : " << (_total / _count) << " : " << _max << "]";
    }

    if (summarize) {
      assert(overall_total);

      s << " [" << (((double)_total / overall_total) * 100) << "%]";
    }
  } else {
    if (totalOnly) {
      s << 0;
    } else {
      s << "<empty>";
    }
  }
}


////////////////////////////////////////////////////////////////////////
// CategoryRecord::addSample
////////////////////////////////////////////////////////////////////////
void CategoryRecord::addSample(statDataType v)
{
  _lastVal = v;

  _total = _total + v;
  _count ++;

  if (v < _min) {
    _min = v;
  }

  if (v > _max) {
    _max = v;
  }
}


////////////////////////////////////////////////////////////////////////
// CategoryRecord::reset
////////////////////////////////////////////////////////////////////////
void CategoryRecord::reset()
{
  _total = 0ull;
  _min = MAXLONG;
  _max = 0ull;
  _count = 0u;

  _lastVal = (statDataType)(-1);
}


////////////////////////////////////////////////////////////////////////
// ScoreProfiler::ScoreProfiler()
////////////////////////////////////////////////////////////////////////
ScoreProfiler::ScoreProfiler(size_t num_cats, const char *str[],
			     bool add_perItem, const char *greeting) :
  _count(num_cats), _str(str), _aggregate(0ull),
  _curr_ts((unsigned int)(-1)), _greeting_str(greeting)
{
  _recs = new CategoryRecord[_count];

  if (add_perItem) {
    _recs_perItem = new CategoryRecord[_count];
  } else {
    _recs_perItem = 0;
  }
}


////////////////////////////////////////////////////////////////////////
// ScoreProfiler::~ScoreProfiler()
////////////////////////////////////////////////////////////////////////
ScoreProfiler::~ScoreProfiler()
{
  delete _recs;
  delete _recs_perItem;
}

////////////////////////////////////////////////////////////////////////
// ScoreProfiler::addSample()
////////////////////////////////////////////////////////////////////////
void ScoreProfiler::addSample(unsigned int ts, 
			      size_t index, statDataType v, bool verbose)
{
  assert(index < _count);
  
  _recs[index].addSample(v);

  if (ts != _curr_ts) { // begin next timeslice
    if (_curr_ts != (unsigned int)(-1)) {
      finishTS(verbose);
    }
    
    _curr_ts = ts;
  }

  _aggregate += v;

  if (verbose) {
    cerr << "PROFILER[" << _curr_ts << "] " << _str[index] << ": "
	 << v << endl;
  }
}

////////////////////////////////////////////////////////////////////////
// ScoreProfiler::addSample_perItem()
////////////////////////////////////////////////////////////////////////
void ScoreProfiler::addSample_perItem(size_t index, statDataType v,
				      unsigned count, bool verbose)
{
  assert(index < _count);
  assert(_recs_perItem);

  if (count > 0) {

    statDataType sample = v / count;


    _recs_perItem[index].addSample(sample);
    
    if (verbose) {
      cerr << "PROFILER[" << _curr_ts << "] " << _str[index]
	   << "_perItem: " << sample << endl;
    }
  }
}


////////////////////////////////////////////////////////////////////////
// ScoreProfiler::finish()
////////////////////////////////////////////////////////////////////////
void ScoreProfiler::finishTS(bool verbose)
{
  if (verbose && _greeting_str) {
    cerr << "PROFILER[" << _curr_ts << "] " << _greeting_str << ": "
	 << _aggregate << endl;
  }
  
  _summary_rec.addSample(_aggregate);
  _aggregate = 0ull;
}

////////////////////////////////////////////////////////////////////////
// ScoreProfiler::print(ostream &s)
////////////////////////////////////////////////////////////////////////
void ScoreProfiler::print(ostream &s, bool summarize, bool totalOnly)
{
  statDataType overall_total = _summary_rec.total();

  for(size_t i = 0; i < _count; i ++) {
    s << "Total " << _str[i] << ": ";
    _recs[i].print(s, overall_total, summarize, totalOnly);

    if (_recs_perItem && (!_recs_perItem[i].empty())) {
      s << " --> ";
      _recs_perItem[i].print(s);
    }
    s << endl;
  }

  if (summarize) {
    s << "*** Overall Total ";
    _summary_rec.print(s);
    s << endl;
  }
}


////////////////////////////////////////////////////////////////////////
// ScoreProfiler::lastVal
////////////////////////////////////////////////////////////////////////
statDataType ScoreProfiler::lastVal(size_t index) 
{
  assert(index < _count);

  return _recs[index].lastVal();
}
