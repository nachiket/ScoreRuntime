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

  int is_signed;
  int int_width;
  int fract_width; // this will be ignored for SPICE
  int is_float;
  int is_double;

};

#endif
