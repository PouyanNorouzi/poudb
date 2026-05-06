#include "db/operations.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "auth.h"
#include "db/data_model.h"
#include "utils/log.h"

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
    "Index already exists",               /* EX_INDEX_ALREADY_EXISTS */
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
static int   validate_value_types(DB* db, Data* values, int valueCount);
static char* format_row_as_table(DB*    db,
                                 Row*   row,
                                 char** fields,
                                 int    fieldCount);
static char* format_rows_as_table(DB* db, char** fields, int fieldCount);
static char* format_search_results(DB*    db,
                                   Row**  matchingRows,
                                   int    matchCount,
                                   char** fields,
                                   int    fieldCount);
static Row** collect_rows(DB* db, int* rowCount);
static int   find_field_index(DB* db, const char* fieldName);
static int   compare_values(Data* a, Data* b, FieldType type);
static void  build_table_header(char** ptr,
                                DB*    db,
                                int*   fieldIndices,
                                int*   colWidths,
                                int    numFields);
static void  build_data_row(char** ptr,
                            DB*    db,
                            Row*   row,
                            int*   fieldIndices,
                            int*   colWidths,
                            int    numFields);

/**
 * Execute a parsed command
 */
CommandResult* execute_command(Command* cmd) {
    CommandResult* result = (CommandResult*)malloc(sizeof(CommandResult));
    if(result == NULL) {
        return NULL;
    }

    if(cmd == NULL) {
        log_debug("execute_command: received NULL command");
        result->code    = -1;
        result->message = EXECUTION_ERROR_MESSAGES[EX_INVALID_DATA];
        return result;
    }

    log_debug("execute_command: dispatching op=%d", (int)cmd->op);

    if(cmd->op == OP_ERROR) {
        log_info("Client parse error: %s", cmd->data.error);
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
        case OP_ADD_KEY:   return execute_add_key(&cmd->data.add_key);
        case OP_DEL_KEY:   return execute_del_key(&cmd->data.del_key);
        case OP_LIST_KEYS: return execute_list_keys();
        case OP_WHOAMI:    return execute_whoami(&cmd->data.whoami);
        default:
            result = (CommandResult*)malloc(sizeof(CommandResult));
            if(result == NULL) {
                return NULL;
            }
            log_warn("Unknown operation");
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
 * Execute a WHOAMI operation: return the authenticated key name.
 */
CommandResult* execute_whoami(const WhoamiData* data) {
    log_debug("WHOAMI: name='%s'", data->name);

    CommandResult* result = (CommandResult*)malloc(sizeof(CommandResult));
    if(result == NULL) {
        return NULL;
    }
    result->message = NULL;

    char* name_copy = (char*)malloc(AUTH_KEY_NAME_MAX);
    if(name_copy == NULL) {
        result->code    = -1;
        result->message = EXECUTION_ERROR_MESSAGES[EX_MEMORY_ALLOCATION_FAILED];
        result->data    = NULL;
        return result;
    }

    strncpy(name_copy, data->name, AUTH_KEY_NAME_MAX - 1);
    name_copy[AUTH_KEY_NAME_MAX - 1] = '\0';

    result->code = 1;
    result->data = name_copy;
    return result;
}

/**
 * Execute an ADD_KEY operation: generate a token, store its hash, return token.
 */
CommandResult* execute_add_key(const AddKeyData* data) {
    log_debug("ADD_KEY: name='%s' role=%d", data->name, (int)data->role);

    CommandResult* result = (CommandResult*)malloc(sizeof(CommandResult));
    if(result == NULL) {
        return NULL;
    }
    result->data    = NULL;
    result->message = NULL;

    char* token = (char*)malloc(AUTH_TOKEN_BUF_SIZE);
    if(token == NULL) {
        result->code    = -1;
        result->message = EXECUTION_ERROR_MESSAGES[EX_MEMORY_ALLOCATION_FAILED];
        return result;
    }

    log_debug("ADD_KEY: generating token");
    if(auth_generate_token(token) != 0) {
        free(token);
        result->code    = -1;
        result->message = "Failed to generate token";
        return result;
    }

    log_debug("ADD_KEY: storing key in auth store");
    if(auth_add_key(&g_auth_store, data->name, token, data->role) != 0) {
        free(token);
        result->code    = -1;
        result->message = "Failed to add key: store full or name already exists";
        return result;
    }

    log_debug("ADD_KEY: key '%s' added successfully", data->name);
    /* Return the raw token in data (shown to the caller exactly once) */
    result->code = 1;
    result->data = token;
    return result;
}

/**
 * Execute a DEL_KEY operation: remove a key from the auth store.
 */
CommandResult* execute_del_key(const DelKeyData* data) {
    log_debug("DEL_KEY: name='%s'", data->name);

    CommandResult* result = (CommandResult*)malloc(sizeof(CommandResult));
    if(result == NULL) {
        return NULL;
    }
    result->data    = NULL;
    result->message = NULL;

    if(auth_del_key(&g_auth_store, data->name) != 0) {
        log_debug("DEL_KEY: key '%s' not found", data->name);
        result->code    = -1;
        result->message = "Key not found";
        return result;
    }

    log_debug("DEL_KEY: key '%s' removed", data->name);
    result->code = 1;
    return result;
}

/**
 * Execute a LIST_KEYS operation: return a table of {name, role}.
 */
CommandResult* execute_list_keys(void) {
    log_debug("LIST_KEYS: listing %d key(s)", g_auth_store.count);

    CommandResult* result = (CommandResult*)malloc(sizeof(CommandResult));
    if(result == NULL) {
        return NULL;
    }
    result->data    = NULL;
    result->message = NULL;

    /* Build a simple text table: name | role */
    /* Max size: header + separator + 64 rows * max line width */
    const int max_line = AUTH_KEY_NAME_MAX + 16;
    const int buf_size = 64 + (AUTH_MAX_KEYS * max_line);
    char*     buf      = (char*)malloc((size_t)buf_size);
    if(buf == NULL) {
        result->code    = -1;
        result->message = EXECUTION_ERROR_MESSAGES[EX_MEMORY_ALLOCATION_FAILED];
        return result;
    }

    int n = 0;
    n += snprintf(buf + n, (size_t)(buf_size - n),
                  "%-63s | role\n", "name");
    n += snprintf(buf + n, (size_t)(buf_size - n),
                  "%-63s-+----------\n",
                  "----------------------------------------------------------------");

    for(int i = 0; i < g_auth_store.count && n < buf_size - 1; i++) {
        const char* role_str =
            (g_auth_store.keys[i].role == ROLE_ADMIN) ? "admin" : "readonly";
        n += snprintf(buf + n, (size_t)(buf_size - n),
                      "%-63s | %s\n",
                      g_auth_store.keys[i].name,
                      role_str);
    }

    result->code = g_auth_store.count;
    result->data = buf;
    return result;
}

/**
 * Execute a CREATE operation
 */
static CommandResult* execute_create(CreateData* data) {
    log_debug("CREATE: db='%s' fieldCount=%d",
              data ? data->dbName : "(null)",
              data ? data->fieldCount : 0);

    CommandResult* result = (CommandResult*)malloc(sizeof(CommandResult));
    if(result == NULL) {
        return NULL;
    }

    if(data == NULL || data->fields == NULL || data->fieldCount <= 0) {
        log_error("Invalid CREATE data");
        result->code    = -1;
        result->message = EXECUTION_ERROR_MESSAGES[EX_INVALID_DATA];
        return result;
    }

    log_debug("CREATE: allocating DB structure for '%s'", data->dbName);
    // Create the database
    DB* db = db_create(data->dbName, data->fields, data->fieldCount);
    if(db == NULL) {
        log_error("Failed to create database '%s'", data->dbName);
        result->code    = -1;
        result->message = EXECUTION_ERROR_MESSAGES[EX_DB_CREATE_FAILED];
        return result;
    }

    log_debug("CREATE: registering '%s' in storage", data->dbName);
    // Add to storage (checks for duplicates)
    int add_result = add_db(db);
    if(add_result != 0) {
        if(add_result == -2) {
            log_warn("Database '%s' already exists", data->dbName);
            result->message = EXECUTION_ERROR_MESSAGES[EX_DB_ALREADY_EXISTS];
        } else if(add_result == -3) {
            log_error("Memory allocation failed when adding database '%s'",
                    data->dbName);
            result->message =
                EXECUTION_ERROR_MESSAGES[EX_MEMORY_ALLOCATION_FAILED];
        } else {
            log_error("Failed to add database '%s'", data->dbName);
            result->message = EXECUTION_ERROR_MESSAGES[EX_UNKNOWN_ERROR];
        }
        db_free(db);
        result->code = add_result;
        result->data = NULL;
        return result;
    }

    log_info("Database '%s' created successfully with %d fields",
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
    log_debug("ADD: db='%s' valueCount=%d autoKey=%d key=%d",
              data->dbName, data->valueCount, data->autoKey, data->key);

    CommandResult* result = (CommandResult*)malloc(sizeof(CommandResult));
    if(result == NULL) {
        return NULL;
    }

    DB* db = find_db(data->dbName);
    if(db == NULL) {
        log_debug("ADD: db '%s' not found", data->dbName);
        result->code    = -1;
        result->message = EXECUTION_ERROR_MESSAGES[EX_DB_NOT_FOUND];
        return result;
    }

    // valueCount should match fieldsCount - 1 (excluding auto-generated key)
    if(db->fieldsCount - 1 != data->valueCount) {
        log_debug("ADD: value count mismatch (got %d, expected %d)",
                  data->valueCount, db->fieldsCount - 1);
        result->code    = -1;
        result->message = EXECUTION_ERROR_MESSAGES[EX_TYPE_MISMATCH];
        return result;
    }

    log_debug("ADD: validating value types for '%s'", data->dbName);
    // Validate that value types match field types
    if(validate_value_types(db, data->values, data->valueCount) != 0) {
        result->code    = -1;
        result->message = EXECUTION_ERROR_MESSAGES[EX_TYPE_MISMATCH];
        return result;
    }

    // Use -1 for auto-generated key, otherwise use provided key
    int key = data->autoKey ? -1 : data->key;
    log_debug("ADD: inserting row into '%s' (requested key=%d)", data->dbName, key);

    int add_result = db_add_row(db, key, data->values, data->valueCount);
    if(add_result != 0) {
        result->code = add_result;
        if(add_result == -3) {
            result->message = "Maximum row capacity reached";
        } else if(add_result == -4) {
            result->message =
                EXECUTION_ERROR_MESSAGES[EX_MEMORY_ALLOCATION_FAILED];
        } else if(add_result == -5) {
            result->message = "Row key already exists";
        } else {
            result->message = EXECUTION_ERROR_MESSAGES[EX_UNKNOWN_ERROR];
        }
        return result;
    }

    int actualKey = data->autoKey ? (db->nextKey - 1) : data->key;

    log_debug("Added row to database '%s' (key: %d)", data->dbName, actualKey);
    result->code    = actualKey;
    result->message = NULL;
    result->data    = NULL;
    return result;
}

/**
 * Execute an UP (update) operation
 */
static CommandResult* execute_up(UpdateData* data) {
    log_debug("UP: db='%s' key=%d valueCount=%d",
              data ? data->dbName : "(null)",
              data ? data->key : -1,
              data ? data->valueCount : 0);

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
        log_debug("UP: db '%s' not found", data->dbName);
        result->code    = -1;
        result->message = EXECUTION_ERROR_MESSAGES[EX_DB_NOT_FOUND];
        return result;
    }

    // valueCount should match fieldsCount - 1 (excluding auto-generated key)
    if(db->fieldsCount - 1 != data->valueCount) {
        log_debug("UP: value count mismatch (got %d, expected %d)",
                  data->valueCount, db->fieldsCount - 1);
        result->code    = -1;
        result->message = EXECUTION_ERROR_MESSAGES[EX_TYPE_MISMATCH];
        return result;
    }

    log_debug("UP: validating types for %d field(s)", data->valueCount);
    // Validate that non-ignored value types match field types
    for(int i = 0; i < data->valueCount; i++) {
        if(data->ignoreFlags != NULL && data->ignoreFlags[i]) {
            continue;  // Skip validation for ignored fields
        }
        // Create a temporary single-value array for validation
        Data      singleValue  = data->values[i];
        int       fieldIdx     = i + 1;  // Skip key field
        FieldType expectedType = db->fields[fieldIdx].type;

        int valid = 0;
        switch(expectedType) {
            case TYPE_INT:    valid = (singleValue.size == -2); break;
            case TYPE_DOUBLE: valid = (singleValue.size == -1); break;
            case TYPE_BOOL:   valid = (singleValue.size == -3); break;
            case TYPE_STRING: valid = (singleValue.size >= 0); break;
            default:          valid = 0;
        }

        if(!valid) {
            result->code    = -1;
            result->message = EXECUTION_ERROR_MESSAGES[EX_TYPE_MISMATCH];
            return result;
        }
    }

    log_debug("UP: applying update to row %d in '%s'", data->key, data->dbName);
    int update_result = db_update_row(db,
                                      data->key,
                                      data->values,
                                      data->valueCount,
                                      data->ignoreFlags);
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

    log_debug("Updated row in database '%s' (key: %d)", data->dbName, data->key);
    result->code    = data->key;
    result->message = NULL;
    result->data    = NULL;
    return result;
}

/**
 * Execute a GET operation
 */
static CommandResult* execute_get(GetData* data) {
    log_debug("GET: db='%s' key=%d fieldCount=%d",
              data ? data->dbName : "(null)",
              data ? data->key : -1,
              data ? data->fieldCount : 0);

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
        log_debug("GET: db '%s' not found", data->dbName);
        result->code    = -1;
        result->message = EXECUTION_ERROR_MESSAGES[EX_DB_NOT_FOUND];
        return result;
    }

    // Validate field names if specific fields are requested
    if(data->fields != NULL && data->fieldCount > 0) {
        log_debug("GET: validating %d requested field name(s)", data->fieldCount);
        for(int i = 0; i < data->fieldCount; i++) {
            if(find_field_index(db, data->fields[i]) == -1) {
                log_debug("GET: unknown field '%s'", data->fields[i]);
                result->code    = -1;
                result->message = EXECUTION_ERROR_MESSAGES[EX_INVALID_FIELD];
                return result;
            }
        }
    }

    log_debug("GET: fetching row %d from '%s'", data->key, data->dbName);
    // Get the row
    Row* row = db_get_row(db, data->key);
    if(row == NULL) {
        log_debug("GET: row %d not found in '%s'", data->key, data->dbName);
        result->code    = -1;
        result->message = EXECUTION_ERROR_MESSAGES[EX_ROW_NOT_FOUND];
        return result;
    }

    log_debug("GET: formatting row %d as table", data->key);
    // Format the row as a table
    char* table = format_row_as_table(db, row, data->fields, data->fieldCount);
    db_free_row(db, row);

    if(table == NULL) {
        result->code    = -1;
        result->message = EXECUTION_ERROR_MESSAGES[EX_MEMORY_ALLOCATION_FAILED];
        return result;
    }

    log_debug("GET: returning row %d from '%s'", data->key, data->dbName);
    result->code    = data->key;
    result->message = NULL;
    result->data    = table;
    return result;
}

/**
 * Execute a DEL (delete) operation
 */
static CommandResult* execute_del(DeleteData* data) {
    log_debug("DEL: db='%s' key=%d",
              data ? data->dbName : "(null)",
              data ? data->key : -1);

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
        log_debug("DEL: db '%s' not found", data->dbName);
        result->code    = -1;
        result->message = EXECUTION_ERROR_MESSAGES[EX_DB_NOT_FOUND];
        return result;
    }

    log_debug("DEL: removing row %d from '%s'", data->key, data->dbName);
    int delete_result = db_delete_row(db, data->key);
    if(delete_result != 0) {
        result->code = delete_result;
        if(delete_result == -2) {
            result->message = EXECUTION_ERROR_MESSAGES[EX_ROW_NOT_FOUND];
        } else {
            result->message = EXECUTION_ERROR_MESSAGES[EX_UNKNOWN_ERROR];
        }
        return result;
    }

    log_debug("Deleted row from database '%s' (key: %d)",
           data->dbName,
           data->key);
    result->code    = data->key;
    result->message = NULL;
    return result;
}

