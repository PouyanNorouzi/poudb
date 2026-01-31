#include "db/operations.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
static char* format_row_as_table(DB* db, Row* row, char** fields, int fieldCount);
static int find_field_index(DB* db, const char* fieldName);

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

void free_command_result(CommandResult* result) {
    if(result == NULL) {
        return;
    }
    if(result->data != NULL) {
        free(result->data);
    }
    free(result);
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
    result->data    = NULL;
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
    result->data    = NULL;
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

    if(data == NULL || data->values == NULL) {
        result->code    = -1;
        result->message = EXECUTION_ERROR_MESSAGES[EX_INVALID_DATA];
        return result;
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

    // Validate that non-ignored value types match field types
    for(int i = 0; i < data->valueCount; i++) {
        if(data->ignoreFlags != NULL && data->ignoreFlags[i]) {
            continue;  // Skip validation for ignored fields
        }
        // Create a temporary single-value array for validation
        Data singleValue = data->values[i];
        int fieldIdx = i + 1;  // Skip key field
        FieldType expectedType = db->fields[fieldIdx].type;

        int valid = 0;
        switch(expectedType) {
            case TYPE_INT:    valid = (singleValue.size == -2); break;
            case TYPE_DOUBLE: valid = (singleValue.size == -1); break;
            case TYPE_BOOL:   valid = (singleValue.size == 0);  break;
            case TYPE_STRING: valid = (singleValue.size >= 0);  break;
            default: valid = 0;
        }

        if(!valid) {
            result->code    = -1;
            result->message = EXECUTION_ERROR_MESSAGES[EX_TYPE_MISMATCH];
            return result;
        }
    }

    int update_result = db_update_row(db, data->key, data->values,
                                       data->valueCount, data->ignoreFlags);
    if(update_result != 0) {
        result->code = update_result;
        if(update_result == -3) {
            result->message = EXECUTION_ERROR_MESSAGES[EX_ROW_NOT_FOUND];
        } else if(update_result == -4) {
            result->message =
                EXECUTION_ERROR_MESSAGES[EX_MEMORY_ALLOCATION_FAILED];
        } else {
            result->message = EXECUTION_ERROR_MESSAGES[EX_UNKNOWN_ERROR];
        }
        return result;
    }

    printf("Updated row in database '%s' (key: %d)\n", data->dbName, data->key);
    result->code    = data->key;
    result->message = NULL;
    result->data    = NULL;
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
    result->data = NULL;

    if(data == NULL) {
        result->code    = -1;
        result->message = EXECUTION_ERROR_MESSAGES[EX_INVALID_DATA];
        return result;
    }

    DB* db = find_db(data->dbName);
    if(db == NULL) {
        result->code    = -1;
        result->message = EXECUTION_ERROR_MESSAGES[EX_DB_NOT_FOUND];
        return result;
    }

    // Validate field names if specific fields are requested
    if(data->fields != NULL && data->fieldCount > 0) {
        for(int i = 0; i < data->fieldCount; i++) {
            if(find_field_index(db, data->fields[i]) == -1) {
                result->code    = -1;
                result->message = EXECUTION_ERROR_MESSAGES[EX_INVALID_FIELD];
                return result;
            }
        }
    }

    // Get the row
    Row* row = db_get_row(db, data->key);
    if(row == NULL) {
        result->code    = -1;
        result->message = EXECUTION_ERROR_MESSAGES[EX_ROW_NOT_FOUND];
        return result;
    }

    // Format the row as a table
    char* table = format_row_as_table(db, row, data->fields, data->fieldCount);
    db_free_row(db, row);

    if(table == NULL) {
        result->code    = -1;
        result->message = EXECUTION_ERROR_MESSAGES[EX_MEMORY_ALLOCATION_FAILED];
        return result;
    }

    result->code    = data->key;
    result->message = NULL;
    result->data    = table;
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
 * Parser uses: size = -2 for int, -1 for double, 0 for bool, >= 0 for string
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
                // Parser sets size = -2 for integers
                if(val->size != -2) {
                    fprintf(stderr,
                            "Type mismatch at field '%s': expected int\n",
                            db->fields[fieldIdx].name);
                    return -1;
                }
                break;

            case TYPE_DOUBLE:
                // Parser sets size = -1 for doubles
                if(val->size != -1) {
                    fprintf(stderr,
                            "Type mismatch at field '%s': expected double\n",
                            db->fields[fieldIdx].name);
                    return -1;
                }
                break;

            case TYPE_BOOL:
                // Parser sets size = 0 for booleans
                if(val->size != 0) {
                    fprintf(stderr,
                            "Type mismatch at field '%s': expected bool\n",
                            db->fields[fieldIdx].name);
                    return -1;
                }
                break;

            case TYPE_STRING:
                // For strings, size >= 0 is the string length
                // and value.s should not be NULL (can be empty string)
                if(val->size < 0) {
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

static int find_field_index(DB* db, const char* fieldName) {
    if(db == NULL || fieldName == NULL) {
        return -1;
    }
    for(int i = 0; i < db->fieldsCount; i++) {
        if(strcmp(db->fields[i].name, fieldName) == 0) {
            return i;
        }
    }
    return -1;
}

static char* format_row_as_table(DB* db, Row* row, char** fields, int fieldCount) {
    if(db == NULL || row == NULL) {
        return NULL;
    }

    // Determine which fields to include
    int* fieldIndices = NULL;
    int  numFields    = 0;

    if(fields == NULL || fieldCount == 0) {
        // Include all fields
        numFields    = db->fieldsCount;
        fieldIndices = (int*)malloc(sizeof(int) * numFields);
        if(fieldIndices == NULL) {
            return NULL;
        }
        for(int i = 0; i < numFields; i++) {
            fieldIndices[i] = i;
        }
    } else {
        // Include only specified fields
        numFields    = fieldCount;
        fieldIndices = (int*)malloc(sizeof(int) * numFields);
        if(fieldIndices == NULL) {
            return NULL;
        }
        for(int i = 0; i < numFields; i++) {
            fieldIndices[i] = find_field_index(db, fields[i]);
        }
    }

    // Calculate column widths
    int* colWidths = (int*)malloc(sizeof(int) * numFields);
    if(colWidths == NULL) {
        free(fieldIndices);
        return NULL;
    }

    // Buffer for converting values to strings
    char valueBuf[256];

    for(int i = 0; i < numFields; i++) {
        int idx = fieldIndices[i];
        // Start with header width
        colWidths[i] = strlen(db->fields[idx].name);

        // Check value width
        Data* val = &row->values[idx];
        int   valLen;
        switch(db->fields[idx].type) {
            case TYPE_INT:
                snprintf(valueBuf, sizeof(valueBuf), "%d", val->value.i);
                valLen = strlen(valueBuf);
                break;
            case TYPE_DOUBLE:
                snprintf(valueBuf, sizeof(valueBuf), "%.6g", val->value.d);
                valLen = strlen(valueBuf);
                break;
            case TYPE_BOOL:
                valLen = val->value.b ? 4 : 5;  // "true" or "false"
                break;
            case TYPE_STRING:
                valLen = val->value.s ? strlen(val->value.s) : 4;  // "NULL"
                break;
            default:
                valLen = 1;
        }
        if(valLen > colWidths[i]) {
            colWidths[i] = valLen;
        }
    }

    // Calculate total line width: | col1 | col2 | ... |
    int lineWidth = 1;  // Leading |
    for(int i = 0; i < numFields; i++) {
        lineWidth += colWidths[i] + 3;  // " value |"
    }

    // Allocate buffer for the table (header + separator + data + null)
    // 3 lines: header, separator, data row
    int bufSize = (lineWidth + 1) * 3 + 1;  // +1 for newlines and null
    char* table = (char*)malloc(bufSize);
    if(table == NULL) {
        free(colWidths);
        free(fieldIndices);
        return NULL;
    }

    char* ptr = table;

    // Build header line
    *ptr++ = '|';
    for(int i = 0; i < numFields; i++) {
        int idx = fieldIndices[i];
        int padding = colWidths[i] - strlen(db->fields[idx].name);
        *ptr++ = ' ';
        strcpy(ptr, db->fields[idx].name);
        ptr += strlen(db->fields[idx].name);
        for(int j = 0; j < padding; j++) {
            *ptr++ = ' ';
        }
        *ptr++ = ' ';
        *ptr++ = '|';
    }
    *ptr++ = '\n';

    // Build separator line
    *ptr++ = '|';
    for(int i = 0; i < numFields; i++) {
        *ptr++ = '-';
        for(int j = 0; j < colWidths[i]; j++) {
            *ptr++ = '-';
        }
        *ptr++ = '-';
        *ptr++ = '|';
    }
    *ptr++ = '\n';

    // Build data line
    *ptr++ = '|';
    for(int i = 0; i < numFields; i++) {
        int   idx = fieldIndices[i];
        Data* val = &row->values[idx];

        switch(db->fields[idx].type) {
            case TYPE_INT:
                snprintf(valueBuf, sizeof(valueBuf), "%d", val->value.i);
                break;
            case TYPE_DOUBLE:
                snprintf(valueBuf, sizeof(valueBuf), "%.6g", val->value.d);
                break;
            case TYPE_BOOL:
                strcpy(valueBuf, val->value.b ? "true" : "false");
                break;
            case TYPE_STRING:
                if(val->value.s) {
                    snprintf(valueBuf, sizeof(valueBuf), "%s", val->value.s);
                } else {
                    strcpy(valueBuf, "NULL");
                }
                break;
            default:
                strcpy(valueBuf, "?");
        }

        int valLen  = strlen(valueBuf);
        int padding = colWidths[i] - valLen;

        *ptr++ = ' ';
        strcpy(ptr, valueBuf);
        ptr += valLen;
        for(int j = 0; j < padding; j++) {
            *ptr++ = ' ';
        }
        *ptr++ = ' ';
        *ptr++ = '|';
    }
    *ptr++ = '\n';
    *ptr   = '\0';

    free(colWidths);
    free(fieldIndices);

    return table;
}
