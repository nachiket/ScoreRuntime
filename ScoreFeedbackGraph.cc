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

#if GET_FEEDBACK

#include "ScoreFeedbackGraph.h"
#include "ScoreGraphNode.h"
#include "ScorePage.h"
#include "LEDA/core/list.h"

using std::map;
using leda::list;

extern ScoreFeedbackMode gFeedbackMode;
extern int gFeedbackSampleFreq;
extern char *gFeedbackFilePrefix;


int ScoreFeedbackGraphNode::_cpuInputs = 0;
int ScoreFeedbackGraphNode::_cpuOutputs = 0;


// error codes returned by i/o functions
#define IO_OK        0
#define IO_EOF       -1
#define IO_CORRUPT   -2

//--------------Implementation of ScoreFeedbackGraphNode------------------

const char *ScoreFeedbackGraphNode::node_kinds[] = { "Page","Segment" };

// records consumption in the _inputConsumption vector.
// from the array provided
void ScoreFeedbackGraphNode::recordConsumption (const unsigned int *inCons, int size)
{
  assert(_numInputs == size); // just verify that array traversal is safe
  Vector_1d *tmpVector = new Vector_1d;
  for (int i = 0; i < _numInputs; i ++, inCons++ ) {
    tmpVector->push_back(*inCons); 
  }
  _inputConsumption.push_back(*tmpVector);
}

// records production in the _outputProduction vector.
// from the array provided
void ScoreFeedbackGraphNode::recordProduction (const unsigned int *outProd, int size)
{
  assert(_numOutputs == size); // just verify that array traversal is safe
  Vector_1d *tmpVector = new Vector_1d;
  for (int i = 0; i < _numOutputs; i ++, outProd++ ) {
    tmpVector->push_back(*outProd); 
  }
  _outputProduction.push_back(*tmpVector);
}

// records how many times the page fired
void ScoreFeedbackGraphNode::recordFireCount(const int fire)
{
  _fireCount.push_back(fire);
}

void ScoreFeedbackGraphNode::writeTab(FILE *f)
{
  fprintf(f, "%d\t%s\t%d\t%d\t",
	  _nodeTag, _name, _numInputs, _numOutputs);
  Vector_1d lastv = _inputConsumption.back();
  int v;
  forall(v,lastv) {
    fprintf(f, "%d\t", v);
  }
  lastv = _outputProduction.back();
  forall (v,lastv) {
    fprintf(f, "%d\t", v);
  }

  fprintf(f, "%d\n", _fireCount.back());
}

// write/read the text file in our feedback format
void ScoreFeedbackGraphNode::writeText(FILE *f)
{
  fprintf(f, "%s: %d %d %d consumed: %d {", _name,
	  _nodeTag, _numInputs, _numOutputs, _inputConsumption.size());
  writeHistoryVector(f, _inputConsumption);
  fprintf(f, "} produced: %d {", _outputProduction.size());
  writeHistoryVector(f, _outputProduction);
  fprintf(f, "} fired: %d { ", _fireCount.size());
  int num;
  forall (num, _fireCount) {
    fprintf(f, "%d ", num);
  }
  fprintf(f, "}\n");
}

int ScoreFeedbackGraphNode::readText(FILE *f)
{
  char buffer[1000];
  int retval, count;
  if ((retval = fscanf(f, "%[^:]%*c%d%d%d", buffer, &_nodeTag, 
		       &_numInputs, &_numOutputs)) == EOF)
    return IO_EOF;

  if (retval != 4) 
    return IO_CORRUPT;

  // free up old value if exists
  if (_name)
    delete _name;

  _name = new char [strlen(buffer) + 1];
  strcpy(_name, buffer);

  _inputConsumption.clear();
  _outputProduction.clear();
  _fireCount.clear();

  if ((retval = fscanf(f, "%s%d", buffer, &count)) != 2)
    return IO_CORRUPT;

  if (strcmp(buffer, "consumed:"))
    return IO_CORRUPT;

  if ((retval = readVector2(f, _inputConsumption, count, _numInputs)) != IO_OK)
    return retval;

  if ((retval = fscanf(f, "%s%d", buffer, &count)) != 2)
    return IO_CORRUPT;

  if (strcmp(buffer, "produced:"))
    return IO_CORRUPT;

  if ((retval = readVector2(f, _outputProduction, count, _numOutputs)) != IO_OK)
    return retval;

  if ((retval = fscanf(f, "%s%d", buffer, &count)) != 2)
    return IO_CORRUPT;

  if (strcmp(buffer, "fired:"))
    return IO_CORRUPT;

  if ((retval = readVector(f, _fireCount, count)) != IO_OK)
    return retval;

  if ((retval = readUntil(f, '\n')) != IO_OK)
    return retval;

  return IO_OK;
}