/**
 * Execute a GET_ALL operation
 */
static CommandResult* execute_get_all(GetAllData* data) {
    log_debug("GET_ALL: db='%s' fieldCount=%d",
              data ? data->dbName : "(null)",
              data ? data->fieldCount : 0);

    CommandResult* result = (CommandResult*)malloc(sizeof(CommandResult));
    if(result == NULL) {
        return NULL;
    }
    result->data = NULL;

    if(data == NULL) {
        result->code    = -EX_INVALID_DATA;
        result->message = EXECUTION_ERROR_MESSAGES[EX_INVALID_DATA];
        return result;
    }

    DB* db = find_db(data->dbName);
    if(db == NULL) {
        log_debug("GET_ALL: db '%s' not found", data->dbName);
        result->code    = -EX_DB_NOT_FOUND;
        result->message = EXECUTION_ERROR_MESSAGES[EX_DB_NOT_FOUND];
        return result;
    }

    log_debug("GET_ALL: '%s' has %d row(s)", data->dbName, db->rowsCount);
    // Validate field names if specific fields are requested
    if(data->fields != NULL && data->fieldCount > 0) {
        log_debug("GET_ALL: validating %d requested field name(s)", data->fieldCount);
        for(int i = 0; i < data->fieldCount; i++) {
            if(find_field_index(db, data->fields[i]) < 0) {
                log_debug("GET_ALL: unknown field '%s'", data->fields[i]);
                result->code    = -EX_INVALID_FIELD;
                result->message = EXECUTION_ERROR_MESSAGES[EX_INVALID_FIELD];
                return result;
            }
        }
    }

    log_debug("GET_ALL: formatting %d row(s) as table", db->rowsCount);
    // Format all rows as a table
    char* table = format_rows_as_table(db, data->fields, data->fieldCount);
    if(table == NULL) {
        result->code    = -EX_MEMORY_ALLOCATION_FAILED;
        result->message = EXECUTION_ERROR_MESSAGES[EX_MEMORY_ALLOCATION_FAILED];
        return result;
    }

    log_debug("GET_ALL: returning %d row(s) from '%s'", db->rowsCount, data->dbName);
    result->code    = db->rowsCount;
    result->message = NULL;
    result->data    = table;
    return result;
}

