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
    char               testData[]     = "frick";
    int                testDataLength = 6;

    len      = sizeof(struct sockaddr_in);
    clientfd = accept(serverfd, (struct sockaddr*)&clientAddress, &len);
    if(clientfd == -1) {
        perror("accept");
    }
    puts("A client connected");

    write(clientfd, &testData, testDataLength);

    return clientfd;
}
