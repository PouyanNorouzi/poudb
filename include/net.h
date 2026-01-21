#ifndef NET_H
#define NET_H

#include <stdlib.h>

#define MAX_CONNECTIONS 5

int create_server(int port);

int accept_connection(int serverfd);

char* receive_data(int clientfd);

ssize_t send_data(int clientfd, const char* data, size_t len);

ssize_t send_int(int clientfd, int value);

#endif  // NET_H
