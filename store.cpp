#include "store.h"
#include <chrono>

using namespace std;

Store::Store(int capacity) : max_capacity(capacity) {}

long long Store::getCurrentTime()
{
    using namespace chrono;
    return duration_cast<seconds>(
               system_clock::now().time_since_epoch())
        .count();
}

// Moves a key to the front of the eviction queue.
// Deleting from the tree is O(log N), but moving in the queue remains O(1)
void Store::updateAccess(const string &key)
{
    auto node_ptr = storage_tree.find(key);
    if (node_ptr == storage_tree.end())
        return;
        
    eviction_queue.erase(node_ptr->second.queue_ptr);
    eviction_queue.push_front(key);
    node_ptr->second.queue_ptr = eviction_queue.begin();
}

// Evicts the Least Recently Used (LRU) node when capacity is breached.
void Store::removeOldest()
{
    if (eviction_queue.empty())
        return;
        
    string oldest_key = eviction_queue.back();
    eviction_queue.pop_back();
    storage_tree.erase(oldest_key);
}

void Store::set(const string &key, const string &val, int ttl)
{
    lock_guard<mutex> lock(store_lock);
    
    // To check if key exists in the Red-Black tree (O(log N) lookup)
    auto node_ptr = storage_tree.find(key);
    if (node_ptr != storage_tree.end())
    {
        eviction_queue.erase(node_ptr->second.queue_ptr);
        storage_tree.erase(node_ptr);
    }

    if ((int)storage_tree.size() >= max_capacity)
        removeOldest();

    eviction_queue.push_front(key);
    long long exp = (ttl > 0) ? getCurrentTime() + ttl : -1;
    
    // Insert new CacheNode into the tree
    storage_tree[key] = {val, exp, eviction_queue.begin()};
}

string Store::get(const string &key)
{
    lock_guard<mutex> lock(store_lock);
    auto node_ptr = storage_tree.find(key);
    if (node_ptr == storage_tree.end())
        return "(nil)";

    CacheNode &node = node_ptr->second;
    
    // Lazy Evaluation: Only check for expiration when read is attempted
    if (node.expiration_time != -1 && getCurrentTime() > node.expiration_time)
    {
        eviction_queue.erase(node.queue_ptr);
        storage_tree.erase(node_ptr);
        return "(nil)";
    }

    updateAccess(key); 
    return node.data;
}

int Store::del(const string &key)
{
    lock_guard<mutex> lock(store_lock);
    auto node_ptr = storage_tree.find(key);
    if (node_ptr == storage_tree.end())
        return 0;
        
    eviction_queue.erase(node_ptr->second.queue_ptr);
    storage_tree.erase(node_ptr);
    return 1;
}

int Store::size()
{
    lock_guard<mutex> lock(store_lock);
    return storage_tree.size();
}

void Store::setRaw(const string &key, const string &val, long long expire_at)
{
    lock_guard<mutex> lock(store_lock);

    auto existing = storage_tree.find(key);
    if (existing != storage_tree.end())
    {
        eviction_queue.erase(existing->second.queue_ptr);
        storage_tree.erase(existing);
    }

    eviction_queue.push_front(key);
    storage_tree[key] = {val, expire_at, eviction_queue.begin()};
}

map<string, SnapshotRecord> Store::getAll()
{
    lock_guard<mutex> lock(store_lock);
    map<string, SnapshotRecord> result;
    for (auto &[key, node] : storage_tree)
    {
        result[key] = {node.data, node.expiration_time};
    }
    return result;
}