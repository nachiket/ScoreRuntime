/////////////////////////////////////////////////////////////////////////////
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

#include "ScoreStateGraph.h"
#include <memory.h>

using std::ostream;

using leda::dictionary;
using leda::dic_item;
//using leda::nodeDict;
//using leda::edgeDict;

bool visualFile_complete = false;

// Implementation of ScoreStateGraphNode class

// write the contents of the data structure into specified binary file
// return true on success, false otherwise
bool ScoreStateGraphNode::write(FILE* myfile)
{
  if (fwrite(&status, sizeof(status), 1, myfile) != 1)
    return false;
  size_t name_len = strlen (name);
  if (fwrite(&name_len, sizeof(name_len), 1, myfile) != 1)
    return false;
  if (fwrite(name, sizeof (*name), name_len, myfile) != name_len)
    return false;
  if (fwrite(&label, sizeof (label), 1, myfile) != 1)
    return false;

  return true;
}

// read the specified binary file and replace the existing contents of this data structure
// return true on success, false otherwise
bool ScoreStateGraphNode::read (FILE* myfile)
{
  clear();
  if (fread(&status, sizeof(status), 1, myfile) != 1)
    return false;
  size_t name_len;
  if (fread(&name_len, sizeof(name_len), 1, myfile) != 1)
    return false;
  name = new char [name_len+1];
  memset(name, '\0', name_len+1);
  if (fread(name, sizeof (*name), name_len, myfile) != name_len)
    return false;
  if (fread(&label, sizeof (label), 1, myfile) != 1)
    return false;

  return true;
}

// node shapes and their correspondance to the status
static const char *shapes[] = {
  "box",               // NODE_STATUS_PAGE
  "rhomb",             // NODE_STATUS_SEGMENT
  "rhomb",             // NODE_STATUS_STITCH
  "ellipse",           // NODE_STATUS_CPU
  "triangle"           // NODE_STATUS_OP
};

const char *segment_modes[] = {
  "SEQSRC",
  "SEQSNK",
  "SEQSRCSNK",
  "RAMSRC",
  "RAMSNK",
  "RAMSRCSNK"
};

static const char shapes_size = sizeof(shapes) / sizeof(shapes[0]);

// write a description of this node in the format req'd by vcg
bool ScoreStateGraphNode::writeVCG(FILE* myfile, bool splitCPU, bool noCPU)
{
  if (noCPU && isStatus(NODE_STATUS_CPU)) {
    return true;
  }

  fprintf(myfile, "node: {\n");
  
  // attribute title is req'd of every node in the vcg graph, and is used
  // by the edge to identify src and dest nodes
  fprintf(myfile, "title: \"%d\"\n", label);

  // attribute label is the text that is displayed in the node
  fprintf(myfile, "label: \"%s\"\n", name);

  int i, j;
  for (i = 0, j = 1; i < shapes_size; i ++, j <<= 1)
    if (isStatus(j))
      break;

  ASSERT(i < shapes_size); // make sure that a node has a valid status entry

  fprintf(myfile, "shape: %s", shapes[i]);

  if (isStatus(NODE_STATUS_RESIDENT))
    fprintf(myfile, "color: red\n");
  
  if (isStatus(NODE_STATUS_FRONTIER))
    fprintf(myfile, "color: green\n");

  if (isStatus(NODE_STATUS_CPU))
    fprintf(myfile, "width: 75\nheight: 75\n");

  fprintf(myfile, "\n}\n");

  return true;
}

// reset the contents of the data structure (release mem if needed)
void ScoreStateGraphNode::clear()
{
  status = 0;
  label = -1;
  if (name) delete name;
  name = 0;
}

// set the name attribute of the node, by making a copy of the string
void ScoreStateGraphNode::setName(char *myname)
{
  if (name)
    delete name;

  name = new char[strlen(myname) + 1];
  strcpy (name, myname);
}

// node status
static const char *node_status_names[] = {
  "PAGE",   
  "SEGMENT",
  "STITCH", 
  "CPU",    
  "OP",
  "RESIDENT"
};

