#pragma once
#include <string>
#include "store.h"

using namespace std;

// Dumps the current state of the Red-Black tree to disk.
// Format: key, value, expiration_timestamp (tab-separated)
void saveToDisk(Store &database, const string &filepath);

// Loads a previously saved memory state back into the tree.
// To bypasses standard TTL constraints to restore exact expiration times.
void loadFromDisk(Store &database, const string &filepath);