// this will only work with integers
template <class C>
int ScoreFeedbackGraphNode::readVector(FILE *f, list<C> &vec, int count)
{
  C value;
 
  if (readUntil(f, '{') != IO_OK)
    return IO_CORRUPT;

  vec.clear();

  // keep reading in until the matching right bracket is encountered
  while (fscanf(f, "%d", &value) == 1)
    vec.push_back(value);

  if (readUntil(f, '}') != IO_OK)
    return IO_CORRUPT;

  if (vec.size() != count)
    return IO_CORRUPT;

  return IO_OK;
}

int ScoreFeedbackGraphNode::readVector2(FILE *f, Vector_2d &vec, int count, int count2)
{
  if (readUntil(f, '{') != IO_OK)
    return IO_CORRUPT;
  
  for (int i = 0; i < count; i ++) {
    list<unsigned int> *tmpV = new list<unsigned int>;
    if (readVector(f, *tmpV, count2) != IO_OK)
      return IO_CORRUPT;
    vec.push_back(*tmpV);
  }
  
  if (readUntil(f, '}') != IO_OK)
    return IO_CORRUPT;
  
  return IO_OK;
} 


int ScoreFeedbackGraphNode::readUntil(FILE *f, char theChar)
{
  int ch;

  while ((ch = fgetc(f)) != EOF) {
    if (((char)ch) == theChar)
      return IO_OK;
  }

  return IO_CORRUPT;
}
void ScoreFeedbackGraphNode::writeHistoryVector(FILE *f, Vector_2d &vec)
{
  list_item time_iter;
  list_item input_iter;
  forall_items (time_iter, vec) {
    fprintf(f, "{ ");
    forall_items(input_iter, vec.inf(time_iter)) {
      fprintf(f, "%d ", vec.inf(time_iter).inf(input_iter));
    }
    fprintf(f, "}");
  }
}


void ScoreFeedbackGraphNode::getStatInfo(unsigned int **consumption, unsigned int **production, unsigned int *fireCount)
{
  list_item l1;

  l1 = _inputConsumption.last();
  if (l1 != NULL) {
    Vector_1d v = _inputConsumption.inf(l1);
    *consumption = new unsigned int [v.size()];
    unsigned int i, j = 0;
    forall(i, v) {
      *((*consumption) + j++) = i;
    }
  }

  l1 = _outputProduction.last();
  if (l1 != NULL) {
    Vector_1d v = _outputProduction.inf(l1);
    *production = new unsigned int [v.size()];
    unsigned int i, j = 0;
    forall(i, v) {
      *((*production) + j++) = i;
    }
  }

  l1 = _fireCount.last();
  if (l1 != NULL)
    *fireCount = _fireCount.inf(l1);
}


//-------------Implementation of ScoreFeedbackGraph-----------------------

ScoreFeedbackGraph::ScoreFeedbackGraph(const char *userDir) : _userDir(userDir), _status(true)
{
  assert(gFeedbackMode != NOTHING);
  
  static char *empty_str = "";
  char *file_prefix = 0;

  if (gFeedbackFilePrefix) {
    file_prefix = gFeedbackFilePrefix;
  } else {
    file_prefix = empty_str;
  }

  if (gFeedbackMode == READFEEDBACK) {
    _needWrite = false;
    char *filename = "program.feedback";
    char *tmpname = new char[strlen(_userDir) +
			    strlen(file_prefix) + strlen(filename)+2];
    sprintf(tmpname, "%s%s%s", _userDir, file_prefix, filename);
    FILE *file = fopen(tmpname, "r");
    if (file) {
      if (readText(file) != IO_OK) 
	_status = false; // simply mark the status, so that use of this 
      // structure is disabled
      fclose(file);
      //writeText(stderr);
    }
    else {
      cerr << "ScoreFeedbackGraph::ScoreFeedbackGraph: Error: Unable to open feedback file " << tmpname << ". Execution will continue without feedback information.\n";
      _status = false;
    }
    delete tmpname;
  } else if (gFeedbackMode == MAKEFEEDBACK || gFeedbackMode == SAMPLERATES) {
    _needWrite = true;
  }
  else {
    assert(0);
  }
}

