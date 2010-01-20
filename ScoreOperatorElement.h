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
// $Revision: 1.4 $
//
//////////////////////////////////////////////////////////////////////////////
#ifndef _ScoreOperatorElement_H

#define _ScoreOperatorElement_H

#include "ScoreOperatorInstanceElement.h"

class ScoreOperator;

class ScoreOperatorElement 
{

public:
  ScoreOperatorElement(char *new_name, int new_params, int new_total_args,
		       int new_param_locs,
                       ScoreOperatorInstanceElement *new_inst,
		       ScoreOperatorElement *new_next):
    name(new_name), param_count(new_params), total_args(new_total_args),
    param_locs(new_param_locs),  next(new_next), inst(new_inst) { }
  char * getName() { return(name); }
  ScoreOperatorElement *getNext() {return(next);}
  int getParamCount() { return(param_count); }
  int getTotalArgs() { return(total_args); }
  int getParamLocations() {return(param_locs);}
  ScoreOperatorInstanceElement *getInstance() {return(inst);}
  void addInstance(ScoreOperator* op, int *instparams);

  private:  
  char * name;
  int param_count;
  int total_args;
  int param_locs; // bit vector
  ScoreOperatorElement *next;
  ScoreOperatorInstanceElement *inst;

};

#endif
