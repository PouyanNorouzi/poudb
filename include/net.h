#ifndef NET_H
#define NET_H

#define MAX_CONNECTIONS 5

int create_server(int port);

int accept_connection(int serverfd);

char* receive_data(int clientfd);

#endif  // NET_H
