/*
 * Core TCP Server implementation.
 * Handles socket creation, network binding, and the main event loop for client connections.
 */
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <algorithm>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "store.h"
#include <thread>
#include "wal.h"
#include "snapshot.h"
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
        // log to WAL before returning — if we crash after set() but before log(),
        // we lose the write. order matters here.
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
        return ":" + to_string(n) + "\r\n"; // n not db.del() again — key is already gone
    }

    if (cmd == "DBSIZE")
        return ":" + to_string(db.size()) + "\r\n";

    return "-ERR unknown command '" + p[0] + "'\r\n";
}

void handleClient(int client_fd, Store &db, WAL &wal)
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

        // TCP doesn't guarantee a full command arrives in one recv() call
        // buffer everything and split on newlines to get complete commands
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
    Store db(100000);
    WAL wal("redis.wal");

    // startup order matters: snapshot first (bulk), then WAL on top (recent changes)
    loadSnapshot(db, "redis.snap");
    wal.replay(db);

    cout << "Loaded " << db.size() << " keys from disk.\n";

    // without SO_REUSEADDR, restarting the server within ~60s fails with
    // "address already in use" because the OS holds the port in TIME_WAIT
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    if (bind(server_fd, (sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind");
        return 1;
    }

    listen(server_fd, 10);
    cout << "Mini Redis listening on port " << PORT << "...\n";

    while (true)
    {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(server_fd, (sockaddr *)&client_addr, &client_len);
        if (client_fd < 0)
            continue;

        // one thread per client — simple and correct for this scale
        // downside: 10k clients = 10k threads = bad
        // proper fix would be a fixed thread pool with a job queue
        thread([client_fd, &db, &wal]()
               {
    handleClient(client_fd, db, wal);
    close(client_fd); })
            .detach();
    }

    close(server_fd);
    return 0;
}