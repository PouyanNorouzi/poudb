#include "db/operations.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

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
static int validate_value_types(DB* db, Data* values, int valueCount);

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

    DB* db = find_db(data->dbName);
    if(db == NULL) {
        result->code    = -1;
        result->message = EXECUTION_ERROR_MESSAGES[EX_DB_NOT_FOUND];
        return result;
    }

    // valueCount should match fieldsCount - 1 (excluding auto-generated key)
    if(db->fieldsCount - 1 != data->valueCount) {
        result->code    = -1;
        result->message = EXECUTION_ERROR_MESSAGES[EX_TYPE_MISMATCH];
        return result;
    }

    // Validate that value types match field types
    if(validate_value_types(db, data->values, data->valueCount) != 0) {
        result->code    = -1;
        result->message = EXECUTION_ERROR_MESSAGES[EX_TYPE_MISMATCH];
        return result;
    }

    // Use -1 for auto-generated key, otherwise use provided key
    int key = data->autoKey ? -1 : data->key;

    int add_result = db_add_row(db, key, data->values, data->valueCount);
    if(add_result != 0) {
        result->code = add_result;
        if(add_result == -3) {
            result->message = "Maximum row capacity reached";
        } else if(add_result == -4) {
            result->message =
                EXECUTION_ERROR_MESSAGES[EX_MEMORY_ALLOCATION_FAILED];
        } else {
            result->message = EXECUTION_ERROR_MESSAGES[EX_UNKNOWN_ERROR];
        }
        return result;
    }

    printf("Added row to database '%s' (key: %d)\n",
           data->dbName,
           db->rows[db->rowsCount - 1].values[0].value.i);
    result->code    = db->rows[db->rowsCount - 1].values[0].value.i;
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

/**
 * Validate that data types match field types
 */
static int validate_value_types(DB* db, Data* values, int valueCount) {
    if(db == NULL || values == NULL) {
        return -1;
    }

    // Values don't include the key field, so we check against fields[1..n]
    for(int i = 0; i < valueCount; i++) {
        int       fieldIdx     = i + 1;  // Skip key field
        FieldType expectedType = db->fields[fieldIdx].type;
        Data*     val          = &values[i];

        switch(expectedType) {
            case TYPE_INT:
                // Check if value looks like an int (size matches)
                if(val->size != sizeof(int)) {
                    fprintf(stderr,
                            "Type mismatch at field '%s': expected int\n",
                            db->fields[fieldIdx].name);
                    return -1;
                }
                break;

            case TYPE_DOUBLE:
                if(val->size != sizeof(double)) {
                    fprintf(stderr,
                            "Type mismatch at field '%s': expected double\n",
                            db->fields[fieldIdx].name);
                    return -1;
                }
                break;

            case TYPE_BOOL:
                if(val->size != sizeof(bool)) {
                    fprintf(stderr,
                            "Type mismatch at field '%s': expected bool\n",
                            db->fields[fieldIdx].name);
                    return -1;
                }
                break;

            case TYPE_STRING:
                // For strings, size is the string length (or 0 for empty)
                // and value.s should be set
                if(val->value.s == NULL && val->size > 0) {
                    fprintf(stderr,
                            "Type mismatch at field '%s': expected string\n",
                            db->fields[fieldIdx].name);
                    return -1;
                }
                break;

            default:
                fprintf(stderr,
                        "Unknown field type at '%s'\n",
                        db->fields[fieldIdx].name);
                return -1;
        }
    }

    return 0;
}
