#include "wal.h"
#include "store.h"
#include <fstream>
#include <sstream>

using namespace std;

WAL::WAL(const string &filepath) : log_filepath(filepath)
{
    file_stream.open(log_filepath, ios::app); // Append mode to protect existing recovery data
}

WAL::~WAL()
{
    if (file_stream.is_open())
        file_stream.close();
}

void WAL::log(const string &operation)
{
    if (!file_stream.is_open())
        return;
        
    file_stream << operation << "\n";
    unflushed_writes++;
    
    // Amortizing Disk I/O: 
    // Flushing on every single write creates a massive OS bottleneck.
    // By batching 100 writes per flush, we drastically improve throughput
    // while maintaining a very small failure window (max 100 lost operations).
    if (unflushed_writes % 100 == 0)
    {
        file_stream.flush();
        unflushed_writes = 0;
    }
}

// Restores the Red-Black tree state from the sequential log on boot
void WAL::replay(Store &database)
{
    ifstream in(log_filepath);
    if (!in.is_open())
        return; // No recovery file found, starting fresh

    string line;
    while (getline(in, line))
    {
        if (line.empty())
            continue;
            
        istringstream ss(line);
        string command, key, val;
        int ttl = -1;
        
        ss >> command >> key;
        
        if (command == "SET")
        {
            ss >> val >> ttl;
            database.set(key, val, ttl); // Re-inserts into the tree
        }
        else if (command == "DEL")
        {
            database.del(key);
        }
    }
}

// Resets the log after a full persistence dump is saved to disk.
// Once the tree state is safely on disk, the sequential log is redundant.
void WAL::clear()
{
    if (file_stream.is_open())
        file_stream.close();
    file_stream.open(log_filepath, ios::trunc); // Overwrites with an empty file
}