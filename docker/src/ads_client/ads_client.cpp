#include <iostream>
#include <string>
#include <arpa/inet.h>
#include <netdb.h>      // for gethostbyname
#include <unistd.h>

int main() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    struct hostent* server = gethostbyname("ads_server");
    if (!server) {
        std::cerr << "Error: could not resolve host" << std::endl;
        return 1;
    }

    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(5000);
    server_address.sin_addr = *((struct in_addr*)server->h_addr);

    if (connect(sock, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
        perror("connect");
        return 1;
    }

    std::string msg = "Hello ADS Server!";
    send(sock, msg.c_str(), msg.size(), 0);

    char buffer[1024] = {0};
    read(sock, buffer, 1024);

    std::cout << "Server responded: " << buffer << std::endl;

    close(sock);
    return 0;
}
