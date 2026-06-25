#pragma once
#include <string>
#include <fstream>

using namespace std;

class Store;

class WAL
{
public:
    WAL(const string &filepath);
    ~WAL();
    
    void log(const string &operation);
    void replay(Store &database);
    void clear();

private:
    string log_filepath;
    ofstream file_stream;
    int unflushed_writes = 0; // Tracks pending operations to amortize I/O cost
};