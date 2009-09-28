#include "ScoreStream.h"
#include <iostream>
#include <cstdio>
#include <cstdlib>

#include "add8.h"

using namespace std;

int main(int argc, char *argv[]) {

    score_init();

    UNSIGNED_SCORE_STREAM  a     = NEW_READ_UNSIGNED_SCORE_STREAM(6);
    UNSIGNED_SCORE_STREAM  b     = NEW_READ_UNSIGNED_SCORE_STREAM(6);
    //UNSIGNED_SCORE_STREAM  c     = NEW_WRITE_UNSIGNED_SCORE_STREAM(6);
    UNSIGNED_SCORE_STREAM  c; //     = NEW_WRITE_UNSIGNED_SCORE_STREAM(6);
    
    cout << "Finished defining streams!!" << endl;

    c=add8(a,b);

    cout << "Defined the operator" << endl;

    for(int i=0; i<6;i++) {
        cout << "Updating stream with value " << i << endl;
    	STREAM_WRITE(a, i);
    	STREAM_WRITE(b, i);
    }
    STREAM_CLOSE(a);
    STREAM_CLOSE(b);

    cout << "Finished populating streams!!" << endl;


    cout << "Finished evaluating operator" << endl;

    // Dump stream into file
    while (!STREAM_EOS(c)) {
        unsigned char byte;
        STREAM_READ(c, byte);
        cout << "Byte=" << (int)(byte) << endl;
    }

    STREAM_FREE(c);

    score_exit();
}

