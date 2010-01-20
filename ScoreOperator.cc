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
// $Revision: 1.21 $
//
//////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <sys/stat.h>
#include <sys/file.h>
#include <unistd.h>
#include <string.h>
#include "ScoreOperator.h"
#include "ScoreConfig.h"
#include "ScoreOperatorMangle.h"

const char *DEFAULT_PATH=".";

char *ScoreOperator::mangle(char *base, int nparam, int *params)
{
  return mangleOpName(base, nparam, params);
}

char *ScoreOperator::resolve(char *base)
{
  char *trial_path;
  char *tname;
  char *original_lpath=lpath;

  //  search through library path

  if (lpath==(char *)NULL)
    {
      lpath=(char *)malloc(strlen(DEFAULT_PATH));
      strcpy(lpath,DEFAULT_PATH);
      original_lpath=lpath;
    }
			 
  trial_path=(char *)malloc(strlen(lpath)+strlen(pwd)+2); // just to be safe
  // +1 for null, +1 for trailing slash, if necessary
  tname=(char *)malloc(strlen(lpath)+strlen(pwd)+strlen(base)+5);
  FILE *try_fd;
  while (strlen(lpath)>0)
    {
      char * firstcolon=index(lpath,':');
      int len=0;
      if (firstcolon!=NULL)
	len=(firstcolon-lpath);
      else
	len=strlen(lpath);

      if (((len==1) && (lpath[0] == '.')) ||
	  ((lpath[0] == '.') && (lpath[1] == '/'))){
	strncpy(trial_path,pwd,strlen(pwd));
	if (len == 1) {
	  len=strlen(pwd);
	} else {
	  strncpy((trial_path+strlen(pwd)),(lpath+1),(len-1));
	  len=len-1+strlen(pwd);
	}
      } else {
	strncpy(trial_path,lpath,len);
      }
      if (*(trial_path+len-1)!='/')
	{
	  *(trial_path+len)='/';
	  len++;
	}
      *(trial_path+len)=(char)NULL;
      
      sprintf(tname,"%s%s.so",trial_path,base);
      if (VERBOSEDEBUG || DEBUG) {
	cerr << "ScoreOperator.resolve trying [" <<
	  tname << "]" << endl;
      }

      // try to access file
      // N.B. shouldn't really have to open it
      //    ...stat would do, but not sure, yet, what stat condition
      //    would suffice for this file...
      try_fd=fopen(tname,"r");
      if (try_fd!=NULL)
	{
	  if (VERBOSEDEBUG || DEBUG) {
	    cerr << "ScoreOperator.resolve: found " << 
	      tname << endl;
	  }
	  fclose(try_fd);
	  free(trial_path);
	  lpath=original_lpath;
	  return(tname);
	}
      else
	{
	  // DEBUG
	  if (VERBOSEDEBUG || DEBUG) {
	    cerr << 
	      "ScoreOperator.resolve: failed to find " << 
	      tname << endl;
	  }
	}
	    
      if (firstcolon==NULL)
	break;
      else
	lpath=firstcolon+1; // after : for next round
    }
  // fail case
  free(trial_path);
  free(tname);
  lpath=original_lpath;
  return(NULL);
}


int get_schedulerid()
{
  int sidnum;
  
  FILE *sid=fopen(SCORE_SCHEDULER_ID_FILE,"r");
  if (sid!=NULL)
    {
      int fcnt=fread(&sidnum,4,1,sid);
      fclose(sid);
      if ((fcnt==1) & (sidnum!=-1))  // added size check 5/19/99 amd
	return(sidnum);
    }

  // For safety?
  if(errno) {
//    perror("Yeah yeah... ");
    errno=0;
  }

  return 0;
  
//  cerr << "ScoreOperator.schedulerid: could not get scheduler id from " 
//       << SCORE_SCHEDULER_ID_FILE << endl;
// Nachiket edit: what is this??? Do we really need this
//  exit(1);
  
}

ScoreOperatorElement *init_op_list()
{

  //atexit(ScoreOperator::forAllOperators);
  return((ScoreOperatorElement *)NULL);

}

char * ScoreOperator::lpath=getenv(SCORE_LIBRARY_PATH_ENV);
char * ScoreOperator::fpath=getenv(SCORE_FEEDBACK_DIR_ENV);
char * ScoreOperator::pwd=getenv("PWD");
int ScoreOperator::schedulerid=get_schedulerid();

ScoreOperatorElement *ScoreOperator::oplist=init_op_list();

ScoreOperatorElement *ScoreOperator::addOperator(char *name, int params,
						 int targs, int plocs)
{
  // create new
  ScoreOperatorElement * result=new ScoreOperatorElement(name,params,
							 targs,plocs,
							 (ScoreOperatorInstanceElement *)NULL,
							 oplist);
  // link in at head of list
  oplist=result;
  return(result);

}

