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

#ifndef ScoreFeedbackGraph_h__
#define ScoreFeedbackGraph_h__

#include "LEDA/core/list.h"
#include "LEDA/core/string.h"
#include <stdio.h>
#include <string.h>
#include <map>

using leda::list;
using leda::string;

class ScoreGraphNode;

typedef enum { NODE_TYPE_PAGE, NODE_TYPE_SEGMENT, NODE_TYPE_ERROR } ScoreFeedbackNodeType;

class ScoreFeedbackGraphNode {
 public: 
  ScoreFeedbackGraphNode() :
    _kind(NODE_TYPE_ERROR),
    _numInputs(0),
    _numOutputs(0),
    _nodeTag(0xffffffff) {}

  ScoreFeedbackGraphNode(ScoreFeedbackNodeType kind,
			 string myname,
			 int numIns,
			 int numOuts,
			 size_t nodeTag,
			 ScoreGraphNode *scoreNode) :
    _kind(kind), _numInputs(numIns), _numOutputs(numOuts), 
    _nodeTag(nodeTag), _scoreNode(scoreNode) {
    _name = new char[strlen(myname)+1];
    strcpy(_name, myname);
  }

  ~ScoreFeedbackGraphNode() {
    if (_name)
      delete _name;
  }

  // records consumption in the _inputConsumption vector.
  // from the array provided
  void recordConsumption (const unsigned int *inCons, int size);

  // records production in the _outputProduction vector.
  // from the array provided
  void recordProduction (const unsigned int *outProd, int size);
  
  void recordFireCount(const int fire);

  void writeTab(FILE *f);
  // write/read the text file in our feedback format
  void writeText(FILE *f);
  int readText(FILE *f);

  int readUntil(FILE *f, char theChar);

  typedef list<unsigned int> Vector_1d;
  typedef list<Vector_1d> Vector_2d;

  int readVector2(FILE *f, Vector_2d &vec, int count, int count2);

  template <class C> int readVector(FILE *f, list<C> &vec, int count);

  void writeHistoryVector(FILE *f, Vector_2d &);

  size_t getTag() const { return _nodeTag; }

  void getStatInfo(unsigned int **consumption, unsigned int **production, 
		   unsigned int *fireCount);

  
  // emit node descriptions for the sched template
  void outputSchedNodes(FILE *f);

  static void resetCPUio() { _cpuInputs = _cpuOutputs = 0; }

 private:
  // these vectors record the history of the cons/prod
  // thus to access mth record about nth input/output
  // one should _inputConsumption[m][n]
  Vector_2d _inputConsumption;
  Vector_2d _outputProduction;
  Vector_1d _fireCount;
  
  ScoreFeedbackNodeType _kind;
  char *_name;
  int _numInputs, _numOutputs;
  size_t _nodeTag;
  static const char *node_kinds[];
  ScoreGraphNode *_scoreNode;

  static int _cpuInputs;
  static int _cpuOutputs;
};

class ScoreFeedbackGraph {
 public:
  ScoreFeedbackGraph(const char *userDir);
  ~ScoreFeedbackGraph();

  // adds a node to the graph and returns a pointer to its
  // entry in the dictionary
  ScoreFeedbackGraphNode *addNode(size_t nodeTag, ScoreGraphNode* node);

  // removes the node from the dictionary, and destroys
  // the node 
  void removeNode(size_t nodeTag);

  void writeTab(FILE *f);
  // write/read the text fiule in our feedback format
  void writeText(FILE *f);
  int readText(FILE *f);

  // given a nodeTag, return the pointer to the entry
  ScoreFeedbackGraphNode *getNodePtr(size_t nodeTag);

  // obtains a "status" of this data structure
  // true if everything is ok (no errors)
  // false if there are errors
  bool getStatus() const { return _status; }

  // emit node descriptions for the sched template
  void outputSchedNodes(FILE *f);

 private:
  std::map<size_t, ScoreFeedbackGraphNode*> _nodes;
  const char *_userDir;
  // the following is true iff the graph needs to be written on
  // disk on destruction. if the graph was constructed dynamically
  // this will be true. however, if it was constructed by reading
  // the feedback file, this will be false.
  bool _needWrite;
  bool _status;
};


#endif // ifndef ScoreFeedbackGraph_h__
