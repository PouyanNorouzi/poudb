#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define DEFAULT_PORT 3005
#define BUFFER_SIZE 1024

int main(void) {
    int clientfd;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;

    // Create socket
    clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if (clientfd == -1) {
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }

    // Setup server address structure
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(DEFAULT_PORT);

    // Convert IPv4 address from text to binary
    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0) {
        perror("Invalid address");
        close(clientfd);
        exit(EXIT_FAILURE);
    }

    // Connect to the server
    if (connect(clientfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        close(clientfd);
        exit(EXIT_FAILURE);
    }

    printf("Connected to server. Type your message (Ctrl+C to quit):\n");

    // Main loop for user input
    while (1) {
        printf("> ");

        // Get user input
        if (fgets(buffer, BUFFER_SIZE, stdin) == NULL) {
            break; // Exit on EOF
        }

        // Remove trailing newline
        buffer[strcspn(buffer, "\n")] = 0;

        // Send the message to server
        send(clientfd, buffer, strlen(buffer), 0);

        // Clear buffer for receiving response
        memset(buffer, 0, BUFFER_SIZE);

        // Receive server's response
        bytes_read = recv(clientfd, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0'; // Null-terminate
            printf("Server response: %s\n", buffer);
        } else if (bytes_read == 0) {
            printf("Server closed the connection\n");
            break;
        } else {
            perror("recv failed");
            break;
        }
    }

    close(clientfd);
    return 0;
}
