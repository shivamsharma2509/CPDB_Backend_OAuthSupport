#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include "auth.h"  // Assuming you have a separate auth.h header

#define PORT 8080

// Function prototypes
void start_local_server();
void handle_client_request(int client_socket);
void initiate_oauth_flow();

int main() {
    // Start the OAuth flow
    initiate_oauth_flow();

    // Start the local server
    start_local_server();

    return 0;
}

void initiate_oauth_flow() {
    // Your existing OAuth flow logic
    // ...
}

void start_local_server() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    // Create socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Attach socket to the port
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Bind the socket to the port
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Start listening for incoming connections
    if (listen(server_fd, 3) < 0) {
        perror("listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server started on port %d\n", PORT);

    // Accept and handle incoming connections
    while (1) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
            perror("accept failed");
            close(server_fd);
            exit(EXIT_FAILURE);
        }
        handle_client_request(new_socket);
        close(new_socket);
    }
}

void handle_client_request(int client_socket) {
    char buffer[1024] = {0};
    read(client_socket, buffer, 1024);

    // Simple HTTP response (modify this based on your OAuth requirements)
    const char *response = "HTTP/1.1 200 OK\nContent-Type: text/plain\nContent-Length: 12\n\nHello World";
    write(client_socket, response, strlen(response));

    // You can parse the buffer to handle OAuth redirection or tokens
    // For example, extract the authorization code from the query parameters
}