static const char node_status_names_size = sizeof(node_status_names) / sizeof(node_status_names[0]);

void ScoreStateGraphNode::print(FILE *myfile)
{
  char buf[1000];

  if (name) {
    strcpy(buf, name);
    for (int i = 0; i < 1000 && buf[i] != '\0'; i++)
      if (buf[i] == '\n') buf[i] = '\t';
  }
  else
    strcpy(buf, "-noname-");

  fprintf(myfile, "NODE: \"%s\"--%d ", buf, label);
  for (int i = 1, j = 0; j < node_status_names_size; i <<= 1, j ++)
    if (status & i)
      fprintf(myfile, "%s ", node_status_names[j]);

  fprintf(myfile, "\n");
}

// implemetation of ScoreStateGraphEdge

// write the contents of the data structure into specified binary file
// return true on success, false otherwise
bool ScoreStateGraphEdge::write(FILE* myfile)
{
  if (fwrite(&ptr_val, sizeof(ptr_val), 1, myfile) != 1)
    return false;
  if (fwrite(&produced, sizeof(produced), 1, myfile) != 1)
    return false;
  if (fwrite(&consumed, sizeof(consumed), 1, myfile) != 1)
    return false;
  if (fwrite(&source, sizeof(source), 1, myfile) != 1)
    return false;
  if (fwrite(&destination, sizeof(destination), 1, myfile) != 1)
    return false;
  if (fwrite(&status, sizeof(status), 1, myfile) != 1)
    return false;
  if (fwrite(&num_tokens, sizeof(num_tokens), 1, myfile) != 1)
    return false;
  if (fwrite(&src_num, sizeof(src_num), 1, myfile) != 1)
    return false;
  if (fwrite(&sink_num, sizeof(sink_num), 1, myfile) != 1)
    return false;

  return true;
}

// read the specified binary file and replace the existing contents of this data structure
bool ScoreStateGraphEdge::read (FILE* myfile)
{
  if (fread(&ptr_val, sizeof(ptr_val), 1, myfile) != 1)
    return false;
  if (fread(&produced, sizeof(produced), 1, myfile) != 1)
    return false;
  if (fread(&consumed, sizeof(consumed), 1, myfile) != 1)
    return false;
  if (fread(&source, sizeof(source), 1, myfile) != 1)
    return false;
  if (fread(&destination, sizeof(destination), 1, myfile) != 1)
    return false;
  if (fread(&status, sizeof(status), 1, myfile) != 1)
    return false;
  if (fread(&num_tokens, sizeof(num_tokens), 1, myfile) != 1)
    return false;
  if (fread(&src_num, sizeof(src_num), 1, myfile) != 1)
    return false;
  if (fread(&sink_num, sizeof(sink_num), 1, myfile) != 1)
    return false;

  return true;
}

// write a description of this edge in the format req'd by vcg
bool ScoreStateGraphEdge::writeVCG(FILE* myfile, bool splitCPU, bool noCPU)
{
  if (noCPU && ((destination == 0) || (destination == 1) ||
		(source == 0) || (source == 1))) {
    return true;
  }

  fprintf(myfile, "edge: {\n");
  
  if (splitCPU && (destination == 0))
    destination = 1;

  fprintf(myfile, "sourcename: \"%d\"\ntargetname: \"%d\"\n",
	  source, destination);
  
  fprintf(myfile, "label: \"");

  if (src_num != -1) {
    if (visualFile_complete) {
      fprintf(myfile, "(src=%d, snk=%d)\n", src_num, sink_num);
    } else {
      fprintf(myfile, "(%d,%d)\n", src_num, sink_num);
    }
  }

  fprintf(myfile, "%d/%d", produced, consumed);
  if(status & EDGE_STATUS_FULL)
    fprintf(myfile, "\n*FULL*");
  if(status & EDGE_STATUS_EMPTY)
    fprintf(myfile, "\n*EMPTY*");
  if (visualFile_complete) {
    fprintf(myfile, "(%d) [%u]", num_tokens, (long)ptr_val);
  }

  fprintf(myfile, "\"\n");

  if (status & EDGE_STATUS_FULL)
    fprintf(myfile, "color: green ");
  if (status & EDGE_STATUS_EMPTY)
    fprintf(myfile, "color: blue ");

//    if (status & EDGE_STATUS_PRODUCE)
//      fprintf(myfile, "arrowmark: yes ");
//    if (status & EDGE_STATUS_CONSUME)
//      fprintf(myfile, "barrowmark: yes ");

  fprintf(myfile, "\n}\n");

  return true;
}

