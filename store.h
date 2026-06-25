#pragma once
#include <string>
#include <list>
#include <map> 
#include <mutex>

using namespace std;

// Represents a single key-value record in the memory tree
struct CacheNode 
{
    string data;
    long long expiration_time;       // unix timestamp, -1 means infinite
    list<string>::iterator queue_ptr; // O(1) pointer into the eviction queue
};

// Safe, stripped-down record for generating persistence dumps
struct SnapshotRecord 
{
    string data;
    long long expiration_time;
};

class Store 
{
public:
    Store(int capacity = 1000);

    void set(const string &key, const string &val, int ttl = -1);
    string get(const string &key);
    int del(const string &key);
    int size();

    // will bypass standard TTL calculation for direct memory loading
    void setRaw(const string &key, const string &val, long long expire_at);
    
    // returns the entire tree for persistence
    map<string, SnapshotRecord> getAll(); 

private:
    map<string, CacheNode> storage_tree; // core memory uses a RB Tree
    list<string> eviction_queue;         // doubly linked list tracking LRU
    int max_capacity;
    mutex store_lock; 
    
    long long getCurrentTime();
    void updateAccess(const string &key);
    void removeOldest();
};