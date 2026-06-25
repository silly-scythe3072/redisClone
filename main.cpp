/*
 * Main entry point for the Key-Value Store.
 * Parses initial setup, loads persistent data, and executes core commands.
 */
#include <iostream>
#include "store.h"
#include "persistence.h" 

using namespace std;

int main()
{
    Store db(1000); // Initialize your custom Red-Black Tree store
    string dump_file = "database_state.dat"; // Professional file extension

    cout << "[INFO] Booting Cache Server...\n";
    
    // 1. Load previous state from disk on startup
    cout << "[INFO] Attempting to load persistent data...\n";
    loadFromDisk(db, dump_file);
    cout << "[INFO] Currently holding " << db.size() << " keys in memory.\n\n";

    // 2. Execute operations
    db.set("name", "alice");
    db.set("city", "delhi");
    db.set("session", "tok123", 3); // 3 second TTL

    cout << "> GET name: " << db.get("name") << "\n";
    cout << "> GET missing: " << db.get("missing") << "\n";
    
    db.del("city");
    cout << "> GET city (after delete): " << db.get("city") << "\n\n";

    // 3. Save state to disk before exiting
    cout << "[INFO] Saving tree state to disk before shutdown...\n";
    saveToDisk(db, dump_file);
    cout << "[INFO] Shutdown complete.\n";

    return 0;
}