/**
 * Execute a SEARCH operation
 */
static CommandResult* execute_search(SearchData* data) {
    log_debug("SEARCH: db='%s' field='%s' returnFieldCount=%d",
              data ? data->dbName : "(null)",
              data ? data->fieldName : "(null)",
              data ? data->fieldCount : 0);

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
        log_debug("SEARCH: db '%s' not found", data->dbName);
        result->code    = -1;
        result->message = EXECUTION_ERROR_MESSAGES[EX_DB_NOT_FOUND];
        return result;
    }

    // Find the search field index
    int searchFieldIdx = find_field_index(db, data->fieldName);
    if(searchFieldIdx < 0) {
        log_debug("SEARCH: field '%s' not found in '%s'", data->fieldName, data->dbName);
        result->code    = -1;
        result->message = EXECUTION_ERROR_MESSAGES[EX_INVALID_FIELD];
        return result;
    }

    log_debug("SEARCH: field '%s' is at index %d", data->fieldName, searchFieldIdx);

    // Validate return field names if specific fields are requested
    if(data->returnFields != NULL && data->fieldCount > 0) {
        log_debug("SEARCH: validating %d return field name(s)", data->fieldCount);
        for(int i = 0; i < data->fieldCount; i++) {
            if(find_field_index(db, data->returnFields[i]) < 0) {
                log_debug("SEARCH: unknown return field '%s'", data->returnFields[i]);
                result->code    = -1;
                result->message = EXECUTION_ERROR_MESSAGES[EX_INVALID_FIELD];
                return result;
            }
        }
    }

    int       matchCount      = 0;
    FieldType searchFieldType = db->fields[searchFieldIdx].type;

    Row** matchingRows = NULL;
    int   hasIndex     = db_has_index(db, searchFieldIdx);

    log_debug("SEARCH: using %s for field '%s'",
              hasIndex == 1 ? "index" : "full scan",
              data->fieldName);

    if(hasIndex == 1) {
        int indexResult = db_index_collect_rows(db,
                                                searchFieldIdx,
                                                &data->value,
                                                &matchingRows,
                                                &matchCount);
        if(indexResult == -3) {
            result->code = -1;
            result->message =
                EXECUTION_ERROR_MESSAGES[EX_MEMORY_ALLOCATION_FAILED];
            return result;
        }
        if(indexResult != 0) {
            log_debug("SEARCH: index lookup failed (rc=%d), falling back to scan",
                      indexResult);
            hasIndex = 0;
        }
    }

    if(hasIndex != 1) {
        matchingRows = (Row**)malloc(sizeof(Row*) * db->rowsCount);
        if(matchingRows == NULL && db->rowsCount > 0) {
            result->code = -1;
            result->message =
                EXECUTION_ERROR_MESSAGES[EX_MEMORY_ALLOCATION_FAILED];
            return result;
        }

        DBRowIterator it;
        Row*          row = db_iter_first(db, &it);
        while(row != NULL) {
            Data* rowValue = &row->values[searchFieldIdx];
            if(compare_values(rowValue, &data->value, searchFieldType) == 0) {
                matchingRows[matchCount++] = row;
            }
            row = db_iter_next(db, &it);
        }
    }

    log_debug("SEARCH: found %d match(es) in '%s'", matchCount, data->dbName);

    // Format the matching rows as a table
    char* table = format_search_results(db,
                                        matchingRows,
                                        matchCount,
                                        data->returnFields,
                                        data->fieldCount);
    free(matchingRows);

    if(table == NULL) {
        result->code    = -1;
        result->message = EXECUTION_ERROR_MESSAGES[EX_MEMORY_ALLOCATION_FAILED];
        return result;
    }

    result->code    = matchCount;
    result->message = NULL;
    result->data    = table;
    return result;
}

