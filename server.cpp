/*
 * Core TCP Server implementation for Windows.
 * Handles Winsock initialization, network binding, and the multithreaded event loop.
 */
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <algorithm>
#include <thread>
#include <winsock2.h>
#include <ws2tcpip.h>

#include "store.h"
#include "wal.h"
#include "persistence.h"

// Directs the MSVC/MinGW compiler to link the Windows Socket library
#pragma comment(lib, "Ws2_32.lib")

using namespace std;

#define PORT 6379

vector<string> splitCmd(const string &s)
{
    vector<string> out;
    istringstream ss(s);
    string w;
    while (ss >> w)
        out.push_back(w);
    return out;
}

string handleCommand(Store &db, WAL &wal, const string &raw)
{
    auto p = splitCmd(raw);
    if (p.empty())
        return "";

    string cmd = p[0];
    for (auto &c : cmd)
        c = toupper(c);
        
    if (cmd == "PING")
        return "+PONG\r\n";

    if (cmd == "SET")
    {
        if (p.size() < 3)
            return "-ERR wrong args\r\n";
        int ttl = -1;
        if (p.size() >= 5)
        {
            string ex = p[3];
            for (auto &c : ex)
                c = toupper(c);
            if (ex == "EX")
                ttl = stoi(p[4]);
        }
        db.set(p[1], p[2], ttl);
        
        // Log to WAL before returning to prevent write-loss on crash
        wal.log("SET " + p[1] + " " + p[2] + (ttl > 0 ? " " + to_string(ttl) : ""));
        return "+OK\r\n";
    }

    if (cmd == "GET")
    {
        if (p.size() < 2)
            return "-ERR wrong args\r\n";
        string val = db.get(p[1]);
        if (val == "(nil)")
            return "$-1\r\n";
            
        // RESP format: $<length>\r\n<value>\r\n
        return "$" + to_string(val.size()) + "\r\n" + val + "\r\n";
    }

    if (cmd == "DEL")
    {
        if (p.size() < 2)
            return "-ERR wrong args\r\n";
        int n = db.del(p[1]);
        if (n > 0)
            wal.log("DEL " + p[1]);
        return ":" + to_string(n) + "\r\n"; 
    }

    if (cmd == "DBSIZE")
        return ":" + to_string(db.size()) + "\r\n";

    return "-ERR unknown command '" + p[0] + "'\r\n";
}

void handleClient(SOCKET client_fd, Store &db, WAL &wal)
{
    char buf[1024];
    string leftover;

    while (true)
    {
        int n = recv(client_fd, buf, sizeof(buf) - 1, 0);
        if (n <= 0)
            break;
            
        buf[n] = '\0';
        leftover += buf;

        size_t pos;
        while ((pos = leftover.find('\n')) != string::npos)
        {
            string line = leftover.substr(0, pos);
            leftover = leftover.substr(pos + 1);
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            if (line.empty())
                continue;

            string resp = handleCommand(db, wal, line);
            if (!resp.empty())
                send(client_fd, resp.c_str(), resp.size(), 0);
        }
    }
}

int main()
{
    // 1. Initialize Windows Networking Hardware
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cerr << "[ERROR] Winsock initialization failed.\n";
        return 1;
    }

    Store db(100000);
    WAL wal("server_log.wal");

    // 2. Load Red-Black Tree persistence and replay WAL
    loadFromDisk(db, "database_state.dat");
    wal.replay(db);

    cout << "[INFO] Loaded " << db.size() << " keys from disk into Red-Black Tree.\n";

    // 3. Create Windows Socket
    SOCKET server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == INVALID_SOCKET)
    {
        cerr << "[ERROR] Socket creation failed.\n";
        WSACleanup();
        return 1;
    }

    // SO_REUSEADDR prevents "address already in use" errors on rapid restarts
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    if (bind(server_fd, (sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR)
    {
        cerr << "[ERROR] Port bind failed.\n";
        closesocket(server_fd);
        WSACleanup();
        return 1;
    }

    listen(server_fd, 10);
    cout << "[INFO] redisClone server listening on port " << PORT << "...\n";

    // 4. Main Event Loop
    while (true)
    {
        sockaddr_in client_addr{};
        int client_len = sizeof(client_addr);

        SOCKET client_fd = accept(server_fd, (sockaddr *)&client_addr, &client_len);
        if (client_fd == INVALID_SOCKET)
            continue;

        // Spawn a detached thread for every incoming client connection
        thread([client_fd, &db, &wal]() {
            handleClient(client_fd, db, wal);
            closesocket(client_fd); 
        }).detach();
    }

    closesocket(server_fd);
    WSACleanup();
    return 0;
}