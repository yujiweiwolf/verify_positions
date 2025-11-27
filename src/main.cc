#include "verify_broker.h"
using namespace co;
using namespace std;

int main(int argc, char** argv) {
    cout << "please input 2个wal文件, 一个交易, 一个查询" << endl;
    string file1 = argv[1];
    string file2 = argv[2];
    VerifyBroker verify;
    verify.Init(file1, file2);
	return 0;
}

