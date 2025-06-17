#include "net.h"

#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

/**
 * Creates a socket server and binds it, listening on the given port and returns
 * the file descriptor of it. returns -1 if error.
 */
int create_server(int port) {
    int                serverfd, res;
    struct sockaddr_in address;

    serverfd = socket(AF_INET, SOCK_STREAM, 0);
    if(serverfd == -1) {
        perror("socket");
        return -1;
    }

    address.sin_family      = AF_INET;
    address.sin_port        = htons(port);
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    memset(&(address.sin_zero), 0, 8);

    res =
        bind(serverfd, (struct sockaddr*)&address, sizeof(struct sockaddr_in));
    if(res == -1) {
        perror("bind");
        close(serverfd);
        return -1;
    }

    res = listen(serverfd, MAX_CONNECTIONS);
    if(res == -1) {
        perror("listen");
        close(serverfd);
        return -1;
    }

    return serverfd;
}

/**
 * Accepts a connection in the server.
 */
int accept_connection(int serverfd) {
    socklen_t          len;
    int                clientfd;
    struct sockaddr_in clientAddress;

    len      = sizeof(struct sockaddr_in);
    clientfd = accept(serverfd, (struct sockaddr*)&clientAddress, &len);
    if(clientfd == -1) {
        perror("accept");
    }

    return clientfd;
}

/**
 * Handles client communication
 * @param clientfd The client socket file descriptor
 * @return 0 on success, -1 on error or client disconnection
 */
int recieve_data(int clientfd) {
    char    buffer[RECIEVE_BUFFER_SIZE];
    ssize_t bytes_read;

    // Clear the buffer
    memset(buffer, 0, RECIEVE_BUFFER_SIZE);

    // Read data from client
    bytes_read = read(clientfd, buffer, RECIEVE_BUFFER_SIZE - 1);
    if(bytes_read <= 0) {
        if(bytes_read == 0) {
            printf("Client disconnected\n");
        } else {
            perror("Error reading from client");
        }
        return -1;
    }

    buffer[bytes_read] = '\0';  // Ensure null termination
    printf("Received from client: %s\n", buffer);

    // Echo back the data to the client
    if(write(clientfd, buffer, bytes_read) != bytes_read) {
        perror("Error sending response to client");
        return -1;
    }

    return 0;
}
