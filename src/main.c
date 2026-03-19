#include "main.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "connection_manager.h"
#include "db/data_model.h"
#include "db/operations.h"
#include "db/parser.h"
#include "db/persistence.h"
#include "net.h"
#include "utils/autosave.h"
#include "utils/stdin.h"

#define AUTOSAVE_INTERVAL_MS 30000LL

/**
 * Cleanup handler registered with atexit()
 */
static void cleanup_handler(void) {
    if(persistence_save_all(NULL) != 0) {
        fprintf(stderr, "Failed to save snapshot on shutdown\n");
    }
    free_db_storage();
}

int main(void) {
    int               serverfd, eventfd, res, i;
    AutosaveState     autosaveState;
    bool              keepRunning;
    ConnectionManager cm;
    char*             data;
    Command*          command;
    EpollEvent        events[MAX_CONNECTIONS + 2];

    puts("Starting to make the server");
    init_db_storage();
    if(persistence_load_all(NULL) != 0) {
        fprintf(stderr, "Failed to load snapshot, starting with empty state\n");
    }

    if(autosave_init(&autosaveState, AUTOSAVE_INTERVAL_MS) != 0) {
        fprintf(stderr,
                "Failed to initialize monotonic autosave timer; autosave disabled\n");
    }

    atexit(cleanup_handler);

    res = create_server(DEFAULT_PORT);
    if(res == -1) {
        exit(EXIT_FAILURE);
    }
    serverfd = res;
    printf("running on port %d\n", DEFAULT_PORT);
    puts("Type Q at anytime to exit");

    if(init_connection_manager(&cm) == -1) {
        close(serverfd);
        exit(EXIT_FAILURE);
    }

    if(watch_fd(&cm, serverfd, EPOLLIN) == -1) {
        close(serverfd);
        destroy_connection_manager(&cm);
        exit(EXIT_FAILURE);
    }

    if(watch_fd(&cm, STDIN_FILENO, EPOLLIN) == -1) {
        close(serverfd);
        destroy_connection_manager(&cm);
        exit(EXIT_FAILURE);
    }

    keepRunning = true;
    while(keepRunning) {
        res = wait_for_events(&cm, events, MAX_CONNECTIONS + 2, 500);
        if(res == -1) {
            close(serverfd);
            destroy_connection_manager(&cm);
            exit(EXIT_FAILURE);
        }

        for(i = 0; i < res; i++) {
            eventfd = events[i].data.fd;

            if(eventfd == STDIN_FILENO) {
                res = handle_stdin_input();
                if(res == 1) {
                    keepRunning = false;
                }
            } else if(eventfd == serverfd) {
                int clientfd = accept_connection(serverfd);
                if(clientfd != -1) {
                    if(add_client(&cm, clientfd) == -1) {
                        /* At capacity — reject the connection */
                        close(clientfd);
                    }
                }
            } else if(is_client(&cm, eventfd)) {
                data = receive_data(eventfd);
                if(data == NULL) {
                    remove_client(&cm, eventfd);
                    continue;
                }
                command = parse_command(data);
                if(command != NULL) {
                    if(command->op == OP_ERROR) {
                        send_data(eventfd,
                                  command->data.error,
                                  MAX_ERROR_LENGTH);
                        free_command(command, 0);
                    } else {
                        CommandResult* result = execute_command(command);

                        if(result != NULL) {
                            if(result->message != NULL) {
                                send_data(eventfd,
                                          result->message,
                                          strlen(result->message));
                            } else if(result->data != NULL) {
                                send_data(eventfd,
                                          result->data,
                                          strlen(result->data));
                            } else {
                                send_int(eventfd, result->code);
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

        autosave_maybe_run(&autosaveState);
    }

    puts("done");
    destroy_connection_manager(&cm);
    close(serverfd);
    return 0;
}