/**
 * Execute a COUNT operation
 */
static CommandResult* execute_count(CountData* data) {
    log_debug("COUNT: db='%s'", data ? data->dbName : "(null)");

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
        log_debug("COUNT: db '%s' not found", data->dbName);
        result->code    = -1;
        result->message = EXECUTION_ERROR_MESSAGES[EX_DB_NOT_FOUND];
        return result;
    }

    log_debug("COUNT: '%s' has %d row(s)", data->dbName, db->rowsCount);
    result->code    = db->rowsCount;
    result->message = NULL;
    return result;
}

/**
 * Execute a CREATE_INDEX operation
 */
static CommandResult* execute_create_index(CreateIndexData* data) {
    log_debug("CREATE_INDEX: db='%s' field='%s'",
              data ? data->dbName : "(null)",
              data ? data->fieldName : "(null)");

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
        log_debug("CREATE_INDEX: db '%s' not found", data->dbName);
        result->code    = -1;
        result->message = EXECUTION_ERROR_MESSAGES[EX_DB_NOT_FOUND];
        return result;
    }

    int fieldIdx = find_field_index(db, data->fieldName);
    if(fieldIdx < 0) {
        log_debug("CREATE_INDEX: field '%s' not found in '%s'",
                  data->fieldName, data->dbName);
        result->code    = -1;
        result->message = EXECUTION_ERROR_MESSAGES[EX_INVALID_FIELD];
        return result;
    }

    log_debug("CREATE_INDEX: building index on field '%s' (idx=%d) in '%s'",
              data->fieldName, fieldIdx, data->dbName);
    int createResult = db_create_index(db, fieldIdx);
    if(createResult != 0) {
        result->code = createResult;
        if(createResult == -2) {
            log_debug("CREATE_INDEX: index already exists for '%s'", data->fieldName);
            result->message = EXECUTION_ERROR_MESSAGES[EX_INDEX_ALREADY_EXISTS];
        } else if(createResult == -3) {
            result->message =
                EXECUTION_ERROR_MESSAGES[EX_MEMORY_ALLOCATION_FAILED];
        } else {
            result->message = EXECUTION_ERROR_MESSAGES[EX_UNKNOWN_ERROR];
        }
        return result;
    }

    log_debug("CREATE_INDEX: index on '%s' built successfully (fieldIdx=%d)",
              data->fieldName, fieldIdx);
    result->code    = fieldIdx;
    result->message = NULL;
    return result;
}