// node status
static const char *edge_status_names[] = {
  "FULL",   
  "EMPTY"
};

static const char edge_status_names_size = sizeof(edge_status_names) / sizeof(edge_status_names[0]);


void ScoreStateGraphEdge::print(FILE* myfile)
{
  fprintf(myfile, "EDGE: src(%d) dest(%d) pro(%d) con(%d) ", source, destination, produced,
	  consumed);

  for (int i = 1, j = 0; j < edge_status_names_size; i <<= 1, j ++)
    if (status & i)
      fprintf(myfile, "%s ", edge_status_names[j]);

  fprintf(myfile, "\n");
}

// implementation of ScoreStateGraph

ScoreStateGraph::ScoreStateGraph(unsigned int _timeslice)
{
  nodeDict = new dictionary<ScoreGraphNode*, ScoreStateGraphNode*> (ScoreStateGraph::node_cmp);
  edgeDict = new dictionary<SCORE_STREAM, ScoreStateGraphEdge*> (ScoreStateGraph::edge_cmp);

  // since label 0 is designated for the CPU, other nodes start with 2
  // let's reserve 1 for the cases of split CPU
  currentLabel = 2;

  // create a CPU node
  ScoreStateGraphNode *cpuNode = new ScoreStateGraphNode(NODE_STATUS_CPU, "CPU", 0);
  nodeDict->insert(0, cpuNode);

  currentTimeslice = _timeslice; 
}

ScoreStateGraph::~ScoreStateGraph()
{
  dic_item item;

  forall_items(item, *nodeDict) delete nodeDict->inf(item);
  forall_items(item, *edgeDict) delete edgeDict->inf(item);

  delete nodeDict;
  delete edgeDict;
}


// addNode function:
// add node of the type specified by the argument and
// return a pointer to the node that was just added.
// these functions guarantee that the node is unique
// thus if an attempt is made to add a node that already
// exists, a pointer to the existing node will be returned.
ScoreStateGraphNode* ScoreStateGraph::addNode(pid_t procId, int count, ScoreGraphNode *page)
{
  dic_item item;
  item = nodeDict->lookup(page);
  if (item != NULL)
    return nodeDict->inf(item);
    
  char buf[1000];
  char aux_buf[1000];
  int status;

  switch(page->getTag()) {
  case ScorePageTag:
    if (visualFile_complete) {
      sprintf(buf, "PID %d\\nPAGE %d\\n(source=%s)\\n(state=%d) [%d]",
	      procId, count, ((ScorePage*)page)->getSource(),
	      ((ScorePage*)page)->sched_lastKnownState,
	      (long) page);
    } else {
      shortenName(((ScorePage*)page)->getSource(), aux_buf);
      sprintf(buf, "%d \'%s\'\\n(state=%d)",
	      count, aux_buf, ((ScorePage*)page)->sched_lastKnownState);
    }
    status = NODE_STATUS_PAGE;
    if (((ScorePage*)page)->sched_isResident)
      status |= NODE_STATUS_RESIDENT;
    if (page->sched_parentCluster->isFrontier)
      status |= NODE_STATUS_FRONTIER;
    break;
  case ScoreSegmentTag:
    if (visualFile_complete) {
      sprintf(buf, "PID %d\nSEGMENT %d [%d]\n%s", procId, count,
	      (long) page,
	      segment_modes[((ScoreSegment*)page)->sched_mode]);
    } else {
      sprintf(buf, "%d %s", count,
	      segment_modes[((ScoreSegment*)page)->sched_mode]);
    }
    status = NODE_STATUS_SEGMENT;
    if (((ScoreSegment*)page)->sched_isResident)
      status |= NODE_STATUS_RESIDENT;
    if (page->sched_parentCluster->isFrontier)
      status |= NODE_STATUS_FRONTIER;
    break;
  case ScoreSegmentStitchTag:
    if (visualFile_complete) {
      sprintf(buf, "PID %d\nSTITCH %d [%d]\n%s", procId, count,
	      (long) page,
	      segment_modes[((ScoreSegment*)page)->sched_mode]);
    } else {
      sprintf(buf, "S%d %s", count,
	      segment_modes[((ScoreSegment*)page)->sched_mode]);
    }
    status = NODE_STATUS_STITCH;
    if (((ScoreSegmentStitch*)page)->sched_isResident)
      status |= NODE_STATUS_RESIDENT;
    break;
  case ScoreOperatorTag:
    cerr << "ScoreStateGraph::AddNode (" << __FILE__ << ": " << __LINE__ << 
      ")-- FIXME! Add handling for operators\n";
    exit(1);
    break;
  default:
    cerr << "ScoreStateGraph::addNode -- invalid tag\n";
    exit(1);
  }

  ScoreStateGraphNode *node = 
    new ScoreStateGraphNode(status, buf, currentLabel++);
  nodeDict->insert(page, node);

  return node;
}


