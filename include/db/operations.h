#ifndef OPERATIONS_H
#define OPERATIONS_H

#include "parser.h"

/**
 * Enumeration of execution error codes
 */
typedef enum {
    EX_SUCCESS,                 /* Operation completed successfully */
    EX_INVALID_DATA,            /* Invalid data provided to operation */
    EX_DB_NOT_FOUND,            /* Database not found */
    EX_DB_ALREADY_EXISTS,       /* Database already exists */
    EX_MEMORY_ALLOCATION_FAILED, /* Memory allocation failed */
    EX_DB_CREATE_FAILED,        /* Database creation failed */
    EX_ROW_NOT_FOUND,           /* Row not found */
    EX_INVALID_FIELD,           /* Invalid field specified */
    EX_TYPE_MISMATCH,           /* Data type mismatch */
    EX_UNKNOWN_ERROR            /* Unknown error */
} ExecutionError;

/**
 * Result structure for operation execution
 */
typedef struct {
    int         code;    /* Return code (positive for success, negative for error) */
    const char* message; /* Error message (NULL on success) */
} CommandResult;

/**
 * Execute a parsed command
 *
 * @param cmd Pointer to the command to execute
 * @return CommandResult pointer with code and error message (caller must free)
 */
CommandResult* execute_command(Command* cmd);

#endif //OPERATIONS_H
