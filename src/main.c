#include "main.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include "db/data_model.h"
#include "db/operations.h"
#include "db/parser.h"
#include "epoll_manager.h"
#include "net.h"
#include "utils/stdin.h"

/**
 * Cleanup handler registered with atexit()
 */
static void cleanup_handler(void) { free_db_storage(); }

int main(void) {
    int  serverfd, epollfd, res, i;
    bool keepRunning;
    // TODO: must make it an array or smth cuz we want to accept multiple.
    int        clientfd = -1;
    char*      data;
    Command*   command;
    EpollEvent events[10];

    puts("Starting to make the server");
    init_db_storage();
    atexit(cleanup_handler);

    res = create_server(DEFAULT_PORT);
    if(res == -1) {
        exit(EXIT_FAILURE);
    }
    serverfd = res;
    printf("running on port %d\n", DEFAULT_PORT);
    puts("Type Q at anytime to exit");

    epollfd = init_epoll();
    if(epollfd == -1) {
        close(serverfd);
        exit(EXIT_FAILURE);
    }

    res = add_to_epoll(epollfd, serverfd, EPOLLIN);
    if(res == -1) {
        close(serverfd);
        close(epollfd);
        exit(EXIT_FAILURE);
    }

    res = add_to_epoll(epollfd, STDIN_FILENO, EPOLLIN);
    if(res == -1) {
        close(serverfd);
        close(epollfd);
        exit(EXIT_FAILURE);
    }

    keepRunning = true;
    while(keepRunning) {
        res = wait_for_events(epollfd, events, 10, 500);
        if(res == -1) {
            close(serverfd);
            close(epollfd);
            exit(EXIT_FAILURE);
        }

        for(i = 0; i < res; i++) {
            if(events[i].data.fd == STDIN_FILENO) {
                res = handle_stdin_input();
                if(res == 1) {
                    keepRunning = false;
                }
            } else if(events[i].data.fd == serverfd) {
                clientfd = accept_connection(serverfd);
                add_to_epoll(epollfd, clientfd, EPOLLIN);
            } else if(events[i].data.fd == clientfd) {
                data = receive_data(clientfd);
                if(data == NULL) {
                    remove_from_epoll(epollfd, clientfd);
                    close(clientfd);
                    clientfd = -1;
                    continue;
                }
                command = parse_command(data);
                if(command != NULL) {
                    if(command->op == OP_ERROR) {
                        send_data(clientfd,
                                  command->data.error,
                                  MAX_ERROR_LENGTH);
                        free_command(command, 0);
                    } else {
                        CommandResult* result = execute_command(command);

                        if(result != NULL) {
                            if(result->message != NULL) {
                                // Send error message
                                send_data(clientfd,
                                          result->message,
                                          strlen(result->message));
                            } else if(result->data != NULL) {
                                // Send result data (e.g., table from GET)
                                send_data(clientfd,
                                          result->data,
                                          strlen(result->data));
                            } else {
                                // Send success code
                                send_int(clientfd, result->code);
                            }
                            free_command_result(result);
                        }
                        // Strings transferred to DB for ADD/UP operations
                        int transferred =
                            (command->op == OP_ADD || command->op == OP_UP);
                        free_command(command, transferred);
                    }
                }

                free(data);
            } else {
                puts("Unknown event occured skipping");
            }
        }
    }

    puts("done");
    if(clientfd != -1) {
        close(clientfd);
    }
    close(epollfd);
    close(serverfd);
    return 0;
}
