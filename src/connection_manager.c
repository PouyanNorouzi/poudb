#include "connection_manager.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int init_connection_manager(ConnectionManager* cm, int maxClients) {
    if(cm == NULL || maxClients <= 0) {
        return -1;
    }

    cm->epollfd      = epoll_create1(0);
    cm->client_count = 0;
    cm->max_clients  = maxClients;
    cm->clients      = (int*)malloc((size_t)maxClients * sizeof(int));

    if(cm->epollfd == -1) {
        perror("epoll_create1");
        return -1;
    }

    if(cm->clients == NULL) {
        close(cm->epollfd);
        cm->epollfd = -1;
        return -1;
    }

    memset(cm->clients, -1, (size_t)maxClients * sizeof(int));
    return 0;
}

int watch_fd(ConnectionManager* cm, int fd, uint32_t events) {
    struct epoll_event event;
    event.data.fd = fd;
    event.events  = events;

    int res = epoll_ctl(cm->epollfd, EPOLL_CTL_ADD, fd, &event);
    if(res == -1) {
        perror("epoll_ctl: watch_fd");
    }
    return res;
}

int add_client(ConnectionManager* cm, int clientfd) {
    if(cm->client_count >= cm->max_clients) {
        fprintf(stderr, "add_client: at capacity (%d)\n", cm->max_clients);
        return -1;
    }

    struct epoll_event event;
    event.data.fd = clientfd;
    event.events  = EPOLLIN;

    if(epoll_ctl(cm->epollfd, EPOLL_CTL_ADD, clientfd, &event) == -1) {
        perror("epoll_ctl: add_client");
        return -1;
    }

    cm->clients[cm->client_count++] = clientfd;
    return 0;
}

int remove_client(ConnectionManager* cm, int clientfd) {
    struct epoll_event event = {0};
    epoll_ctl(cm->epollfd, EPOLL_CTL_DEL, clientfd, &event);
    close(clientfd);

    for(int i = 0; i < cm->client_count; i++) {
        if(cm->clients[i] == clientfd) {
            /* Shift remaining entries left to fill the gap */
            for(int j = i; j < cm->client_count - 1; j++) {
                cm->clients[j] = cm->clients[j + 1];
            }
            cm->clients[--cm->client_count] = -1;
            return 0;
        }
    }

    fprintf(stderr, "remove_client: fd %d not found\n", clientfd);
    return -1;
}

int is_client(const ConnectionManager* cm, int fd) {
    for(int i = 0; i < cm->client_count; i++) {
        if(cm->clients[i] == fd) {
            return 1;
        }
    }
    return 0;
}

int wait_for_events(ConnectionManager* cm,
                    EpollEvent*        events,
                    int                maxevents,
                    int                timeout) {
    int nfds = epoll_wait(cm->epollfd, events, maxevents, timeout);
    if(nfds == -1) {
        perror("epoll_wait");
    }
    return nfds;
}

void close_all_clients(ConnectionManager* cm) {
    for(int i = 0; i < cm->client_count; i++) {
        struct epoll_event event = {0};
        epoll_ctl(cm->epollfd, EPOLL_CTL_DEL, cm->clients[i], &event);
        close(cm->clients[i]);
        cm->clients[i] = -1;
    }
    cm->client_count = 0;
}

void destroy_connection_manager(ConnectionManager* cm) {
    if(cm == NULL) {
        return;
    }

    close_all_clients(cm);
    if(cm->epollfd != -1) {
        close(cm->epollfd);
        cm->epollfd = -1;
    }

    free(cm->clients);
    cm->clients = NULL;
    cm->max_clients = 0;
}
