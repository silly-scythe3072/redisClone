#include "persistence.h"
#include <fstream>
#include <sstream>

using namespace std;

void saveToDisk(Store &database, const string &filepath)
{
    // getAll() returns map<string, SnapshotRecord>, safe from tree rotations
    auto records = database.getAll(); 
    ofstream out(filepath, ios::trunc);
    
    for (auto &[key, record] : records)
    {
        out << key << "\t"
            << record.data << "\t"
            << record.expiration_time << "\n";
    }
    out.flush();
}

void loadFromDisk(Store &database, const string &filepath)
{
    ifstream in(filepath);
    if (!in.is_open())
        return;

    string line;
    while (getline(in, line))
    {
        if (line.empty())
            continue;
            
        istringstream ss(line);
        string key, val;
        long long expiration;
        
        getline(ss, key, '\t');
        getline(ss, val, '\t');
        ss >> expiration;
        
        database.setRaw(key, val, expiration); 
    }
}