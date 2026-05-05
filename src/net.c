#include "net.h"

#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "utils/log.h"

#define INITIAL_RECIEVE_BUFFER_SIZE 1024
#define MAX_RECIEVE_SIZE            65536

/**
 * Creates a socket server and binds it, listening on the given port and returns
 * the file descriptor of it. returns -1 if error.
 */
int create_server(int port, int backlog) {
    int                serverfd, res;
    struct sockaddr_in address;
    int                opt = 1;

    if(backlog <= 0) {
        backlog = DEFAULT_MAX_CONNECTIONS;
    }

    serverfd = socket(AF_INET, SOCK_STREAM, 0);
    if(serverfd == -1) {
        log_errno("socket");
        return -1;
    }

    // Set SO_REUSEADDR option to allow immediate reuse of the address
    if(setsockopt(serverfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) ==
       -1) {
        log_errno("setsockopt");
        close(serverfd);
        return -1;
    }

    address.sin_family      = AF_INET;
    address.sin_port        = htons(port);
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    memset(&(address.sin_zero), 0, 8);

    res =
        bind(serverfd, (struct sockaddr*)&address, sizeof(struct sockaddr_in));
    if(res == -1) {
        log_errno("bind");
        close(serverfd);
        return -1;
    }

    res = listen(serverfd, backlog);
    if(res == -1) {
        log_errno("listen");
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
        log_errno("accept");
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
    char * buffer      = malloc(total_size), *end;
    if(!buffer) return NULL;

    ssize_t bytes_read;
    while(1) {
        if(bytes_total >= total_size - 1) {
            // Grow buffer (double strategy)
            size_t new_size = total_size * 2;
            if(new_size > MAX_RECIEVE_SIZE) {
                log_warn("Request too large");
                free(buffer);
                return NULL;
            }
            char* new_buffer = realloc(buffer, new_size);
            if(!new_buffer) {
                log_errno("realloc");
                free(buffer);
                return NULL;
            }
            buffer     = new_buffer;
            total_size = new_size;
        }

        bytes_read =
            read(clientfd, buffer + bytes_total, total_size - bytes_total - 1);
        if(bytes_read <= 0) {
            // Client disconnected or error occurred
            free(buffer);
            return NULL;
        }

        bytes_total         += bytes_read;
        buffer[bytes_total]  = '\0';  // Null-terminate

        // Check for end of message if protocol defines it (e.g., newline)
        end = strchr(buffer, '\n');
        if(end) break;
    }

    if(end) {
        *end = '\0';
    }
    // printf("Received: %s\n", buffer);
    return buffer;
}

/**
 * Sends data to a client socket
 * @param clientfd The client socket file descriptor
 * @param data The data to send
 * @param len The length of data to send
 * @return Number of bytes sent on success, -1 on error
 */
ssize_t send_data(int clientfd, const char* data, size_t len) {
    size_t  bytes_sent = 0;
    ssize_t result;

    while(bytes_sent < len) {
        result = write(clientfd, data + bytes_sent, len - bytes_sent);

        if(result <= 0) {
            log_errno("write");
            return -1;
        }

        bytes_sent += result;
    }

    return bytes_sent;
}

/**
 * Sends an integer to a client socket as a string followed by newline
 * @param clientfd The client socket file descriptor
 * @param value The integer value to send
 * @return Number of bytes sent on success, -1 on error
 */
ssize_t send_int(int clientfd, int value) {
    char buffer[32];
    int  len = snprintf(buffer, sizeof(buffer), "%d\n", value);

    if(len < 0 || (unsigned long)len >= sizeof(buffer)) {
        return -1;
    }

    return send_data(clientfd, buffer, len);
}
