/*
 * Main entry point for the Key-Value Store.
 * Parses any initial setup and launches the primary server instance.
 */
#include <iostream>
#include "store.h"
using namespace std;

int main()
{
    Store db;

    db.set("name", "alice");
    db.set("city", "delhi");
    db.set("session", "tok123", 3);

    cout << db.get("name") << "\n";
    cout << db.get("missing") << "\n";
    cout << db.size() << "\n";

    db.del("city");
    cout << db.get("city") << "\n";
    cout << db.size() << "\n";

    return 0;
}