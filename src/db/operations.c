#include "db/operations.h"

#include <stdio.h>
#include <stdlib.h>

#include "db/data_model.h"

/**
 * Execution error messages corresponding to ExecutionError enum
 */
static const char* EXECUTION_ERROR_MESSAGES[] = {
    NULL,                                 /* EX_SUCCESS */
    "Invalid data provided to operation", /* EX_INVALID_DATA */
    "Database not found",                 /* EX_DB_NOT_FOUND */
    "Database already exists",            /* EX_DB_ALREADY_EXISTS */
    "Memory allocation failed",           /* EX_MEMORY_ALLOCATION_FAILED */
    "Failed to create database",          /* EX_DB_CREATE_FAILED */
    "Row not found",                      /* EX_ROW_NOT_FOUND */
    "Invalid field specified",            /* EX_INVALID_FIELD */
    "Data type mismatch",                 /* EX_TYPE_MISMATCH */
    "Unknown error occurred"              /* EX_UNKNOWN_ERROR */
};

/**
 * Function prototypes for private handler functions
 */
static CommandResult* execute_create(CreateData* data);
static CommandResult* execute_add(AddData* data);
static CommandResult* execute_up(UpdateData* data);
static CommandResult* execute_get(GetData* data);
static CommandResult* execute_del(DeleteData* data);
static CommandResult* execute_get_all(GetAllData* data);
static CommandResult* execute_search(SearchData* data);
static CommandResult* execute_count(CountData* data);
static CommandResult* execute_create_index(CreateIndexData* data);

/**
 * Execute a parsed command
 */
CommandResult* execute_command(Command* cmd) {
    CommandResult* result = (CommandResult*)malloc(sizeof(CommandResult));
    if(result == NULL) {
        return NULL;
    }

    if(cmd == NULL) {
        result->code    = -1;
        result->message = EXECUTION_ERROR_MESSAGES[EX_INVALID_DATA];
        return result;
    }

    if(cmd->op == OP_ERROR) {
        fprintf(stderr, "Error: %s\n", cmd->data.error);
        result->code    = -1;
        result->message = cmd->data.error;
        return result;
    }

    free(result);
    switch(cmd->op) {
        case OP_CREATE:  return execute_create(&cmd->data.create);
        case OP_ADD:     return execute_add(&cmd->data.add);
        case OP_UP:      return execute_up(&cmd->data.update);
        case OP_GET:     return execute_get(&cmd->data.get);
        case OP_DEL:     return execute_del(&cmd->data.delete);
        case OP_GET_ALL: return execute_get_all(&cmd->data.get_all);
        case OP_SEARCH:  return execute_search(&cmd->data.search);
        case OP_COUNT:   return execute_count(&cmd->data.count);
        case OP_CREATE_INDEX:
            return execute_create_index(&cmd->data.create_index);
        default:
            result = (CommandResult*)malloc(sizeof(CommandResult));
            if(result == NULL) {
                return NULL;
            }
            fprintf(stderr, "Unknown operation\n");
            result->code    = -1;
            result->message = EXECUTION_ERROR_MESSAGES[EX_UNKNOWN_ERROR];
            return result;
    }
}

/**
 * Execute a CREATE operation
 */
static CommandResult* execute_create(CreateData* data) {
    CommandResult* result = (CommandResult*)malloc(sizeof(CommandResult));
    if(result == NULL) {
        return NULL;
    }

    if(data == NULL || data->fields == NULL || data->fieldCount <= 0) {
        fprintf(stderr, "Invalid CREATE data\n");
        result->code    = -1;
        result->message = EXECUTION_ERROR_MESSAGES[EX_INVALID_DATA];
        return result;
    }

    // Create the database
    DB* db = db_create(data->dbName, data->fields, data->fieldCount);
    if(db == NULL) {
        fprintf(stderr, "Failed to create database '%s'\n", data->dbName);
        result->code    = -1;
        result->message = EXECUTION_ERROR_MESSAGES[EX_DB_CREATE_FAILED];
        return result;
    }

    // Add to storage (checks for duplicates)
    int add_result = add_db(db);
    if(add_result != 0) {
        if(add_result == -2) {
            fprintf(stderr, "Database '%s' already exists\n", data->dbName);
            result->message = EXECUTION_ERROR_MESSAGES[EX_DB_ALREADY_EXISTS];
        } else if(add_result == -3) {
            fprintf(stderr,
                    "Memory allocation failed when adding database '%s'\n",
                    data->dbName);
            result->message =
                EXECUTION_ERROR_MESSAGES[EX_MEMORY_ALLOCATION_FAILED];
        } else {
            fprintf(stderr, "Failed to add database '%s'\n", data->dbName);
            result->message = EXECUTION_ERROR_MESSAGES[EX_UNKNOWN_ERROR];
        }
        db_free(db);
        result->code = add_result;
        return result;
    }

    printf("Database '%s' created successfully with %d fields\n",
           data->dbName,
           data->fieldCount);
    result->code    = data->fieldCount;
    result->message = NULL;
    return result;
}

