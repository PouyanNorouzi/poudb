#include "main.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include "db/parser.h"
#include "epoll_manager.h"
#include "net.h"
#include "utils.h"

int main(void) {
    int  serverfd;
    int  epollfd;
    int  res;
    int  i;
    bool keepRunning;
    // TODO: must make it an array or smth cuz we are going to be accepting
    // multiple connections.
    int                clientfd;
    char*              data;
    struct epoll_event events[10];

    puts("Starting to make the server");

    res = create_server(DEFAULT_PORT);
    if(res == -1) {
        exit(EXIT_FAILURE);
    }
    serverfd = res;
    printf("running on port %d\n", DEFAULT_PORT);
    puts("Type Q at anytime to exit");

    epollfd = init_epoll();
    if(epollfd == -1) {
        close(serverfd);
        exit(EXIT_FAILURE);
    }

    res = add_to_epoll(epollfd, serverfd, EPOLLIN);
    if(res == -1) {
        close(serverfd);
        close(epollfd);
        exit(EXIT_FAILURE);
    }

    res = add_to_epoll(epollfd, STDIN_FILENO, EPOLLIN);
    if(res == -1) {
        close(serverfd);
        close(epollfd);
        exit(EXIT_FAILURE);
    }

    keepRunning = true;
    while(keepRunning) {
        res = wait_for_events(epollfd, events, 10, 500);
        if(res == -1) {
            close(serverfd);
            close(epollfd);
            exit(EXIT_FAILURE);
        }

        for(i = 0; i < res; i++) {
            if(events[i].data.fd == STDIN_FILENO) {
                res = handle_stdin_input();
                if(res == 1) {
                    keepRunning = false;
                }
            } else if(events[i].data.fd == serverfd) {
                clientfd = accept_connection(serverfd);
                add_to_epoll(epollfd, clientfd, EPOLLIN);
            } else if(events[i].data.fd == clientfd) {
                data = recieve_data(clientfd);
                if(data == NULL) {
                    remove_from_epoll(epollfd, clientfd);
                    close(clientfd);
                    clientfd = -1;
                }
                parse_command(data);
                free(data);
            } else {
                puts("Unknown event occured skipping");
            }
        }
    }

    puts("done");
    if(clientfd != -1) {
        close(clientfd);
    }
    close(epollfd);
    close(serverfd);
    return 0;
}
