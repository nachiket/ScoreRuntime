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
// $Revision: 1.1 $
//
//////////////////////////////////////////////////////////////////////////////
#ifndef _ScoreStreamType_H

#define _ScoreStreamType_H


class ScoreStreamType
{

public:
  ScoreStreamType(int new_is_signed, int new_width):
    is_signed(new_is_signed), 
    int_width(new_width), fract_width(0),
    is_float(0), is_double(0) 
    {}
  ScoreStreamType(int new_is_signed, int new_int_width, int new_fract_width):
    is_signed(new_is_signed), 
    int_width(new_int_width), fract_width(new_fract_width),
    is_float(0), is_double(0)
    {}
  ScoreStreamType(int new_is_signed, int new_is_float, int new_is_double, int new_width):
    is_signed(0), 
    int_width(0), fract_width(0),
    is_float(new_is_float), is_double(new_is_double)
    {}

   // Nachiket added method for floats/doubles
   // signed is irrelevant but retained for having 4-input constructor for disambiguation...
    
  // probably add stuff to this and move these to private
  //   once we have an idea of how we're going to use

  int is_signed;
  int int_width;
  int fract_width;

  // Nachiket added support for floating-point
  int is_float;
  int is_double;

};



#endif






