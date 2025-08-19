#include <iostream>
#include <string>
#include <thread>
#include <netinet/in.h>
#include <unistd.h>

void handle_client(int client_socket) {
    char buffer[1024] = {0};
    int bytes = read(client_socket, buffer, 1024);
    if (bytes > 0) {
        std::string request(buffer, bytes);
        std::cout << "Received: " << request << std::endl;

        std::string response = "Hello from ADS! You sent: " + request;
        send(client_socket, response.c_str(), response.size(), 0);
    }
    close(client_socket);
}

int main() {
    int server_fd, client_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(5000);

    bind(server_fd, (struct sockaddr*)&address, sizeof(address));
    listen(server_fd, 5);

    std::cout << "ADS Hello World Server listening on port 5000..." << std::endl;

    while (true) {
        client_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
        std::thread(handle_client, client_socket).detach();
    }

    return 0;
}