ScoreFeedbackGraph::~ScoreFeedbackGraph()
{
  static char *empty_str = "";
  char *file_prefix = 0;
  
  if (gFeedbackFilePrefix) {
    file_prefix = gFeedbackFilePrefix;
  } else {
    file_prefix = empty_str;
  }
  // if this needs to be written into the file do it now
  if (_needWrite) {
    char *filename = "program.feedback";
    char *tmpname = new char[1000];
    sprintf(tmpname, "%s%s%s", _userDir, file_prefix, filename);
    FILE *file = fopen(tmpname, "w");
    if (!file) {
      cerr << "ScoreFeedbackGraph: unable to open feedback file for writing\n";
      cerr << "---------------nothing will be written----------------------\n";
      _status = false;
    }
    else {
      if (gFeedbackMode == SAMPLERATES) {
	fprintf(file, "sample_rate=%d\n", gFeedbackSampleFreq);
      }
      writeText(file);
      fclose(file);
    }

    char *filename1 = "program.feedback.tab";
    sprintf(tmpname, "%s%s%s", _userDir, file_prefix, filename1);
    file = fopen(tmpname, "w");
    if (!file) {
      cerr << "ScoreFeedbackGraph: unable to open feedback.tab file for writing\n";
      cerr << "---------------nothing will be written----------------------\n";
      _status = false;
    }
    else {
      writeTab(file);
      fclose(file);
    }
    delete tmpname;
  }
  _nodes.clear();
}

// adds a node to the graph and returns a pointer to its
// entry in the dictionary
ScoreFeedbackGraphNode *ScoreFeedbackGraph::addNode(size_t nodeTag, ScoreGraphNode *node)
{
#ifndef NDEBUG
  map<size_t, ScoreFeedbackGraphNode*>::iterator it = _nodes.find(nodeTag);
#endif

  // make sure that this is not already present in the map
  assert(it == _nodes.end());

  ScoreFeedbackGraphNode *newnode = 0;

  if(node->isPage()) {
    newnode = new ScoreFeedbackGraphNode(NODE_TYPE_PAGE,
					 string(((ScorePage*)node)->getSource()), 
					 node->getInputs(),
					 node->getOutputs(),
					 nodeTag, node);
  }
  else if (node->isSegment()) {
    newnode = new ScoreFeedbackGraphNode(NODE_TYPE_SEGMENT, 
					 string("Segment"),
					 node->getInputs(),
					 node->getOutputs(),
					 nodeTag, node);
  }
  else {
    assert(0);
  }

  assert(newnode);

  _nodes[nodeTag] = newnode;
  return newnode;
}

// removes the node from the dictionary, and destroys
// the node 
void ScoreFeedbackGraph::removeNode(size_t nodeTag)
{
  _nodes.erase(nodeTag);
}

void ScoreFeedbackGraph::writeTab(FILE *f)
{
  map<size_t, ScoreFeedbackGraphNode*>::const_iterator it;

  for (it = _nodes.begin(); it != _nodes.end(); it ++)
    it->second->writeTab(f);  
}

// write/read the text fiule in our feedback format
void ScoreFeedbackGraph::writeText(FILE *f)
{
  map<size_t, ScoreFeedbackGraphNode*>::const_iterator it;

  for (it = _nodes.begin(); it != _nodes.end(); it ++)
    it->second->writeText(f);
}
int ScoreFeedbackGraph::readText(FILE *f)
{
  int ret_val;
  _nodes.clear();
  
  while (!feof(f)) {
    ScoreFeedbackGraphNode *n = new ScoreFeedbackGraphNode();
    if ((ret_val = n->readText(f)) == IO_OK) {
      _nodes[n->getTag()]=n;
    }
    else if (ret_val == IO_CORRUPT) {
      cerr << "ScoreFeedbackGraph::readText: Error: feedback file is corrupted\n";
      return IO_CORRUPT;
    }
  }

  return IO_OK;
}

// given a nodeTag, return the pointer to the entry
ScoreFeedbackGraphNode *ScoreFeedbackGraph::getNodePtr(size_t nodeTag)
{
  map<size_t, ScoreFeedbackGraphNode*>::iterator it = _nodes.find(nodeTag);

  // make sure that it is present
  assert(it != _nodes.end());
  
  return it->second;
}

#endif