ScoreStateGraphEdge* ScoreStateGraph::addEdge(pid_t procId,
					      SCORE_STREAM stream)
{
  dic_item item;
  item = edgeDict->lookup(stream);
  if (item != NULL) {
    return edgeDict->inf(item);
  }

  if (!stream->sched_isStitch) {
    forall_items(item, *edgeDict) {
      if (edgeDict->key(item)->streamID == stream->streamID)
	return edgeDict->inf(item);
    }
  }

  if (stream->sched_src == 0 && stream->sched_srcFunc != STREAM_OPERATOR_TYPE)
    return 0;
  if (stream->sched_sink == 0 && stream->sched_snkFunc != STREAM_OPERATOR_TYPE)
    return 0;
  
  int status = 0;
  ScoreStateGraphNode *src =
    addNode(procId, (long)(stream->sched_src), stream->sched_src);
  ScoreStateGraphNode *dest =
    addNode(procId, (long)(stream->sched_sink), stream->sched_sink);

  if (stream->sched_isPotentiallyFull)
    status |= EDGE_STATUS_FULL;

  if (stream->sched_isPotentiallyEmpty)
    status |= EDGE_STATUS_EMPTY;

  ScoreStateGraphEdge *edge = new ScoreStateGraphEdge
    (stream, STREAM_TOKENS_PRODUCED(stream), STREAM_TOKENS_CONSUMED(stream), 
     src->getLabel(), dest->getLabel(), status, STREAM_NUMTOKENS(stream),
     stream->sched_srcNum, stream->sched_snkNum);

  edgeDict->insert(stream, edge);

  return edge;
}

bool ScoreStateGraph::addEdgeStatus(SCORE_STREAM stream, int mystatus)
{
  // FIX ME! LATER!
#if 0
  dic_item item;
  
  item = edgeDict->lookup(stream);

  if (!item && !stream->sched_isStitch) {
    forall_items(item, *edgeDict) {
      if (edgeDict->key(item)->streamID == stream->streamID)
	break;
    }
  }

  ASSERT(item != NULL);
 
  ScoreStateGraphEdge *edge = edgeDict->inf(item);

  edge->addStatus(mystatus);
#endif
  
  return true;
}

// write the graph into specified binary file
bool ScoreStateGraph::write(FILE* myfile)
{
  int magic = visualFileMagicNum;
  if (fwrite(&magic, sizeof (magic), 1, myfile) != 1)
    return false;

  if (fwrite(&visualFile_complete, sizeof(visualFile_complete), 1, myfile)
      != 1) {
    return false;
  }

  if (fwrite(&currentTimeslice, sizeof(currentTimeslice), 1, myfile) != 1)
    return false;

  int num = nodeDict->size();
  if (fwrite(&num, sizeof(num), 1, myfile) != 1)
    return false;

  dic_item item;
  forall_items(item, *nodeDict) {
    if (nodeDict->inf(item)->write(myfile) == false)
      return false;
  }

  num = edgeDict->size();
  if (fwrite(&num, sizeof(num), 1, myfile) != 1)
    return false;
  forall_items(item, *edgeDict) {
    if (edgeDict->inf(item)->write(myfile) == false)
      return false;
  }

  return true;
}

