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
// $Revision: 1.9 $
//
//////////////////////////////////////////////////////////////////////////////
#ifndef _Score_H

#define _Score_H

#ifdef __cplusplus

#include <iostream>

using std::cout;
using std::endl;
using std::cerr;

// Added by Nachiket on 21st April 2010
// painful lesson about two diff. compiles
#include "ScoreConfig.h"

#include "ScoreOperator.h"
#include "ScoreOperatorElement.h"
#include "ScoreOperatorInstance.h"
#include "ScoreStream.h"
#include "ScoreSegment.h"
#include "ScoreSegmentReadOnly.h"
#include "ScoreSegmentWriteOnly.h"
#include "ScoreSegmentReadWrite.h"
#include "ScoreSegmentSeqReadOnly.h"
#include "ScoreSegmentSeqCyclicReadOnly.h"
#include "ScoreSegmentSeqWriteOnly.h"
#include "ScoreSegmentSeqReadWrite.h"
#include "ScoreGlobalCounter.h"

#else

#include "ScoreCInterface.h"

#endif

#include "ScoreSegmentOperatorReadOnly.h"
#include "ScoreSegmentOperatorWriteOnly.h"
#include "ScoreSegmentOperatorReadWrite.h"
#include "ScoreSegmentOperatorSeqReadOnly.h"
#include "ScoreSegmentOperatorSeqCyclicReadOnly.h"
#include "ScoreSegmentOperatorSeqWriteOnly.h"
#include "ScoreSegmentOperatorSeqReadWrite.h"

#include "ScoreConfig.h"

#endif
