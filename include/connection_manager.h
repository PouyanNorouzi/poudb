#ifndef CONNECTION_MANAGER_H
#define CONNECTION_MANAGER_H

#include <stdint.h>
#include <sys/epoll.h>

#include "net.h"

typedef struct epoll_event EpollEvent;

typedef struct {
    int epollfd;
    int clients[MAX_CONNECTIONS];
    int client_count;
} ConnectionManager;

/**
 * Creates and initializes a ConnectionManager: opens an epoll instance and
 * zeroes the client array.
 * Returns 0 on success, -1 on error.
 */
int init_connection_manager(ConnectionManager* cm);

/**
 * Adds an arbitrary fd to epoll without tracking it as a client.
 * Use this for serverfd and stdin.
 * Returns 0 on success, -1 on error.
 */
int watch_fd(ConnectionManager* cm, int fd, uint32_t events);

/**
 * Accepts a new client: records clientfd in the client array and registers it
 * with epoll.
 * Returns 0 on success, -1 if at capacity or on epoll error.
 */
int add_client(ConnectionManager* cm, int clientfd);

/**
 * Removes a client: deregisters from epoll, closes the fd, and frees its slot.
 * Returns 0 on success, -1 if fd was not found.
 */
int remove_client(ConnectionManager* cm, int clientfd);

/**
 * Returns 1 if fd is a tracked client, 0 otherwise.
 */
int is_client(const ConnectionManager* cm, int fd);

/**
 * Waits for events using the internal epollfd.
 * Returns the number of ready fds, or -1 on error.
 */
int wait_for_events(ConnectionManager* cm,
                    EpollEvent*        events,
                    int                maxevents,
                    int                timeout);

/**
 * Closes all tracked client connections and resets the client array.
 */
void close_all_clients(ConnectionManager* cm);

/**
 * Closes all clients and the epoll fd.
 */
void destroy_connection_manager(ConnectionManager* cm);

#endif /* CONNECTION_MANAGER_H */
