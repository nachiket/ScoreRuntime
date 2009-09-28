#include "ScoreStream.h"
#include <iostream>
#include <cstdio>
#include <cstdlib>

#include "add8.h"

using namespace std;

int main(int argc, char *argv[]) {

    score_init();

    UNSIGNED_SCORE_STREAM  a     = NEW_READ_UNSIGNED_SCORE_STREAM(256);
    UNSIGNED_SCORE_STREAM  b     = NEW_READ_UNSIGNED_SCORE_STREAM(256);
    UNSIGNED_SCORE_STREAM  c     = NEW_WRITE_UNSIGNED_SCORE_STREAM(256);

    //ScoreOperator* op = (ScoreOperator*)NEW_nonfunc_add8(a, b);
    //c=op->getResult();
    NEW_nonfunc_add8(a,b);

    // Dump stream into file
    while (!STREAM_EOS(c)) {
        unsigned char byte;
        STREAM_READ(c, byte);
        cout << "Byte=" << byte << endl;
    }

    STREAM_FREE(c);

    score_exit();
}

