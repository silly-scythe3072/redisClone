#include <iostream>
#include <string>
#include <chrono>
#include <winsock2.h>
#include <ws2tcpip.h>

using namespace std;

SOCKET establish_connection(int port)
{
    // Windows requires waking up the networking hardware first
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cerr << "Winsock initialization failed.\n";
        exit(1);
    }

    SOCKET sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd == INVALID_SOCKET) {
        cerr << "Socket creation failed.\n";
        WSACleanup();
        exit(1);
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    if (connect(sock_fd, (sockaddr *)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        cerr << "Connection to server failed.\n";
        closesocket(sock_fd);
        WSACleanup();
        exit(1);
    }

    return sock_fd;
}

double run_benchmark(SOCKET sock_fd, const string &command, int iterations)
{
    char buffer[256];
    auto start_time = chrono::high_resolution_clock::now();
    
    for (int i = 0; i < iterations; i++)
    {
        send(sock_fd, command.c_str(), command.size(), 0);
        recv(sock_fd, buffer, sizeof(buffer), 0);
    }
    
    auto end_time = chrono::high_resolution_clock::now();
    return chrono::duration<double, milli>(end_time - start_time).count();
}

int main()
{
    const int total_operations = 100000;
    SOCKET client_socket = establish_connection(6379);

    cout << "[BENCHMARK] Testing redisClone performance (" << total_operations << " operations per metric)\n\n";

    run_benchmark(client_socket, "PING\n", 1000); 

    double set_duration = run_benchmark(client_socket, "SET benchkey benchvalue\n", total_operations);
    double set_throughput = total_operations / (set_duration / 1000.0);
    cout << "[METRIC] SET:  " << (int)set_throughput << " req/sec"
         << "  (" << set_duration / total_operations << " ms/op)\n";

    double get_duration = run_benchmark(client_socket, "GET benchkey\n", total_operations);
    double get_throughput = total_operations / (get_duration / 1000.0);
    cout << "[METRIC] GET:  " << (int)get_throughput << " req/sec"
         << "  (" << get_duration / total_operations << " ms/op)\n";

    double ping_duration = run_benchmark(client_socket, "PING\n", total_operations);
    double ping_throughput = total_operations / (ping_duration / 1000.0);
    cout << "[METRIC] PING: " << (int)ping_throughput << " req/sec"
         << "  (" << ping_duration / total_operations << " ms/op)\n";

    closesocket(client_socket);
    WSACleanup(); // Turn off Windows internet hardware
    return 0;
}