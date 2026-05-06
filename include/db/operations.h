#ifndef OPERATIONS_H
#define OPERATIONS_H

#include "auth.h"
#include "parser.h"

/**
 * Enumeration of execution error codes
 */
typedef enum {
    EX_SUCCESS,                  /* Operation completed successfully */
    EX_INVALID_DATA,             /* Invalid data provided to operation */
    EX_DB_NOT_FOUND,             /* Database not found */
    EX_DB_ALREADY_EXISTS,        /* Database already exists */
    EX_MEMORY_ALLOCATION_FAILED, /* Memory allocation failed */
    EX_DB_CREATE_FAILED,         /* Database creation failed */
    EX_ROW_NOT_FOUND,            /* Row not found */
    EX_INVALID_FIELD,            /* Invalid field specified */
    EX_TYPE_MISMATCH,            /* Data type mismatch */
    EX_INDEX_ALREADY_EXISTS,     /* Index already exists */
    EX_LIMIT_EXCEEDED,           /* A configured limit was exceeded */
    EX_UNKNOWN_ERROR             /* Unknown error */
} ExecutionError;

/**
 * Result structure for operation execution
 */
typedef struct {
    int code; /* Return code (positive for success, negative for error) */
    const char* message; /* Error message (NULL on success) */
    char*       data;    /* Result data string (caller must free if not NULL) */
} CommandResult;

/**
 * Execute a parsed command
 *
 * @param cmd Pointer to the command to execute
 * @return CommandResult pointer with code and error message (caller must free)
 */
CommandResult* execute_command(Command* cmd);

/**
 * Execute an ADD_KEY command: generate a token, store its hash, return token.
 * The caller receives the raw token (shown once) in result->data.
 */
CommandResult* execute_add_key(const AddKeyData* data);

/**
 * Execute a DEL_KEY command: remove a key from the auth store.
 */
CommandResult* execute_del_key(const DelKeyData* data);

/**
 * Execute a LIST_KEYS command: return a name/role table.
 */
CommandResult* execute_list_keys(void);

/**
 * Execute a WHOAMI command: return the authenticated key name.
 */
CommandResult* execute_whoami(const WhoamiData* data);

/**
 * Free a CommandResult and its data
 *
 * @param result Pointer to the CommandResult to free
 */
void free_command_result(CommandResult* result);

#endif  // OPERATIONS_H
