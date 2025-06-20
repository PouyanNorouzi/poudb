#ifndef EPOLL_MANAGER_H
#define EPOLL_MANAGER_H

#include <sys/epoll.h>

typedef struct epoll_event EpollEvent ;

/**
 * Creates and initializes an epoll instance.
 * Returns the file descriptor for the epoll instance, or -1 on error.
 */
int init_epoll(void);

/**
 * Adds a file descriptor to an epoll instance with specified events.
 * Returns 0 on success, -1 on error.
 */
int add_to_epoll(int epollfd, int fd, uint32_t events);

/**
 * Removes a file descriptor from an epoll instance.
 * Returns 0 on success, -1 on error.
 */
int remove_from_epoll(int epollfd, int fd);

/**
 * Waits for events on the epoll instance.
 * Returns the number of file descriptors ready for the requested I/O, or -1 on error.
 */
int wait_for_events(int epollfd, struct epoll_event *events, int maxevents, int timeout);

#endif /* EPOLL_MANAGER_H */
