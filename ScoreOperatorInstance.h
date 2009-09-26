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
// $Revision: 1.10 $
//
//////////////////////////////////////////////////////////////////////////////
#ifndef _ScoreOperatorInstance_H

#define _ScoreOperatorInstance_H

#include "ScorePage.h"
#include "ScoreSegment.h"


// prototypes to avoid circular includes.
class ScoreProcess;


class ScoreOperatorInstance
{

public:
  ScoreOperatorInstance() {
    pages = 0;
    segments = 0;
    page = NULL;
    segment = NULL;
    sharedObjectName = NULL;
    parentProcess = NULL;
    page_group = NULL;
    segment_group = NULL;
  }

  ~ScoreOperatorInstance() {
    if (sharedObjectName != NULL) {
      delete(sharedObjectName);
    }
  }

  unsigned int pages;
  unsigned int segments;
  ScorePage **page;
  ScoreSegment **segment;

  // stores the shared object name used to instantiate this operator instance.
  char *sharedObjectName;

  // stores the pointer to the parent process.
  ScoreProcess *parentProcess;

  //////////////////////////////////////////////////////
  // BEGIN SCHEDULER VARIABLES
  //////////////////////////////////////////////////////

  // used by the scheduler to count how many "live" pages/segments there
  // really are.
  unsigned int sched_livePages;
  unsigned int sched_liveSegments;

  void *sched_handle;

  //////////////////////////////////////////////////////
  // END SCHEDULER VARIABLES
  //////////////////////////////////////////////////////

protected:
  int * page_group;
  int * segment_group;

};

typedef ScoreOperatorInstance *(* construct_t) (char *);

// needed by LEDA for use with lists/etc.
int compare(ScoreOperatorInstance * const & left, 
	    ScoreOperatorInstance * const & right);

#endif

