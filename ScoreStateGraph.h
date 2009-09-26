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
// SCORE visualization
//
//////////////////////////////////////////////////////////////////////////////
#ifndef _ScoreStateGraph_H

#define _ScoreStateGraph_H

#include "ScoreScheduler.h"
#include "LEDA/core/dictionary.h"
#include <stdio.h>
#include <assert.h>

using leda::dictionary;
using std::ostream;

#if 0
#define ASSERT(_cond)  if (_cond) {} else { cerr << "ASSERTION FAILED IN __FILE__ " << \
 "ON LINE __LINE__" << endl; exit(1); }
#else
#define ASSERT(_cond)  assert(_cond)
#endif

#define NODE_STATUS_PAGE     0x1
#define NODE_STATUS_SEGMENT  0x2
#define NODE_STATUS_STITCH   0x4
#define NODE_STATUS_CPU      0x8
#define NODE_STATUS_OP       0x10
#define NODE_STATUS_RESIDENT 0x20
#define NODE_STATUS_FRONTIER 0x40

#define EDGE_STATUS_FULL     0x1
#define EDGE_STATUS_EMPTY    0x2
#define EDGE_STATUS_CONSUME  0x4
#define EDGE_STATUS_PRODUCE  0x8

#define visualFileMagicNum 0x12345678

#define READ_OK        0
#define READ_EOF      -1
#define READ_ERROR    -2

// class ScoreStateGraphNode
// this class represents a node of the state graph with all its attributes.

class ScoreStateGraphNode {

public:
  ScoreStateGraphNode() : status(0), name(0), label(-1) {}
  ScoreStateGraphNode(int mystatus, char *myname, int myLabel) {
    name = 0;
    setStatus(mystatus);
    setName(myname);
    setLabel(myLabel);
  }
  ~ScoreStateGraphNode() { if (name) delete name; }
  
  // write the contents of the data structure into specified binary file
  bool write(FILE* myfile);

  // read the specified binary file and replace the existing contents of this data structure
  bool read (FILE* myfile);
  
  // write a description of this node in the format req'd by vcg
  bool writeVCG(FILE* myfile, bool splitCPU = false, bool noCPU = false);

  // reset the contents of the data structure (release mem if needed)
  void clear();

  void print(FILE *myfile);

  //bool cmp(ScoreStateGraphNode *nd);

  // accessors/selectors:

  bool isStatus(int statusId) { return (statusId & status) ? true : false; }
  void setStatus(int statusId) { status = statusId; }
  void addStatus(int statusId) { status |= statusId; }
  void removeStatus(int statusId) { status &= (~statusId); }

  void setName(char *myname);
  char *getName() { return name; }

  void setLabel(int mylabel) { label = mylabel; }
  int  getLabel() { return label; } 

private:
  int status;
  char *name;
  
  int label;

};


// class ScoreStateGraphEdge
// this class represents an edge of the state graph with all its attributes
// NOTE: since the ScoreStateGraphNode does not contain any information about
//       adjecent nodes, this class contains a source and desitnation nodes that
//       are connected by this edge
class ScoreStateGraphEdge {

public:
  ScoreStateGraphEdge() : ptr_val(0), produced(-1), consumed(-1),
    source(-1), destination(-1), status (0), num_tokens(-1),
    src_num(-1), sink_num(-1) {}
  ScoreStateGraphEdge(SCORE_STREAM ptr, int pro, int con, int src, int dest, int stat, int ntokens, int src_n = -1, int sink_n = -1) :
    ptr_val(ptr),
    produced(pro), consumed(con), source(src),
    destination(dest), status(stat), num_tokens(ntokens),
    src_num(src_n), sink_num(sink_n)
    {}
  ~ScoreStateGraphEdge() {}

  // write the contents of the data structure into specified binary file
  bool write(FILE* myfile);
  
  // read the specified binary file and replace the existing contents of this data structure
  bool read (FILE* myfile);
  
  // write a description of this edge in the format req'd by vcg
  bool writeVCG(FILE* myfile, bool splitCPU = false, bool noCPU = false);

  void setProduced(int num) { ASSERT(num >= 0); produced = num; }
  int  getProduced() { return produced; }

  void setConsumed(int num) { ASSERT(num >= 0); consumed = num; }
  int  getConsumed() { return consumed; }

  void setSource(int num) { source = num; }
  int getSource() { return source; }

  void setDestination(int num) { destination = num; }
  int getDestination() { return destination; }

  void print(FILE *myfile);

  void setStatus(int statusId) { status = statusId; }
  void addStatus(int statusId) { status |= statusId; }
  void removeStatus(int statusId) { status &= (~statusId); }

  //bool cmp (ScoreStateGraphEdge *edge);

private:
  SCORE_STREAM ptr_val;