// read specified bin file to obtain a description of the graph
int ScoreStateGraph::read (FILE* myfile)
{
  clear();

  int tryMagic = 0;
  if (fread(&tryMagic, sizeof (tryMagic), 1, myfile) != 1) {
    if (feof(myfile))
      return READ_EOF;
    else
      return READ_ERROR;
  }

  if (tryMagic != visualFileMagicNum) {
    cerr << "Magic number does not match" << endl;
    return READ_ERROR;
  }

  if (fread(&visualFile_complete, sizeof(visualFile_complete), 1, myfile)
      != 1) {
    return READ_ERROR;
  }
  
  if (fread(&currentTimeslice, sizeof(currentTimeslice), 1, myfile) != 1)
    return READ_ERROR;
  
  int num;
  if (fread(&num, sizeof(num), 1, myfile) != 1)
    return READ_ERROR;

  for ( ; num > 0; num --) {
    ScoreStateGraphNode *node = new ScoreStateGraphNode();
    if (node->read(myfile) == false) {
      delete node;
      return READ_ERROR;
    }
    nodeDict->insert((ScoreGraphNode*)(node->getLabel()), node);
  }

  if (fread(&num, sizeof(num), 1, myfile) != 1)
    return READ_ERROR;
  
  for ( ; num > 0; num --) {
    ScoreStateGraphEdge *edge = new ScoreStateGraphEdge;
    if (edge->read(myfile) == false) {
      delete edge;
      return READ_ERROR;
    }
    edgeDict->insert((SCORE_STREAM)num, edge);
  }

  return READ_OK;
}

// write a description of this graph in the format req'd by vcg
bool ScoreStateGraph::writeVCG(FILE* myfile, bool splitCPU, bool noCPU)
{
  fprintf(myfile, "graph: {\n");

  fprintf(myfile, "textmode: center\n");
  fprintf(myfile, "display_edge_labels: yes\n");
  fprintf(myfile, "layoutalgorithm: dfs\n");
  fprintf(myfile, "port_sharing: no\n");
  //fprintf(myfile, "orientation: left_to_right\n");

  if (splitCPU) { 
    // first, find the cpu node and change its name
    dic_item item;
    item = nodeDict->lookup((ScoreGraphNode*)0);
    assert(item != NULL);
    ScoreStateGraphNode *cpuNode = nodeDict->inf(item);
    cpuNode->setName("CPU Out");
    
    // create a node for the CPU In and put it in the dictionary
    cpuNode = new ScoreStateGraphNode(NODE_STATUS_CPU, "CPU In", 1);
    nodeDict->insert((ScoreGraphNode*)1, cpuNode);
  } 

  fprintf(myfile, "\n");
  dic_item item;
  forall_items(item, *nodeDict)
    nodeDict->inf(item)->writeVCG(myfile, splitCPU, noCPU);

  fprintf(myfile, "\n");
  forall_items(item, *edgeDict)
    edgeDict->inf(item)->writeVCG(myfile, splitCPU, noCPU);

  fprintf(myfile, "\n}\n");

  return true;
}

void ScoreStateGraph::clear()
{
  dic_item it;
  forall_items(it, *nodeDict) delete nodeDict->inf(it);
  forall_items(it, *edgeDict) delete edgeDict->inf(it);

  nodeDict->clear();
  edgeDict->clear();
}

void ScoreStateGraph::print(FILE* myfile)
{
  fprintf(myfile, "GRAPH (slice=%u):\n%d NODES\n", currentTimeslice, nodeDict->size());
  dic_item it;
  forall_items(it, *nodeDict) nodeDict->inf(it)->print(myfile);
  fprintf(myfile, "%d EDGES\n", edgeDict->size());
  forall_items(it, *edgeDict) edgeDict->inf(it)->print(myfile);
  fprintf(myfile, "EOG\n\n");
}


