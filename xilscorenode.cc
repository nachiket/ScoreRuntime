#include "xilscorestream.h"
#include "xilscorenode.h"

ScoreGraphNode::ScoreGraphNode() {
  inputs = 0;
  outputs = 0;
} 


ScoreGraphNode::~ScoreGraphNode() {
  int i;
  for (i = 0; i < inputs; i++) {
    SCORE_STREAM currentStream = in[i];

    if (currentStream != NULL) {
      STREAM_FREE(currentStream);
    }
  }
  for (i = 0; i < outputs; i++) {
    SCORE_STREAM currentStream = out[i];
    
    if (currentStream != NULL) {
      STREAM_CLOSE(currentStream);
    }
  }

  delete(in);
  delete(out);
  delete(in_types);
  delete(out_types);

}

