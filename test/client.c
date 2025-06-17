#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define DEFAULT_PORT 3005
#define SERVER_IP "127.0.0.1"
#define BUFFER_SIZE 1024

/**
 * Simple client that connects to the server and receives a message.
 */
int main(void) {
    int clientfd;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];

    // Create socket
    clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if (clientfd == -1) {
        perror("Error creating socket");
        return -1;
    }
    printf("Socket created successfully\n");

    // Prepare server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(DEFAULT_PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    // Convert IP address from text to binary
    // if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
    //     perror("Invalid address");
    //     close(clientfd);
    //     return -1;
    // }

    // Connect to server
    if (connect(clientfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("Connection failed");
        close(clientfd);
        return -1;
    }
    printf("Connected to server at %s:%d\n", SERVER_IP, DEFAULT_PORT);

    // Receive message from server
    // memset(buffer, 0, BUFFER_SIZE);
    // ssize_t bytes_received = read(clientfd, buffer, BUFFER_SIZE - 1);

    // if (bytes_received > 0) {
    //     printf("Message received from server: %s\n", buffer);
    // } else if (bytes_received == 0) {
    //     printf("Server closed the connection\n");
    // } else {
    //     perror("Error receiving data");
    // }

    // Close connection
    close(clientfd);
    printf("Connection closed\n");

    return 0;
}