// write a netlist description of edges, iff both nodes are resident
// for now exclude the processor
bool ScoreStateGraph::writeNetlist(FILE *myfile, bool uniqRes)
{
  score_vector<score_pair<int, int> > netlist;

  dic_item item;
  forall_items(item, *edgeDict) {
    int resSrc = -1, resSink = -1;
    ScoreStateGraphEdge *edge = edgeDict->inf(item);
    
    // do not bother with cpu nodes for now
    if ((edge->getSource() != 0) && (edge->getSource() != 1)) {
      dic_item srcItem = NULL;
      dic_item tmpItem;
      forall_items(tmpItem, *nodeDict) {
	if (nodeDict->inf(tmpItem)->getLabel() == edge->getSource()) {
	  assert(srcItem == NULL); // assume no repetition in labels
	  srcItem = tmpItem;
	}
      }
      assert(srcItem != NULL);
      ScoreStateGraphNode *stateGraphNode = nodeDict->inf(srcItem);
      assert(stateGraphNode);
      if (stateGraphNode->isStatus(NODE_STATUS_RESIDENT)) {
	ScoreGraphNode *scoreGraphNode = nodeDict->key(srcItem);
	assert(scoreGraphNode);
	assert(scoreGraphNode->sched_isResident);
	int tmpLoc = scoreGraphNode->sched_residentLoc;
	if (uniqRes) {
	  resSink = (int) (scoreGraphNode->isSegment() ?
	    (-scoreGraphNode->uniqTag-10) : scoreGraphNode->uniqTag+10);
	} else {
	  resSink = scoreGraphNode->isSegment() ? 2 * tmpLoc + 1 : 2 * tmpLoc;
	}
      }
    }

    // do not bother with cpu nodes for now
    if ((edge->getDestination() != 0) && (edge->getDestination() != 1)) {
      dic_item sinkItem = NULL;
      dic_item tmpItem;
      forall_items(tmpItem, *nodeDict) {
	if (nodeDict->inf(tmpItem)->getLabel() == edge->getDestination()) {
	  assert(sinkItem == NULL); // assume no repetition in labels
	  sinkItem = tmpItem;
	}
      }
      assert(sinkItem != NULL);
      ScoreStateGraphNode *stateGraphNode = nodeDict->inf(sinkItem);
      assert(stateGraphNode);
      if (stateGraphNode->isStatus(NODE_STATUS_RESIDENT)) {
	ScoreGraphNode *scoreGraphNode = nodeDict->key(sinkItem);
	assert(scoreGraphNode);
	assert(scoreGraphNode->sched_isResident);
	int tmpLoc = scoreGraphNode->sched_residentLoc;
	if (uniqRes) {
	  resSrc = (int) (scoreGraphNode->isSegment() ? 
	    (-scoreGraphNode->uniqTag-10) : scoreGraphNode->uniqTag+10);
	} else {
	  resSrc = scoreGraphNode->isSegment() ? 2 * tmpLoc + 1 : 2 * tmpLoc;
	}
      }
    }
    
    if ((resSrc != -1) && (resSink != -1)) {
      netlist.push_back(score_pair<int,int>(resSrc,resSink));
    }
  }
  
  // now that the vector is built emit it to the file

  fprintf(myfile, "%d\n", netlist.size());

  for (int i = 0; i < netlist.size(); i++) {
    score_pair<int, int> p = netlist.at(i);
    fprintf(myfile, "%d\t%d\n", p.first, p.second);
  }
  
  return true;
}

void ScoreStateGraph::shortenName(const char *src, char *snk)
{
  const char *ptr = strstr(src, "in");

  // if in was found
  if (ptr) {
    strncpy(snk, src, ptr-src);
    snk[ptr-src-1] = '\0';
    return;
  }

  if (strncmp(src, "internal ", 9) == 0) {
    if (strstr(src, "copy")) {
      strcpy(snk, "copy");
    } else {
      strcpy(snk, snk + 9); // skip "internal"
    }
    return;
  }

  strcpy(snk, src);
}
