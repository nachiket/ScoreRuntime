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

#include "ScoreArray.h"
#include "ScoreConfig.h"


///////////////////////////////////////////////////////////////////////////////
// ScoreArrayCP::ScoreArrayCP:
//   Constructor for ScoreArrayCP.
//   Initializes all internal structures.
//
// Parameters: None.
//
// Return value: None.
///////////////////////////////////////////////////////////////////////////////
ScoreArrayCP::ScoreArrayCP() {
  loc = 0;
  active = NULL;
  actual = NULL;
  scheduled = NULL;
}


///////////////////////////////////////////////////////////////////////////////
// ScoreArrayCP::~ScoreArrayCP:
//   Destructor for ScoreArrayCP.
//   Clean up all internal structures.
//
// Parameters: None.
//
// Return value: None.
///////////////////////////////////////////////////////////////////////////////
ScoreArrayCP::~ScoreArrayCP() {
  // do nothing!
}


///////////////////////////////////////////////////////////////////////////////
// ScoreArrayCMB::ScoreArrayCMB:
//   Constructor for ScoreArrayCMB.
//   Initializes all internal structures.
//
// Parameters: None.
//
// Return value: None.
///////////////////////////////////////////////////////////////////////////////
ScoreArrayCMB::ScoreArrayCMB() {
  loc = 0;
  active = NULL;
  actual = NULL;
  scheduled = NULL;
  segmentTable = NULL;
  unusedPhysicalCMBsItem = SCORECUSTOMLINKEDLIST_NULL;
}


///////////////////////////////////////////////////////////////////////////////
// ScoreArrayCMB::~ScoreArrayCMB:
//   Destructor for ScoreArrayCMB.
//   Clean up all internal structures.
//
// Parameters: None.
//
// Return value: None.
///////////////////////////////////////////////////////////////////////////////
ScoreArrayCMB::~ScoreArrayCMB() {
  if (segmentTable != NULL) {
    delete(segmentTable);
  }
}

