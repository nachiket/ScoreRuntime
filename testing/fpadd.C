#include "ScoreStream.h"
#include <iostream>
#include <cstdio>
#include <cstdlib>

#include "fpadd.h"

using namespace std;

int main(int argc, char *argv[]) {

    score_init();

    DOUBLE_SCORE_STREAM  a = NEW_DOUBLE_SCORE_STREAM();
    DOUBLE_SCORE_STREAM  b = NEW_DOUBLE_SCORE_STREAM();
    BOOLEAN_SCORE_STREAM c = NEW_BOOLEAN_SCORE_STREAM();
    DOUBLE_SCORE_STREAM  result; 
    
    result=fpadd(a,b,c);

    cout << "Finished setting up the dataflow graph!!" << endl;

    for(int i=0; i<6;i++) {
        cout << "Updating stream with value " << i << endl;
    	STREAM_WRITE(a, i*1e6);
    	STREAM_WRITE(b, i*1e-6);
        //STREAM_WRITE(c, false);
        STREAM_WRITE(c, true);
    }

    STREAM_CLOSE(a);
    STREAM_CLOSE(b);
    STREAM_CLOSE(c);

    cout << "Finished populating streams!!" << endl;

    // Dump stream into file
    while (!STREAM_EOS(result)) {
        long long int value;
        STREAM_READ(result, value);
        cout << "Byte=" << value << endl;
    }

    cout << "Finished reading streams!!" << endl;

    STREAM_FREE(result);

    score_exit();
}

