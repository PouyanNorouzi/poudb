#include "main.h"

#include <stdio.h>
#include <unistd.h>

#include "net.h"

int main(void) {
    int serverfd;
    int res;
    // TODO: must make it an array or smth cuz we are going to be accepting multiple connections.
    int clientfd;

    puts("Starting to make the server");

    res = create_server(DEFAULT_PORT);
    if(res == -1) {
        return -1;
    }
    serverfd = res;
    printf("running on port %d\n", DEFAULT_PORT);

    clientfd = accept_connection(serverfd);

    close(clientfd);
    close(serverfd);
    return 0;
}