/**
 * Validate that data types match field types
 * Parser uses: size = -3 for bool, -2 for int, -1 for double, >= 0 for string
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
                    log_warn("Type mismatch at field '%s': expected int",
                            db->fields[fieldIdx].name);
                    return -1;
                }
                break;

            case TYPE_DOUBLE:
                // Parser sets size = -1 for doubles
                if(val->size != -1) {
                    log_warn("Type mismatch at field '%s': expected double",
                            db->fields[fieldIdx].name);
                    return -1;
                }
                break;

            case TYPE_BOOL:
                // Parser sets size = -3 for booleans
                if(val->size != -3) {
                    log_warn("Type mismatch at field '%s': expected bool",
                            db->fields[fieldIdx].name);
                    return -1;
                }
                break;

            case TYPE_STRING:
                // For strings, size >= 0 is the string length
                // and value.s should not be NULL (can be empty string)
                if(val->size < 0) {
                    log_warn("Type mismatch at field '%s': expected string",
                            db->fields[fieldIdx].name);
                    return -1;
                }
                break;

            default:
                log_warn("Unknown field type at '%s'",
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

static char* format_row_as_table(DB*    db,
                                 Row*   row,
                                 char** fields,
                                 int    fieldCount) {
    if(db == NULL || row == NULL) {
        return NULL;
    }

    // Determine which fields to include
    int* fieldIndices = NULL;
    int  numFields    = 0;

    if(fields == NULL || fieldCount == 0) {
        // Include all fields
        numFields = db->fieldsCount;
        if(numFields <= 0) {
            return NULL;
        }
        fieldIndices = (int*)malloc(sizeof(int) * numFields);
        if(fieldIndices == NULL) {
            return NULL;
        }
        for(int i = 0; i < numFields; i++) {
            fieldIndices[i] = i;
        }
    } else {
        // Include only specified fields
        numFields = fieldCount;
        if(numFields <= 0) {
            return NULL;
        }
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
            default: valLen = 1;
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
    int   bufSize = (lineWidth + 1) * 3 + 1;  // +1 for newlines and null
    char* table   = (char*)malloc(bufSize);
    if(table == NULL) {
        free(colWidths);
        free(fieldIndices);
        return NULL;
    }

    char* ptr = table;

    build_table_header(&ptr, db, fieldIndices, colWidths, numFields);
    build_data_row(&ptr, db, row, fieldIndices, colWidths, numFields);
    *ptr = '\0';

    free(colWidths);
    free(fieldIndices);

    return table;
}

static char* format_rows_as_table(DB* db, char** fields, int fieldCount) {
    if(db == NULL) {
        return NULL;
    }

    // Handle empty database
    if(db->rowsCount == 0) {
        const char* emptyMsg = "(No rows)\n";
        char*       result   = (char*)malloc(strlen(emptyMsg) + 1);
        if(result) {
            strcpy(result, emptyMsg);
        }
        return result;
    }

    // Determine which fields to include
    int* fieldIndices = NULL;
    int  numFields    = 0;

    if(fields == NULL || fieldCount == 0) {
        // Include all fields
        numFields = db->fieldsCount;
        if(numFields <= 0) {
            return NULL;
        }
        fieldIndices = (int*)malloc(sizeof(int) * numFields);
        if(fieldIndices == NULL) {
            return NULL;
        }
        for(int i = 0; i < numFields; i++) {
            fieldIndices[i] = i;
        }
    } else {
        // Include only specified fields
        numFields = fieldCount;
        if(numFields <= 0) {
            return NULL;
        }
        fieldIndices = (int*)malloc(sizeof(int) * numFields);
        if(fieldIndices == NULL) {
            return NULL;
        }
        for(int i = 0; i < numFields; i++) {
            fieldIndices[i] = find_field_index(db, fields[i]);
        }
    }

    int   rowCount = 0;
    Row** rows     = collect_rows(db, &rowCount);
    if(rows == NULL && db->rowsCount > 0) {
        free(fieldIndices);
        return NULL;
    }

    // Calculate column widths
    int* colWidths = (int*)malloc(sizeof(int) * numFields);
    if(colWidths == NULL) {
        free(rows);
        free(fieldIndices);
        return NULL;
    }

    // Buffer for converting values to strings
    char valueBuf[256];

    for(int i = 0; i < numFields; i++) {
        int idx = fieldIndices[i];
        // Start with header width
        colWidths[i] = strlen(db->fields[idx].name);

        // Check all row values to find maximum width
        for(int r = 0; r < rowCount; r++) {
            Data* val = &rows[r]->values[idx];
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
                default: valLen = 1;
            }
            if(valLen > colWidths[i]) {
                colWidths[i] = valLen;
            }
        }
    }

    // Calculate total line width: | col1 | col2 | ... |
    int lineWidth = 1;  // Leading |
    for(int i = 0; i < numFields; i++) {
        lineWidth += colWidths[i] + 3;  // " value |"
    }

    // Allocate buffer for the table (header + separator + data rows + null)
    int   bufSize = (lineWidth + 1) * (2 + rowCount) + 1;
    char* table   = (char*)malloc(bufSize);
    if(table == NULL) {
        free(colWidths);
        free(rows);
        free(fieldIndices);
        return NULL;
    }

    char* ptr = table;

    build_table_header(&ptr, db, fieldIndices, colWidths, numFields);

    // Build data lines for all rows
    for(int r = 0; r < rowCount; r++) {
        build_data_row(&ptr, db, rows[r], fieldIndices, colWidths, numFields);
    }
    *ptr = '\0';

    free(colWidths);
    free(rows);
    free(fieldIndices);

    return table;
}

static Row** collect_rows(DB* db, int* rowCount) {
    if(rowCount == NULL) {
        return NULL;
    }
    *rowCount = 0;

    if(db == NULL || db->rowsCount <= 0) {
        return NULL;
    }

    Row** rows = (Row**)malloc(sizeof(Row*) * db->rowsCount);
    if(rows == NULL) {
        return NULL;
    }

    DBRowIterator it;
    Row*          row = db_iter_first(db, &it);
    while(row != NULL) {
        rows[*rowCount] = row;
        (*rowCount)++;
        row = db_iter_next(db, &it);
    }

    return rows;
}

static void build_table_header(char** ptr,
                               DB*    db,
                               int*   fieldIndices,
                               int*   colWidths,
                               int    numFields) {
    // Build header line
    **ptr = '|';
    (*ptr)++;
    for(int i = 0; i < numFields; i++) {
        int idx     = fieldIndices[i];
        int padding = colWidths[i] - strlen(db->fields[idx].name);
        **ptr       = ' ';
        (*ptr)++;
        strcpy(*ptr, db->fields[idx].name);
        *ptr += strlen(db->fields[idx].name);
        for(int j = 0; j < padding; j++) {
            **ptr = ' ';
            (*ptr)++;
        }
        **ptr = ' ';
        (*ptr)++;
        **ptr = '|';
        (*ptr)++;
    }
    **ptr = '\n';
    (*ptr)++;

    // Build separator line
    **ptr = '|';
    (*ptr)++;
    for(int i = 0; i < numFields; i++) {
        **ptr = '-';
        (*ptr)++;
        for(int j = 0; j < colWidths[i]; j++) {
            **ptr = '-';
            (*ptr)++;
        }
        **ptr = '-';
        (*ptr)++;
        **ptr = '|';
        (*ptr)++;
    }
    **ptr = '\n';
    (*ptr)++;
}

static void build_data_row(char** ptr,
                           DB*    db,
                           Row*   row,
                           int*   fieldIndices,
                           int*   colWidths,
                           int    numFields) {
    char valueBuf[256];

    **ptr = '|';
    (*ptr)++;
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
            default: strcpy(valueBuf, "?");
        }

        int valLen  = strlen(valueBuf);
        int padding = colWidths[i] - valLen;

        **ptr = ' ';
        (*ptr)++;
        strcpy(*ptr, valueBuf);
        *ptr += valLen;
        for(int j = 0; j < padding; j++) {
            **ptr = ' ';
            (*ptr)++;
        }
        **ptr = ' ';
        (*ptr)++;
        **ptr = '|';
        (*ptr)++;
    }
    **ptr = '\n';
    (*ptr)++;
}

/**
 * Compare two values of the same type
 * Returns 0 if equal, non-zero otherwise
 */
