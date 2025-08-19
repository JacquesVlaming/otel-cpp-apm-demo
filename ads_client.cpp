#include <iostream>
#include <string>
#include <arpa/inet.h>
#include <unistd.h>

int main() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(5000);
    inet_pton(AF_INET, "127.0.0.1", &server_address.sin_addr);

    connect(sock, (struct sockaddr*)&server_address, sizeof(server_address));

    std::string msg = "Hello ADS Server!";
    send(sock, msg.c_str(), msg.size(), 0);

    char buffer[1024] = {0};
    read(sock, buffer, 1024);

    std::cout << "Server responded: " << buffer << std::endl;

    close(sock);
    return 0;
}