/**
 * Execute an ADD operation
 */
static CommandResult* execute_add(AddData* data) {
    CommandResult* result = (CommandResult*)malloc(sizeof(CommandResult));
    if(result == NULL) {
        return NULL;
    }
    // TODO: Implement ADD operation
    printf("Executing ADD for database: %s\n", data->dbName);
    result->code    = 0;
    result->message = NULL;
    return result;
}

/**
 * Execute an UP (update) operation
 */
static CommandResult* execute_up(UpdateData* data) {
    CommandResult* result = (CommandResult*)malloc(sizeof(CommandResult));
    if(result == NULL) {
        return NULL;
    }
    // TODO: Implement UP operation
    printf("Executing UP for database: %s\n", data->dbName);
    result->code    = 0;
    result->message = NULL;
    return result;
}

/**
 * Execute a GET operation
 */
static CommandResult* execute_get(GetData* data) {
    CommandResult* result = (CommandResult*)malloc(sizeof(CommandResult));
    if(result == NULL) {
        return NULL;
    }
    // TODO: Implement GET operation
    printf("Executing GET for database: %s\n", data->dbName);
    result->code    = 0;
    result->message = NULL;
    return result;
}

/**
 * Execute a DEL (delete) operation
 */
static CommandResult* execute_del(DeleteData* data) {
    CommandResult* result = (CommandResult*)malloc(sizeof(CommandResult));
    if(result == NULL) {
        return NULL;
    }
    // TODO: Implement DEL operation
    printf("Executing DEL for database: %s\n", data->dbName);
    result->code    = 0;
    result->message = NULL;
    return result;
}

/**
 * Execute a GET_ALL operation
 */
static CommandResult* execute_get_all(GetAllData* data) {
    CommandResult* result = (CommandResult*)malloc(sizeof(CommandResult));
    if(result == NULL) {
        return NULL;
    }
    // TODO: Implement GET_ALL operation
    printf("Executing GET_ALL for database: %s\n", data->dbName);
    result->code    = 0;
    result->message = NULL;
    return result;
}

/**
 * Execute a SEARCH operation
 */
static CommandResult* execute_search(SearchData* data) {
    CommandResult* result = (CommandResult*)malloc(sizeof(CommandResult));
    if(result == NULL) {
        return NULL;
    }
    // TODO: Implement SEARCH operation
    printf("Executing SEARCH for database: %s\n", data->dbName);
    result->code    = 0;
    result->message = NULL;
    return result;
}

/**
 * Execute a COUNT operation
 */
static CommandResult* execute_count(CountData* data) {
    CommandResult* result = (CommandResult*)malloc(sizeof(CommandResult));
    if(result == NULL) {
        return NULL;
    }
    // TODO: Implement COUNT operation
    printf("Executing COUNT for database: %s\n", data->dbName);
    result->code    = 0;
    result->message = NULL;
    return result;
}

/**
 * Execute a CREATE_INDEX operation
 */
static CommandResult* execute_create_index(CreateIndexData* data) {
    CommandResult* result = (CommandResult*)malloc(sizeof(CommandResult));
    if(result == NULL) {
        return NULL;
    }
    // TODO: Implement CREATE_INDEX operation
    printf("Executing CREATE_INDEX for database: %s\n", data->dbName);
    result->code    = 0;
    result->message = NULL;
    return result;
}
