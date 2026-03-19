#include "main.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config/runtime_config.h"
#include "connection_manager.h"
#include "db/data_model.h"
#include "db/operations.h"
#include "db/parser.h"
#include "db/persistence.h"
#include "net.h"
#include "utils/autosave.h"
#include "utils/stdin.h"

static char g_snapshotPath[RUNTIME_CONFIG_PATH_MAX] = {0};

/**
 * Cleanup handler registered with atexit()
 */
static void cleanup_handler(void) {
    const char* snapshotPath = (g_snapshotPath[0] != '\0') ? g_snapshotPath : NULL;

    if(persistence_save_all(snapshotPath) != 0) {
        fprintf(stderr, "Failed to save snapshot on shutdown\n");
    }
    free_db_storage();
}

int main(int argc, char* argv[]) {
    int               serverfd, eventfd, res, i, maxEvents;
    AutosaveState     autosaveState;
    bool              keepRunning;
    ConnectionManager cm;
    RuntimeConfig     config;
    char*             data;
    Command*          command;
    EpollEvent*       events;

    res = runtime_config_init(&config, argc, argv);
    if(res == 1) {
        return EXIT_SUCCESS;
    }
    if(res != 0) {
        return EXIT_FAILURE;
    }

    strncpy(g_snapshotPath, config.snapshotPath, sizeof(g_snapshotPath));
    g_snapshotPath[sizeof(g_snapshotPath) - 1] = '\0';

    maxEvents = config.maxConnections + 2;
    events    = (EpollEvent*)calloc((size_t)maxEvents, sizeof(EpollEvent));
    if(events == NULL) {
        return EXIT_FAILURE;
    }

    puts("Starting to make the server");
    init_db_storage();
    if(persistence_load_all(config.snapshotPath) != 0) {
        fprintf(stderr, "Failed to load snapshot, starting with empty state\n");
    }

    autosaveState.enabled = 0;
    if(config.autosaveEnabled) {
        if(autosave_init(&autosaveState, config.autosaveIntervalMs) != 0) {
            fprintf(stderr,
                    "Failed to initialize monotonic autosave timer; autosave disabled\n");
        }
    }

    atexit(cleanup_handler);

    res = create_server(config.port, config.maxConnections);
    if(res == -1) {
        free(events);
        exit(EXIT_FAILURE);
    }
    serverfd = res;
    printf("running on port %d\n", config.port);
    puts("Type Q at anytime to exit");

    if(init_connection_manager(&cm, config.maxConnections) == -1) {
        free(events);
        close(serverfd);
        exit(EXIT_FAILURE);
    }

    if(watch_fd(&cm, serverfd, EPOLLIN) == -1) {
        free(events);
        close(serverfd);
        destroy_connection_manager(&cm);
        exit(EXIT_FAILURE);
    }

    if(watch_fd(&cm, STDIN_FILENO, EPOLLIN) == -1) {
        free(events);
        close(serverfd);
        destroy_connection_manager(&cm);
        exit(EXIT_FAILURE);
    }

    keepRunning = true;
    while(keepRunning) {
        res = wait_for_events(&cm, events, maxEvents, 500);
        if(res == -1) {
            free(events);
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

        autosave_maybe_run(&autosaveState, config.snapshotPath);
    }

    puts("done");
    free(events);
    destroy_connection_manager(&cm);
    close(serverfd);
    return 0;
}
