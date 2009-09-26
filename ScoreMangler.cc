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
// $Revision: 2.2 $
//
//////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "ScoreOperatorMangle.h"

#define MAX_NAME_LEN          80
#define MAX_NUM_PARAMS        128

#define TOKEN_ERROR           0
#define TOKEN_LPAREN          1
#define TOKEN_RPAREN          2
#define TOKEN_NUM             3
#define TOKEN_COMMA           4
#define TOKEN_EOF             5
#define TOKEN_NEWLINE         6

int get_token(FILE *f, int *val) 
{
  int c = fgetc(f);

  if (c == EOF) {
    return TOKEN_EOF;
  }

  if (c == '(') {
    return TOKEN_LPAREN;
  } else if (c == ')') {
    return TOKEN_RPAREN;
  } else if (c == ',') {
    return TOKEN_COMMA;
  } else if (c == '\n') {
    return TOKEN_NEWLINE;
  } else if (isdigit(c)) {
    if (ungetc(c, f) != c) {
      fprintf(stderr, "died on ungetc\n");
      exit (1);
    }
    fscanf(f, "%d", val);
    return TOKEN_NUM;
  } else if (isspace(c)) { // skip spaces
    return get_token(f, val);
  } else {
    return TOKEN_ERROR;
  }
}

bool parse_line(FILE *f, char *buf, int *nparams, int *params)
{
  fscanf(f, "%[^(]", buf);

  int token_type, val;
  int counter = 0;

  while ((token_type = get_token(f, &val)) != TOKEN_EOF) {
    if (token_type == TOKEN_NEWLINE) {
      break;
    }

    if (token_type == TOKEN_NUM) {
      params[counter++] = val;
    }
    
    if (token_type == TOKEN_ERROR) {
      fprintf(stderr, "parsing error\n");
      exit(1);
    }
  }

  *nparams = counter;

  return (token_type == TOKEN_NEWLINE);
}

void process_file(const char *filename) 
{
  FILE *f = fopen (filename, "r");

  if (!f) {
    fprintf(stderr, "ScoreMangler: error: unable to open \'%s\'\n",
	    filename);
    exit(1);
  }

  int counter = 0;
  char buf[MAX_NAME_LEN];
  int nparams;
  int params[MAX_NUM_PARAMS];

  while (parse_line(f, buf, &nparams, params)) {
    if (counter > 0) {
      printf("|");
    }
    printf("%s", mangleOpName(buf, nparams, params));
    counter ++;
  }

  if (nparams) {
    if (counter > 0) {
      printf("|");
    }
    printf("%s", mangleOpName(buf, nparams, params));
  }

  fclose(f);
}

int main (int argc, char *argv[]) 
{
  printf("(");
  for(int i = 1; i < argc; i ++) {
    if (i > 1) {
      printf("|");
    }
    process_file (argv[i]);
  }
  printf(")");
  return 0;
}
