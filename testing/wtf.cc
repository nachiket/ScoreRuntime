#include <cstdlib>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <ios>
#include <iomanip>

using namespace std;
using std::fstream;
using std::setfill;
using std::setw;
using std::ios;

int main(int argc, char* argv[]) {

	int tempID=0;

	/*
	   fstream outfile("/tmp/streamid",ios::out);
	   outfile << setfill('0');
	   outfile << setw(4);
	   outfile << 0;
	   outfile.close();
	 */

	// need to get new streamID
	fstream infile("/tmp/streamid",ios::in);

	if (infile.fail() || infile.bad()) {
		cout << "Initialize /tmp/streamid" << endl;
		infile.close();
		// initialize the file..
		fstream outfile("/tmp/streamid",ios::out);
		outfile << setfill('0');
		outfile << setw(4);
		outfile << 0;
		outfile.close();
	} else {
		char* buffer=new char[4];
		infile.read(buffer,4);
		tempID=(buffer[3]-48)+
			10*(buffer[2]-48)+
			100*(buffer[1]-48)+
			1000*(buffer[0]-48);
		cout << "Found existing /tmp/streamid: " << tempID << endl;
		infile.close();
	}


	// Nachiket added update routine...
	// need to get new streamID
	fstream outfile("/tmp/streamid",ios::out);
	outfile << setfill('0');
	outfile << setw(4);
	int stupID=tempID+1;
	outfile << stupID;
	outfile.close();

}
