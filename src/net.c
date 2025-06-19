#include "net.h"

#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define INITIAL_RECIEVE_BUFFER_SIZE 1024
#define MAX_RECIEVE_SIZE            65536

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
 * @return data on success, NULL on error or client disconnection
 */
char* receive_data(int clientfd) {
    size_t total_size  = INITIAL_RECIEVE_BUFFER_SIZE;
    size_t bytes_total = 0;
    char*  buffer      = malloc(total_size);
    if(!buffer) return NULL;

    ssize_t bytes_read;
    while(1) {
        if(bytes_total >= total_size - 1) {
            // Grow buffer (double strategy)
            size_t new_size = total_size * 2;
            if(new_size > MAX_RECIEVE_SIZE) {
                fprintf(stderr, "Request too large\n");
                free(buffer);
                return NULL;
            }
            char* new_buffer = realloc(buffer, new_size);
            if(!new_buffer) {
                perror("realloc");
                free(buffer);
                return NULL;
            }
            buffer     = new_buffer;
            total_size = new_size;
        }

        bytes_read =
            read(clientfd, buffer + bytes_total, total_size - bytes_total - 1);
        if(bytes_read <= 0) {
            break;
        }

        bytes_total         += bytes_read;
        buffer[bytes_total]  = '\0';  // Null-terminate

        // Check for end of message if protocol defines it (e.g., newline)
        // if(strchr(buffer, '\n')) break;
    }

    // printf("Received: %s\n", buffer);
    return buffer;
}