static int compare_values(Data* a, Data* b, FieldType type) {
    switch(type) {
        case TYPE_INT:    return (a->value.i == b->value.i) ? 0 : 1;
        case TYPE_DOUBLE: return (a->value.d == b->value.d) ? 0 : 1;
        case TYPE_BOOL:   return (a->value.b == b->value.b) ? 0 : 1;
        case TYPE_STRING:
            if(a->value.s == NULL && b->value.s == NULL) {
                return 0;
            }
            if(a->value.s == NULL || b->value.s == NULL) {
                return 1;
            }
            return strcmp(a->value.s, b->value.s);
        default: return 1;
    }
}

/**
 * Format search results (matching rows) as a table
 */
static char* format_search_results(DB*    db,
                                   Row**  matchingRows,
                                   int    matchCount,
                                   char** fields,
                                   int    fieldCount) {
    if(db == NULL) {
        return NULL;
    }

    // Handle no matching rows
    if(matchCount == 0) {
        const char* emptyMsg = "(No rows)\n";
        char*       result   = (char*)malloc(strlen(emptyMsg) + 1);
        if(result) {
            strcpy(result, emptyMsg);
        }
        return result;
    }

    // Determine which fields to include
    int* fieldIndices = NULL;
    int  numFields    = 0;

    if(fields == NULL || fieldCount == 0) {
        // Include all fields
        numFields = db->fieldsCount;
        if(numFields <= 0) {
            return NULL;
        }
        fieldIndices = (int*)malloc(sizeof(int) * numFields);
        if(fieldIndices == NULL) {
            return NULL;
        }
        for(int i = 0; i < numFields; i++) {
            fieldIndices[i] = i;
        }
    } else {
        // Include only specified fields
        numFields = fieldCount;
        if(numFields <= 0) {
            return NULL;
        }
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

        // Check all matching row values to find maximum width
        for(int r = 0; r < matchCount; r++) {
            Data* val = &matchingRows[r]->values[idx];
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
                default: valLen = 1;
            }
            if(valLen > colWidths[i]) {
                colWidths[i] = valLen;
            }
        }
    }

    // Calculate total line width: | col1 | col2 | ... |
    int lineWidth = 1;  // Leading |
    for(int i = 0; i < numFields; i++) {
        lineWidth += colWidths[i] + 3;  // " value |"
    }

    // Allocate buffer for the table (header + separator + data rows + null)
    int   bufSize = (lineWidth + 1) * (2 + matchCount) + 1;
    char* table   = (char*)malloc(bufSize);
    if(table == NULL) {
        free(colWidths);
        free(fieldIndices);
        return NULL;
    }

    char* ptr = table;

    build_table_header(&ptr, db, fieldIndices, colWidths, numFields);

    // Build data lines for matching rows
    for(int r = 0; r < matchCount; r++) {
        build_data_row(&ptr,
                       db,
                       matchingRows[r],
                       fieldIndices,
                       colWidths,
                       numFields);
    }
    *ptr = '\0';

    free(colWidths);
    free(fieldIndices);

    return table;
}
