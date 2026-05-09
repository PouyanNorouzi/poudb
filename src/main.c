#include "main.h"

#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/signalfd.h>
#include <unistd.h>

#include "auth.h"
#include "config/runtime_config.h"
#include "connection_manager.h"
#include "db/data_model.h"
#include "db/operations.h"
#include "db/parser.h"
#include "db/persistence.h"
#include "net.h"
#include "utils/autosave.h"
#include "utils/log.h"

static char g_snapshotPath[RUNTIME_CONFIG_PATH_MAX] = {0};

/**
 * Cleanup handler registered with atexit()
 */
static void cleanup_handler(void) {
    const char* snapshotPath = (g_snapshotPath[0] != '\0') ? g_snapshotPath : NULL;

    if(persistence_save_all(snapshotPath) != 0) {
        log_error("Failed to save snapshot on shutdown");
    }
    free_db_storage();
}

int main(int argc, char* argv[]) {
    int               serverfd, eventfd, res, i, maxEvents, sigfd;
    sigset_t          sigmask;
    AutosaveState     autosaveState;
    bool              keepRunning;
    ConnectionManager cm;
    RuntimeConfig     config;
    char*             data;
    Command*          command;
    EpollEvent*       events;

    if(sodium_init() < 0) {
        log_error("Failed to initialize libsodium");
        return EXIT_FAILURE;
    }

    res = runtime_config_init(&config, argc, argv);
    if(res == 1) {
        return EXIT_SUCCESS;
    }
    if(res != 0) {
        return EXIT_FAILURE;
    }

    log_set_level(config.logLevel);
    log_info("Log level: %s", log_level_to_string(config.logLevel));

    strncpy(g_snapshotPath, config.snapshotPath, sizeof(g_snapshotPath));
    g_snapshotPath[sizeof(g_snapshotPath) - 1] = '\0';

    maxEvents = config.maxConnections + 2;
    events    = (EpollEvent*)calloc((size_t)maxEvents, sizeof(EpollEvent));
    if(events == NULL) {
        return EXIT_FAILURE;
    }

    puts("Starting to make the server");
    auth_store_init(&g_auth_store);
    init_db_storage();
    if(persistence_load_all(config.snapshotPath) != 0) {
        log_warn("Failed to load snapshot, starting with empty state");
    }

    /* Bootstrap: generate an admin key when no keys exist yet. */
    if(g_auth_store.count == 0) {
        char token[AUTH_TOKEN_BUF_SIZE];
        if(auth_generate_token(token) == 0 &&
           auth_add_key(&g_auth_store, "admin", token, ROLE_ADMIN) == 0) {
            log_info("[AUTH] First-run admin key: %s", token);
            log_info("[AUTH] Store it securely - it will not be shown again.");
            /* Persist immediately so the hash survives a restart. */
            if(persistence_save_all(config.snapshotPath) != 0) {
                log_warn("Failed to persist initial admin key");
            }
        } else {
            log_error("Failed to generate initial admin key");
        }
        sodium_memzero(token, sizeof(token));
    }

    autosaveState.enabled = 0;
    if(config.autosaveEnabled) {
        if(autosave_init(&autosaveState, config.autosaveIntervalMs) != 0) {
            log_warn("Failed to initialize monotonic autosave timer; autosave disabled");
        }
    }

    sigemptyset(&sigmask);
    sigaddset(&sigmask, SIGTERM);
    sigaddset(&sigmask, SIGINT);
    if(sigprocmask(SIG_BLOCK, &sigmask, NULL) == -1) {
        log_errno("sigprocmask");
        free(events);
        exit(EXIT_FAILURE);
    }
    sigfd = signalfd(-1, &sigmask, SFD_CLOEXEC);
    if(sigfd == -1) {
        log_errno("signalfd");
        free(events);
        exit(EXIT_FAILURE);
    }

    atexit(cleanup_handler);

    res = create_server(config.port, config.maxConnections);
    if(res == -1) {
        free(events);
        close(sigfd);
        exit(EXIT_FAILURE);
    }
    serverfd = res;
    log_info("Running on port %d", config.port);

    if(init_connection_manager(&cm, config.maxConnections) == -1) {
        free(events);
        close(serverfd);
        close(sigfd);
        exit(EXIT_FAILURE);
    }

    if(watch_fd(&cm, serverfd, EPOLLIN) == -1) {
        free(events);
        close(serverfd);
        close(sigfd);
        destroy_connection_manager(&cm);
        exit(EXIT_FAILURE);
    }

    if(watch_fd(&cm, sigfd, EPOLLIN) == -1) {
        free(events);
        close(serverfd);
        close(sigfd);
        destroy_connection_manager(&cm);
        exit(EXIT_FAILURE);
    }

    keepRunning = true;
    while(keepRunning) {
        res = wait_for_events(&cm, events, maxEvents, 500);
        if(res == -1) {
            free(events);
            close(serverfd);
            close(sigfd);
            destroy_connection_manager(&cm);
            exit(EXIT_FAILURE);
        }

        for(i = 0; i < res; i++) {
            eventfd = events[i].data.fd;

            if(eventfd == sigfd) {
                struct signalfd_siginfo siginfo;
                ssize_t nbytes = read(sigfd, &siginfo, sizeof(siginfo));
                if(nbytes == (ssize_t)sizeof(siginfo)) {
                    log_info("Received signal %u, shutting down",
                             siginfo.ssi_signo);
                }
                keepRunning = false;
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
                    } else if(command->op == OP_AUTH) {
                        /* Always permitted: authenticate this connection. */
                        char      auth_name[AUTH_KEY_NAME_MAX];
                        AuthLevel level =
                            auth_verify_ex(&g_auth_store,
                                           command->data.auth.token,
                                           auth_name);
                        if(level == AUTH_NONE) {
                            const char* msg = "ERR Invalid token";
                            send_data(eventfd, msg, strlen(msg));
                        } else {
                            set_client_auth(&cm, eventfd, level);
                            set_client_name(&cm, eventfd, auth_name);
                            send_int(eventfd, (int)level);
                        }
                        free_command(command, 0);
                    } else {
                        /* All other commands require authentication. */
                        AuthLevel auth = get_client_auth(&cm, eventfd);
                        if(auth == AUTH_NONE) {
                            const char* msg = "ERR Authentication required";
                            send_data(eventfd, msg, strlen(msg));
                            free_command(command, 0);
                        } else if(auth == AUTH_READONLY &&
                                  command->op != OP_GET &&
                                  command->op != OP_GET_ALL &&
                                  command->op != OP_SEARCH &&
                                  command->op != OP_COUNT &&
                                  command->op != OP_WHOAMI) {
                            const char* msg = "ERR Permission denied";
                            send_data(eventfd, msg, strlen(msg));
                            free_command(command, 0);
                        } else {
                            if(command->op == OP_WHOAMI) {
                                get_client_name(&cm, eventfd,
                                                command->data.whoami.name);
                            }
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
                                (command->op == OP_ADD ||
                                 command->op == OP_UP);
                            free_command(command, transferred);
                        }
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
    close(sigfd);
    return 0;
}