void ScoreOperator::addInstance(ScoreOperatorElement *elm, ScoreOperator* op, int *params)
{
  elm->addInstance(op, params);
}

FILE *ScoreOperator::feedback_file (char *base)
{
  if (fpath==(char *)NULL)
    {
      fpath=(char *)malloc(strlen(DEFAULT_PATH)+1);
      strcpy(fpath,DEFAULT_PATH);
    }

  char *fname=(char *)malloc(strlen(fpath)+1+strlen(base)+1+1+
			     strlen(SCORE_AUTO_FEEDBACK_EXTENSION));
  // deal with trailing slash (or lack thereof)
  if (*(fpath+strlen(fpath)-1)!='/')
    sprintf(fname,"%s/%s.%s",fpath,base,SCORE_AUTO_FEEDBACK_EXTENSION);
  else
    sprintf(fname,"%s%s.%s",fpath,base,SCORE_AUTO_FEEDBACK_EXTENSION);


  FILE *tf=fopen(fname,"a");
  if (tf!=(FILE *)NULL)
    {
      flock(fileno(tf),LOCK_EX); // blocking
      return(tf);
    }
  else
    {
      cerr << "ScoreOperator: Error updating feedback file " << 
	fname << endl;
      perror("\t");
      return(tf);
    }
  
}

void ScoreOperator::forAllOperators()
{

  if (VERBOSEDEBUG || EXTRA_DEBUG || 1) {
    cerr << "now calling forAllOperators" << endl;
  }
  ScoreOperatorElement *eptr=oplist;
  while (eptr!=(ScoreOperatorElement *)NULL)
    {
      int num_params=eptr->getParamCount();
      if (VERBOSEDEBUG) {
	cerr << "Operator: " << eptr->getName() << 
	  " params=" << num_params << endl;
      }

      // TODO: filter out duplicate parameter requests

      FILE *fbfd=feedback_file(eptr->getName());
      if (fbfd!=NULL)
	{
	  int arg_count=eptr->getTotalArgs();
	  int plocs=eptr->getParamLocations();
	  
	  ScoreOperatorInstanceElement *iptr=eptr->getInstance();
	  while (iptr!=(ScoreOperatorInstanceElement *)NULL)
	    {
	      fprintf(fbfd,"%s(",eptr->getName());
	      int *params=iptr->getParams();
	      for (int i=0;i<arg_count;i++)
		{
		  if (i==(arg_count-1)) // last time
		    if (((plocs>>i) & 0x01)==0)
		      { // nothing
		      }
		    else
		      {
			fprintf(fbfd,"0x%x",*params); // no comma
			params++;
		      }
		  else
		    if (((plocs>>i) & 0x01)==0)
		      fprintf(fbfd,",");
		    else
		      {
			fprintf(fbfd,"0x%x,",*params);
			params++;
		      }
		}
	      fprintf(fbfd,");\n");
	      iptr=iptr->getNext();
	    }
	  // unlock file
	  flock(fileno(fbfd),LOCK_UN); // blocking
	  //close
	  fclose(fbfd);
	}

      if (VERBOSEDEBUG) {
	ScoreOperatorInstanceElement *iptr=oplist->getInstance();
	while (iptr!=(ScoreOperatorInstanceElement *)NULL)
	  {
	    int *params=iptr->getParams();
	    for (int i=0;i<num_params;i++)
	      {
		cerr << params[i] << " ";
	      }
	    cerr << endl;
	    iptr=iptr->getNext();
	  }
      }

      eptr=eptr->getNext();

    }



}


void ScoreOperator::dumpGraphviz(ofstream *fout) {
	ScoreOperatorElement *eptr=oplist;
	while (eptr!=(ScoreOperatorElement *)NULL)  
	{
		ScoreOperatorInstanceElement *iptr=eptr->getInstance();
		while (iptr!=(ScoreOperatorInstanceElement *)NULL)
		{
			*fout << "inside.." << eptr->getName() << endl;
			ScoreOperator* op= iptr->getOperator();

			// iterate over outputs
			for (int i = 0; i < op->outputs; i++) {
			    ScoreGraphNode *src=op->getOutput(i)->src;
			    ScoreGraphNode *sink=op->getOutput(i)->sink;

			    if(src!=NULL ** sink!=NULL) {
				    *fout << src->getName() << "->" << sink->getName() << " label=["<<op->getOutput(i)->getName()<<"]" << endl;
			    }
			}

	
	      		iptr=iptr->getNext();
		}
      		eptr=eptr->getNext();
	}
}