  int produced;
  int consumed;

  int source; // the label of the node where this edge originiates
  int destination; // similar to prev.

  int status;

  int num_tokens;

  int src_num;
  int sink_num;
};


// class ScoreStateGraph
// The object of this class is responsible for creating scheduler's state
// graph. It is capable of saving and restoring the graph from disk, and 
// also creating a .vcg description of the graph.

class ScoreStateGraph {
  
public:
  ScoreStateGraph(unsigned int _timeslice);
  ~ScoreStateGraph();

  // addNode function:
  // add node of the type specified by the argument and
  // return a pointer to the node that was just added.
  // these functions guarantee that the node is unique
  // thus if an attempt is made to add a node that already
  // exists, a pointer to the existing node will be returned.
  ScoreStateGraphNode* addNode(pid_t procId, int count, ScoreGraphNode *page);

  // addEdge:
  // add edge to the graph, making sure that it is unique
  // return a pointer to the newly added edge.
  ScoreStateGraphEdge* addEdge(pid_t procId, SCORE_STREAM stream);

  // addEdgeStatus:
  // add status (attribute) to the specified stream.
  // note: the stram _must_ be found
  //bool ScoreStateGraph::addEdgeStatus(SCORE_STREAM stream, int mystatus);
  bool addEdgeStatus(SCORE_STREAM stream, int mystatus);

  // write the graph into specified binary file
  bool write(FILE* myfile);
  
  // read specified bin file to obtain a description of the graph
  int read (FILE* myfile);
  
  // write a description of this graph in the format req'd by vcg
  bool writeVCG(FILE* myfile, bool splitCPU = false, bool noCPU = false);

  void clear();

  void print(FILE *myfile);

  // write a netlist description of edges, iff both nodes are resident
  // for now exclude the processor
  bool writeNetlist(FILE *myfile, bool uniqRes);

  //bool cmp (ScoreStateGraph &graph);

  // accessor methods
  void setCurrentTimeslice(unsigned int val) { currentTimeslice = val; }
  unsigned int getCurrentTimeslice() { return currentTimeslice; }

  void setCMBoffset(unsigned offset) { cmbOffset = offset; }

  void shortenName(const char *src, char *snk);

private:
  dictionary<ScoreGraphNode*, ScoreStateGraphNode*> *nodeDict;
  dictionary<SCORE_STREAM, ScoreStateGraphEdge*> *edgeDict;
  unsigned int currentTimeslice; // as seen by the ScoreScheduler

  int currentLabel;

  static int node_cmp(ScoreGraphNode* const &x, ScoreGraphNode* const &y) {
    size_t newx = (size_t)x;
    size_t newy = (size_t)y;
    int diff = newx - newy;
    return diff;
  }
  
  static int edge_cmp(SCORE_STREAM const &x, SCORE_STREAM const &y) {
    size_t newx = (size_t)x;
    size_t newy = (size_t)y;
    int diff = newx - newy;
    return diff;
  }

/*    static int edge_cmp(int const &x, int const &y) { */
/*      return x - y; */
/*    } */

  unsigned int cmbOffset;
 
};

///////////////////////////our vector type/////////////////////////////
template<class T> class score_vector;

template<class T>
ostream & operator<<(ostream &s, score_vector<T>& v) {
  s << "score_list: size = " << v._size <<
    " capacity = " << v._capacity << endl;
  for (int i = 0; i < v._size; i ++)
    s << v._buf[i] << endl;
  
  return s;
}

template<class T>
class score_vector {
  friend ostream & operator<< <>(ostream &s, score_vector<T>& v);

 public:
  score_vector() : _size(0), _capacity(16) {
    _buf = (T*) malloc(_capacity * sizeof(T));
    assert(_buf);
  }
  ~score_vector() {
    if (_buf)  free(_buf);
  }

  int size() const { return _size; }

  void push_back(T elm) {
    if (_size == _capacity) { // expand
      _capacity = (int)((float)_capacity * 1.5f);
      _buf = (T*) realloc(_buf, _capacity * sizeof(T));
      assert(_buf);
    }

    _buf[_size++] = elm;
  }
  
  T& at(int index) {
    assert(index >= 0 && index < _size);
    return _buf[index];
  }

 private:
  T *_buf;
  int _size;
  int _capacity;
};

template <class A, class B>
class score_pair {
 public:
  score_pair(A v1, B v2) : first(v1), second(v2) {}
  ~score_pair() {}

  A first;
  B second;
};

template <class A, class B>
ostream & operator<<(ostream &s, score_pair<A,B>& v) {
  s << "(" << v.first << ", " << v.second << ") ";
  return s;
}

#endif // _ScoreStateGraph_H



