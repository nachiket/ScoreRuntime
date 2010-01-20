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
// $Revision: 1.15 $
//
//////////////////////////////////////////////////////////////////////////////
#ifndef _ScoreOperator_H

#define _ScoreOperator_H

#include "ScoreOperatorElement.h"
#include "ScoreStream.h"
#include "ScoreStreamType.h"
#include "ScoreGraphNode.h"
#include "ScoreConfig.h"
#include <iostream>
#include <fstream>

using std::ofstream;

class ScoreOperator: public ScoreGraphNode
{

public:
  ScoreOperator() {
    _isPage = 0;
    _isOperator = 1;
    _isSegment = 0;
  }

  static ScoreOperatorElement *addOperator(char *,int,int,int);
  static void forAllOperators();
  static void dumpGraphviz(ofstream *fout);

  virtual NodeTags getTag() { return ScoreOperatorTag; }

protected:
  virtual char *mangle(char *base, int nparam, int *params);
  static char *lpath;
  static char *fpath;
  static char *pwd;
  static int schedulerid; 
  static ScoreOperatorElement *oplist;;
  static char *resolve(char *base); 
  static FILE *feedback_file(char *base);
  static int schedulerId() {return(schedulerid);}
  static void addInstance(ScoreOperatorElement *elm, int *params);

};

//ofstream* ScoreOperator::fout=NULL;

#endif






