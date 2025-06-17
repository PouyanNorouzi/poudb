#include "epoll_manager.h"

#include <stdio.h>
#include <sys/epoll.h>
#include <unistd.h>

/**
 * Creates and initializes an epoll instance.
 * Returns the file descriptor for the epoll instance, or -1 on error.
 */
int init_epoll(void) {
    int epollfd = epoll_create1(0);
    if(epollfd == -1) {
        perror("epoll_create1");
    }
    return epollfd;
}

/**
 * Adds a file descriptor to an epoll instance with specified events.
 * Returns 0 on success, -1 on error.
 */
int add_to_epoll(int epollfd, int fd, uint32_t events) {
    struct epoll_event event;

    event.data.fd = fd;
    event.events  = events;

    int res = epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    if(res == -1) {
        perror("epoll_ctl: add");
    }

    return res;
}

/**
 * Removes a file descriptor from an epoll instance.
 * Returns 0 on success, -1 on error.
 */
int remove_from_epoll(int epollfd, int fd) {
    // For EPOLL_CTL_DEL, the event parameter is ignored but we still need to
    // provide it Some older versions of Linux required a non-NULL event pointer
    struct epoll_event event = {0};

    int res = epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, &event);
    if(res == -1) {
        perror("epoll_ctl: del");
    }

    return res;
}

/**
 * Waits for events on the epoll instance.
 * Returns the number of file descriptors ready for the requested I/O, or -1 on
 * error.
 */
int wait_for_events(int                 epollfd,
                    struct epoll_event* events,
                    int                 maxevents,
                    int                 timeout) {
    int nfds = epoll_wait(epollfd, events, maxevents, timeout);
    if(nfds == -1) {
        perror("epoll_wait");
    }

    return nfds;
}
