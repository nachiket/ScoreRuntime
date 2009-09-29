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
    BOOLEAN_SCORE_STREAM d = NEW_READ_BOOLEAN_SCORE_STREAM();
    UNSIGNED_SCORE_STREAM  c; //     = NEW_WRITE_UNSIGNED_SCORE_STREAM(6);
    
    cout << "Finished defining streams!!" << endl;

    c=add8(a,b,d);

    cout << "Defined the operator" << endl;

    for(int i=0; i<6;i++) {
        cout << "Updating stream with value " << i << endl;
    	STREAM_WRITE(a, i);
    	STREAM_WRITE(b, i);
        //STREAM_WRITE(d, false);
        STREAM_WRITE(d, true);
    }

    STREAM_CLOSE(a);
    STREAM_CLOSE(b);
    STREAM_CLOSE(d);

    cout << "Finished populating streams!!" << endl;

    // Dump stream into file
    while (!STREAM_EOS(c)) {
        unsigned char byte;
        STREAM_READ(c, byte);
        cout << "Byte=" << (int)(byte) << endl;
    }

    cout << "Finished reading streams!!" << endl;

    STREAM_FREE(c);

    score_exit();
}

