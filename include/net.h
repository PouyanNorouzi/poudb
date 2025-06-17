#ifndef NET_H
#define NET_H

#define MAX_CONNECTIONS 5
#define RECIEVE_BUFFER_SIZE 1024

int create_server(int port);

int accept_connection(int serverfd);

int recieve_data(int clientfd);

#endif  // NET_H
