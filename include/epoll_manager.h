#ifndef EPOLL_MANAGER_H
#define EPOLL_MANAGER_H

#include <sys/epoll.h>

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
 * Waits for events on the epoll instance.
 * Returns the number of file descriptors ready for the requested I/O, or -1 on error.
 */
int wait_for_events(int epollfd, struct epoll_event *events, int maxevents, int timeout);

#endif /* EPOLL_MANAGER_H */
