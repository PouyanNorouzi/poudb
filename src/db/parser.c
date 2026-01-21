#include "db/parser.h"

#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#define COMMAND_LENGTH  9
#define TYPE_STR_LENGTH 32

/**
 * Command strings that match Operation enum values
 */
static const char* COMMAND_STRINGS[] = {"CREATE",
                                        "ADD",
                                        "UP",
                                        "GET",
                                        "DEL",
                                        "GET_ALL",
                                        "SEARCH",
                                        "COUNT",
                                        "CREATE_INDEX"};

const char* ERROR_MESSAGES[] = {
    "Command must be one of CREATE, ADD, UP, GET, DEL, GET_ALL, SEARCH, COUNT, CREATE_INDEX", /* ER_INVALID_COMMAND */
    "Format must be CREATE <DB> (type field1, type field2, ...)", /* ER_INVALID_CREATE_FORMAT
                                                                   */
    "Format must be ADD <DB> <KEY> (value1, value2, ...)", /* ER_INVALID_ADD_FORMAT
                                                            */
    "Format must be UP <DB> <KEY> (value1, value2, ...)", /* ER_INVALID_UPDATE_FORMAT
                                                           */
    "Format must be GET <DB> <KEY> [field1, field2, ...]", /* ER_INVALID_GET_FORMAT
                                                            */
    "Format must be DEL <DB> <KEY>", /* ER_INVALID_DELETE_FORMAT */
    "Format must be SEARCH <DB> <FIELD> <VALUE> [field1, field2, ...]", /* ER_INVALID_SEARCH_FORMAT
                                                                         */
    "Format must be COUNT <DB>",                /* ER_INVALID_COUNT_FORMAT */
    "Format must be CREATE_INDEX <DB> <FIELD>", /* ER_INVALID_INDEX_FORMAT */
    "Missing required argument: %s",            /* ER_MISSING_ARGUMENT */
    "Unexpected argument: %s",                  /* ER_UNEXPECTED_ARGUMENT */
    "Invalid identifier '%s' (must follow naming rules)", /* ER_INVALID_IDENTIFIER
                                                           */
    "Invalid or unsupported data type: %s",    /* ER_INVALID_DATA_TYPE */
    "Syntax error: %s",                        /* ER_SYNTAX_ERROR */
    "Unknown error occured with the parser %s" /* ER_OTHER */
};

/**
 * Function prototypes for private helper functions
 */
static Command* parse_create(const char* input);
static Command* parse_add(const char* input);
static Command* parse_up(const char* input);
static Command* parse_get(const char* input);
static Command* parse_del(const char* input);
static Command* parse_get_all(const char* input);
static Command* parse_search(const char* input);
static Command* parse_count(const char* input);
static Command* parse_create_index(const char* input);
static Command* parse_error(ParseError errorCode, const char* detail);

static Operation determine_operation(const char* input);

static int         parse_field_type(const char* typeStr, FieldType* outType);
static int         is_valid_identifier(const char* str);
static const char* skip_whitespace(const char* str);
static int         parse_int(const char** ptr, int* outValue);
static int         parse_double(const char** ptr, double* outValue);
static char*       parse_string(const char** ptr);

static int  split_at_paren(const char* input,
                           char**      before_paren,
                           char**      inside_paren);
static int  tokenize_single_arg(const char* args_str, char** arg);
static int  tokenize_args(const char* args_str, char** db_name, char** key_str);
static int  tokenize_values(const char* values_str,
                            Data**      values,
                            int*        value_count);
static int  tokenize_fields(const char* fields_str,
                            Field**     fields,
                            int*        field_count);
static int  tokenize_update_values(const char* values_str,
                                   Data**      values,
                                   int**       ignore_flags,
                                   int*        value_count);
static int  parse_single_value(const char** ptr, Data* out_value);
static int  parse_single_field(const char** ptr, Field* out_field);
static int  parse_update_value(const char** ptr,
                               Data*        out_value,
                               int*         is_ignored);
static void free_values_array(Data* values, int count);
static void free_fields_array(Field* fields, int count);

/**
 * Parse a command string into a Command structure
 */
Command* parse_command(const char* input) {
    if(input == NULL || *input == '\0') {
        return parse_error(ER_INVALID_COMMAND, NULL);
    }

    // Skip leading whitespace
    while(*input && (*input == ' ' || *input == '\t')) input++;

    // Check if input is empty after skipping whitespace
    if(*input == '\0') {
        return parse_error(ER_INVALID_COMMAND, NULL);
    }

    // Determine which operation to parse
    Operation op = determine_operation(input);

    if(op == OP_ERROR) {
        return parse_error(ER_INVALID_COMMAND, NULL);
    }

    // Skip past the command to get the arguments
    const char* args = input + strlen(COMMAND_STRINGS[op]);

    // Call the appropriate parsing function with just the arguments
    switch(op) {
        case OP_CREATE:       return parse_create(args);
        case OP_ADD:          return parse_add(args);
        case OP_UP:           return parse_up(args);
        case OP_GET:          return parse_get(args);
        case OP_DEL:          return parse_del(args);
        case OP_GET_ALL:      return parse_get_all(args);
        case OP_SEARCH:       return parse_search(args);
        case OP_COUNT:        return parse_count(args);
        case OP_CREATE_INDEX: return parse_create_index(args);
        default:              return parse_error(ER_INVALID_COMMAND, NULL);
    }
}

/**
 * Parse a CREATE operation command
 * CREATE <DB> (int smth, string smthElse, ...)
 */
static Command* parse_create(const char* input) {
    Command* cmd = (Command*)malloc(sizeof(Command));
    if(cmd == NULL) {
        return parse_error(ER_OTHER, "Failed to allocate memory");
    }

    cmd->op                     = OP_CREATE;
    cmd->data.create.fields     = NULL;
    cmd->data.create.fieldCount = 0;

    // Step 1: Split input at parenthesis into two parts
    char* before_paren = NULL;
    char* inside_paren = NULL;

    int split_result = split_at_paren(input, &before_paren, &inside_paren);
    if(split_result == -1) {
        free(cmd);
        return parse_error(ER_SYNTAX_ERROR, "unmatched parenthesis");
    }
    if(split_result == -2) {
        free(cmd);
        return parse_error(ER_OTHER, "Failed to allocate memory");
    }
    if(split_result == -3) {
        free(cmd);
        return parse_error(ER_INVALID_CREATE_FORMAT, NULL);
    }

    // Step 2: Tokenize the first part - should only have database name
    char* db_name = NULL;

    int token_result = tokenize_single_arg(before_paren, &db_name);
    if(token_result == -1) {
        free(before_paren);
        free(inside_paren);
        free(cmd);
        return parse_error(ER_MISSING_ARGUMENT, "database name");
    }
    if(token_result == -2) {
        free(before_paren);
        free(inside_paren);
        free(cmd);
        return parse_error(ER_OTHER, "Failed to allocate memory");
    }

    // Step 3: Validate database name and store it
    if(!is_valid_identifier(db_name)) {
        char* invalid_name = db_name;
        free(before_paren);
        free(inside_paren);
        free(cmd);
        Command* err = parse_error(ER_INVALID_IDENTIFIER, invalid_name);
        free(invalid_name);
        return err;
    }

    strncpy(cmd->data.create.dbName, db_name, MAX_DB_NAME_LENGTH - 1);
    cmd->data.create.dbName[MAX_DB_NAME_LENGTH - 1] = '\0';
    free(db_name);
    free(before_paren);

    // Step 4: Tokenize fields from inside parentheses (in its own function)
    Field* fields     = NULL;
    int    fieldCount = 0;

    int fields_result = tokenize_fields(inside_paren, &fields, &fieldCount);
    free(inside_paren);

    if(fields_result == -1) {
        free(cmd);
        return parse_error(ER_OTHER, "Failed to allocate memory for fields");
    }
    if(fields_result == -2) {
        free(cmd);
        return parse_error(ER_MISSING_ARGUMENT, "field definitions");
    }
    if(fields_result == -3) {
        free(cmd);
        return parse_error(ER_MISSING_ARGUMENT, "field type");
    }
    if(fields_result == -4) {
        free(cmd);
        return parse_error(ER_INVALID_DATA_TYPE, "unknown type");
    }
    if(fields_result == -5) {
        free(cmd);
        return parse_error(ER_MISSING_ARGUMENT, "field name");
    }
    if(fields_result == -6) {
        free(cmd);
        return parse_error(ER_INVALID_IDENTIFIER, "invalid field name");
    }
    if(fields_result == -7) {
        free(cmd);
        return parse_error(ER_SYNTAX_ERROR, "expected ',' or end of fields");
    }

    cmd->data.create.fields     = fields;
    cmd->data.create.fieldCount = fieldCount;

    return cmd;
}

/**
 * Parse an ADD operation command
 * <DB> <KEY> (value1, value2, ...)
 * Key can be '*' for auto-generated key
 * Values can be: integers, doubles, booleans (true/false), or "strings"
 */
static Command* parse_add(const char* args) {
    Command* cmd = (Command*)malloc(sizeof(Command));
    if(cmd == NULL) {
        return parse_error(ER_OTHER, "Failed to allocate memory");
    }

    cmd->op                  = OP_ADD;
    cmd->data.add.values     = NULL;
    cmd->data.add.valueCount = 0;
    cmd->data.add.autoKey    = 0;
    cmd->data.add.key        = 0;

    // Step 1: Split input at parenthesis into two parts
    char* before_paren = NULL;
    char* inside_paren = NULL;

    int split_result = split_at_paren(args, &before_paren, &inside_paren);
    if(split_result == -1) {
        free(cmd);
        return parse_error(ER_SYNTAX_ERROR, "unmatched parenthesis");
    }
    if(split_result == -2) {
        free(cmd);
        return parse_error(ER_OTHER, "Failed to allocate memory");
    }
    if(split_result == -3) {
        free(cmd);
        return parse_error(ER_INVALID_ADD_FORMAT, NULL);
    }

    // Step 2: Tokenize the first part and check argument count
    char* db_name = NULL;
    char* key_str = NULL;

    int token_result = tokenize_args(before_paren, &db_name, &key_str);
    if(token_result == -1) {
        free(before_paren);
        free(inside_paren);
        free(cmd);
        return parse_error(ER_MISSING_ARGUMENT, "database name");
    }
    if(token_result == -2) {
        free(db_name);
        free(before_paren);
        free(inside_paren);
        free(cmd);
        return parse_error(ER_MISSING_ARGUMENT, "key");
    }
    if(token_result == -3) {
        free(db_name);
        free(key_str);
        free(before_paren);
        free(inside_paren);
        free(cmd);
        return parse_error(ER_OTHER, "Failed to allocate memory");
    }

    // Step 3: Validate and store the arguments
    // Validate database name
    if(!is_valid_identifier(db_name)) {
        char* invalid_name = db_name;
        free(key_str);
        free(before_paren);
        free(inside_paren);
        free(cmd);
        Command* err = parse_error(ER_INVALID_IDENTIFIER, invalid_name);
        free(invalid_name);
        return err;
    }

    strncpy(cmd->data.add.dbName, db_name, MAX_DB_NAME_LENGTH - 1);
    cmd->data.add.dbName[MAX_DB_NAME_LENGTH - 1] = '\0';
    free(db_name);

    // Validate and parse key
    if(strcmp(key_str, "*") == 0) {
        cmd->data.add.autoKey = 1;
        cmd->data.add.key     = 0;
    } else {
        const char* key_ptr = key_str;
        if(!parse_int(&key_ptr, &cmd->data.add.key) || *key_ptr != '\0') {
            free(key_str);
            free(before_paren);
            free(inside_paren);
            free(cmd);
            return parse_error(ER_SYNTAX_ERROR,
                               "key must be an integer or '*'");
        }
    }
    free(key_str);
    free(before_paren);

    // Step 4: Tokenize values from inside parentheses (in its own function)
    Data* values     = NULL;
    int   valueCount = 0;

    int values_result = tokenize_values(inside_paren, &values, &valueCount);
    free(inside_paren);

    if(values_result == -1) {
        free(cmd);
        return parse_error(ER_OTHER, "Failed to allocate memory for values");
    }
    if(values_result == -2) {
        free(cmd);
        return parse_error(ER_MISSING_ARGUMENT, "values");
    }
    if(values_result == -3) {
        free(cmd);
        return parse_error(ER_SYNTAX_ERROR, "unterminated string");
    }
    if(values_result == -4) {
        free(cmd);
        return parse_error(ER_SYNTAX_ERROR, "invalid double value");
    }
    if(values_result == -5) {
        free(cmd);
        return parse_error(ER_SYNTAX_ERROR, "invalid integer value");
    }
    if(values_result == -6) {
        free(cmd);
        return parse_error(ER_SYNTAX_ERROR, "invalid value");
    }
    if(values_result == -7) {
        free(cmd);
        return parse_error(ER_SYNTAX_ERROR, "expected ',' or ')'");
    }

    cmd->data.add.values     = values;
    cmd->data.add.valueCount = valueCount;

    return cmd;
}

/**
 * Parse an UP (update) operation command
 * UP <DB> <KEY> (<VALUES>)
 */
static Command* parse_up(const char* args) {
    Command* cmd = (Command*)malloc(sizeof(Command));
    if(cmd == NULL) {
        return parse_error(ER_OTHER, "Failed to allocate memory");
    }

    cmd->op                      = OP_UP;
    cmd->data.update.values      = NULL;
    cmd->data.update.valueCount  = 0;
    cmd->data.update.ignoreFlags = NULL;
    cmd->data.update.key         = 0;

    // Step 1: Split input at parenthesis into two parts
    char* before_paren = NULL;
    char* inside_paren = NULL;

    int split_result = split_at_paren(args, &before_paren, &inside_paren);
    if(split_result == -1) {
        free(cmd);
        return parse_error(ER_SYNTAX_ERROR, "unmatched parenthesis");
    }
    if(split_result == -2) {
        free(cmd);
        return parse_error(ER_OTHER, "Failed to allocate memory");
    }
    if(split_result == -3) {
        free(cmd);
        return parse_error(ER_INVALID_UPDATE_FORMAT, NULL);
    }

    // Step 2: Tokenize the first part and check argument count
    char* db_name = NULL;
    char* key_str = NULL;

    int token_result = tokenize_args(before_paren, &db_name, &key_str);
    if(token_result == -1) {
        free(before_paren);
        free(inside_paren);
        free(cmd);
        return parse_error(ER_MISSING_ARGUMENT, "database name");
    }
    if(token_result == -2) {
        free(db_name);
        free(before_paren);
        free(inside_paren);
        free(cmd);
        return parse_error(ER_MISSING_ARGUMENT, "key");
    }
    if(token_result == -3) {
        free(db_name);
        free(key_str);
        free(before_paren);
        free(inside_paren);
        free(cmd);
        return parse_error(ER_OTHER, "Failed to allocate memory");
    }

    // Step 3: Validate and store the arguments
    // Validate database name
    if(!is_valid_identifier(db_name)) {
        char* invalid_name = db_name;
        free(key_str);
        free(before_paren);
        free(inside_paren);
        free(cmd);
        Command* err = parse_error(ER_INVALID_IDENTIFIER, invalid_name);
        free(invalid_name);
        return err;
    }

    strncpy(cmd->data.update.dbName, db_name, MAX_DB_NAME_LENGTH - 1);
    cmd->data.update.dbName[MAX_DB_NAME_LENGTH - 1] = '\0';
    free(db_name);

    // Validate and parse key (must be an integer for UP, no auto-key)
    const char* key_ptr = key_str;
    if(!parse_int(&key_ptr, &cmd->data.update.key) || *key_ptr != '\0') {
        free(key_str);
        free(before_paren);
        free(inside_paren);
        free(cmd);
        return parse_error(ER_SYNTAX_ERROR, "key must be an integer");
    }
    free(key_str);
    free(before_paren);

    // Step 4: Tokenize values from inside parentheses (with ignore flag
    // support)
    Data* values      = NULL;
    int*  ignoreFlags = NULL;
    int   valueCount  = 0;

    int values_result = tokenize_update_values(inside_paren,
                                               &values,
                                               &ignoreFlags,
                                               &valueCount);
    free(inside_paren);

    if(values_result == -1) {
        free(cmd);
        return parse_error(ER_OTHER, "Failed to allocate memory for values");
    }
    if(values_result == -2) {
        free(cmd);
        return parse_error(ER_MISSING_ARGUMENT, "values");
    }
    if(values_result == -3) {
        free(cmd);
        return parse_error(ER_SYNTAX_ERROR, "unterminated string");
    }
    if(values_result == -4) {
        free(cmd);
        return parse_error(ER_SYNTAX_ERROR, "invalid double value");
    }
    if(values_result == -5) {
        free(cmd);
        return parse_error(ER_SYNTAX_ERROR, "invalid integer value");
    }
    if(values_result == -6) {
        free(cmd);
        return parse_error(ER_SYNTAX_ERROR, "invalid value");
    }
    if(values_result == -7) {
        free(cmd);
        return parse_error(ER_SYNTAX_ERROR, "expected ',' or ')'");
    }

    cmd->data.update.values      = values;
    cmd->data.update.ignoreFlags = ignoreFlags;
    cmd->data.update.valueCount  = valueCount;

    return cmd;
}

/**
 * Parse a GET operation command
 * GET <DB> <KEY> (<FIELDS>)
 * Fields are optional - if omitted or empty parens, returns all fields
 */
static Command* parse_get(const char* args) {
    Command* cmd = (Command*)malloc(sizeof(Command));
    if(cmd == NULL) {
        return parse_error(ER_OTHER, "Failed to allocate memory");
    }

    cmd->op                  = OP_GET;
    cmd->data.get.fields     = NULL;
    cmd->data.get.fieldCount = 0;
    cmd->data.get.key        = 0;

    // Check if there are parentheses (optional fields)
    const char* paren = strchr(args, '(');

    if(paren != NULL) {
        // Has field specification - split at parenthesis
        char* before_paren = NULL;
        char* inside_paren = NULL;

        int split_result = split_at_paren(args, &before_paren, &inside_paren);
        if(split_result == -1) {
            free(cmd);
            return parse_error(ER_SYNTAX_ERROR, "unmatched parenthesis");
        }
        if(split_result == -2) {
            free(cmd);
            return parse_error(ER_OTHER, "Failed to allocate memory");
        }

        // Tokenize db_name and key from before parenthesis
        char* db_name = NULL;
        char* key_str = NULL;

        int token_result = tokenize_args(before_paren, &db_name, &key_str);
        if(token_result == -1) {
            free(before_paren);
            free(inside_paren);
            free(cmd);
            return parse_error(ER_MISSING_ARGUMENT, "database name");
        }
        if(token_result == -2) {
            free(db_name);
            free(before_paren);
            free(inside_paren);
            free(cmd);
            return parse_error(ER_MISSING_ARGUMENT, "key");
        }
        if(token_result == -3) {
            free(db_name);
            free(key_str);
            free(before_paren);
            free(inside_paren);
            free(cmd);
            return parse_error(ER_OTHER, "Failed to allocate memory");
        }

        // Validate database name
        if(!is_valid_identifier(db_name)) {
            char* invalid_name = db_name;
            free(key_str);
            free(before_paren);
            free(inside_paren);
            free(cmd);
            Command* err = parse_error(ER_INVALID_IDENTIFIER, invalid_name);
            free(invalid_name);
            return err;
        }

        strncpy(cmd->data.get.dbName, db_name, MAX_DB_NAME_LENGTH - 1);
        cmd->data.get.dbName[MAX_DB_NAME_LENGTH - 1] = '\0';
        free(db_name);

        // Validate and parse key
        const char* key_ptr = key_str;
        if(!parse_int(&key_ptr, &cmd->data.get.key) || *key_ptr != '\0') {
            free(key_str);
            free(before_paren);
            free(inside_paren);
            free(cmd);
            return parse_error(ER_SYNTAX_ERROR, "key must be an integer");
        }
        free(key_str);
        free(before_paren);

        // Parse field names from inside parentheses (if not empty)
        const char* field_ptr = skip_whitespace(inside_paren);
        if(*field_ptr != '\0') {
            // Count fields
            int         field_count = 1;
            const char* p           = field_ptr;
            while(*p) {
                if(*p == ',') field_count++;
                p++;
            }

            // Allocate field array
            cmd->data.get.fields = (char**)malloc(sizeof(char*) * field_count);
            if(cmd->data.get.fields == NULL) {
                free(inside_paren);
                free(cmd);
                return parse_error(ER_OTHER, "Failed to allocate memory");
            }

            // Parse each field name
            p = field_ptr;
            for(int i = 0; i < field_count; i++) {
                p                 = skip_whitespace(p);
                const char* start = p;
                while(*p && *p != ',' && !isspace((unsigned char)*p)) {
                    p++;
                }

                size_t len = p - start;
                if(len == 0) {
                    // Free already allocated fields
                    for(int j = 0; j < i; j++) {
                        free(cmd->data.get.fields[j]);
                    }
                    free(cmd->data.get.fields);
                    free(inside_paren);
                    free(cmd);
                    return parse_error(ER_MISSING_ARGUMENT, "field name");
                }

                cmd->data.get.fields[i] = (char*)malloc(len + 1);
                if(cmd->data.get.fields[i] == NULL) {
                    for(int j = 0; j < i; j++) {
                        free(cmd->data.get.fields[j]);
                    }
                    free(cmd->data.get.fields);
                    free(inside_paren);
                    free(cmd);
                    return parse_error(ER_OTHER, "Failed to allocate memory");
                }
                strncpy(cmd->data.get.fields[i], start, len);
                cmd->data.get.fields[i][len] = '\0';

                // Validate field name
                if(!is_valid_identifier(cmd->data.get.fields[i])) {
                    for(int j = 0; j <= i; j++) {
                        free(cmd->data.get.fields[j]);
                    }
                    free(cmd->data.get.fields);
                    free(inside_paren);
                    free(cmd);
                    return parse_error(ER_INVALID_IDENTIFIER,
                                       "invalid field name");
                }

                p = skip_whitespace(p);
                if(*p == ',') p++;
            }

            cmd->data.get.fieldCount = field_count;
        }

        free(inside_paren);
    } else {
        // No opening parenthesis - check for closing parenthesis (syntax error)
        if(strchr(args, ')') != NULL) {
            free(cmd);
            return parse_error(ER_SYNTAX_ERROR, "unmatched parenthesis");
        }

        // No parentheses - just parse db_name and key
        char* db_name = NULL;
        char* key_str = NULL;

        int token_result = tokenize_args(args, &db_name, &key_str);
        if(token_result == -1) {
            free(cmd);
            return parse_error(ER_MISSING_ARGUMENT, "database name");
        }
        if(token_result == -2) {
            free(db_name);
            free(cmd);
            return parse_error(ER_MISSING_ARGUMENT, "key");
        }
        if(token_result == -3) {
            free(db_name);
            free(key_str);
            free(cmd);
            return parse_error(ER_OTHER, "Failed to allocate memory");
        }

        // Validate database name
        if(!is_valid_identifier(db_name)) {
            char* invalid_name = db_name;
            free(key_str);
            free(cmd);
            Command* err = parse_error(ER_INVALID_IDENTIFIER, invalid_name);
            free(invalid_name);
            return err;
        }

        strncpy(cmd->data.get.dbName, db_name, MAX_DB_NAME_LENGTH - 1);
        cmd->data.get.dbName[MAX_DB_NAME_LENGTH - 1] = '\0';
        free(db_name);

        // Validate and parse key
        const char* key_ptr = key_str;
        if(!parse_int(&key_ptr, &cmd->data.get.key) || *key_ptr != '\0') {
            free(key_str);
            free(cmd);
            return parse_error(ER_SYNTAX_ERROR, "key must be an integer");
        }
        free(key_str);

        // Check for extra arguments after key (should be none)
        const char* after_key = skip_whitespace(args);
        // Skip db_name
        while(*after_key && !isspace((unsigned char)*after_key)) after_key++;
        after_key = skip_whitespace(after_key);
        // Skip key
        while(*after_key && !isspace((unsigned char)*after_key)) after_key++;
        after_key = skip_whitespace(after_key);

        if(*after_key != '\0') {
            free(cmd);
            return parse_error(ER_SYNTAX_ERROR,
                               "unexpected arguments after key");
        }

        // No fields specified - will return all fields
        cmd->data.get.fields     = NULL;
        cmd->data.get.fieldCount = 0;
    }

    return cmd;
}
/**
 * Parse a DEL (delete) operation command
 * DEL <DB> <KEY>
 */
static Command* parse_del(const char* input) {
    Command* cmd = (Command*)malloc(sizeof(Command));
    if(cmd == NULL) {
        return parse_error(ER_OTHER, "Failed to allocate memory");
    }

    cmd->op = OP_DEL;
    // Stub - actual parsing logic would go here

    return cmd;
}

/**
 * Parse a GET_ALL operation command
 * GET_ALL <DB> (<FIELDS>)
 */
static Command* parse_get_all(const char* input) {
    Command* cmd = (Command*)malloc(sizeof(Command));
    if(cmd == NULL) {
        return parse_error(ER_OTHER, "Failed to allocate memory");
    }

    cmd->op = OP_GET_ALL;
    // Stub - actual parsing logic would go here

    return cmd;
}

/**
 * Parse a SEARCH operation command
 * SEARCH <DB> <FIELD> <VALUE> (<FIELDS>)
 */
static Command* parse_search(const char* input) {
    Command* cmd = (Command*)malloc(sizeof(Command));
    if(cmd == NULL) {
        return parse_error(ER_OTHER, "Failed to allocate memory");
    }

    cmd->op = OP_SEARCH;
    // Stub - actual parsing logic would go here

    return cmd;
}

/**
 * Parse a COUNT operation command
 * COUNT <DB>
 */
static Command* parse_count(const char* input) {
    Command* cmd = (Command*)malloc(sizeof(Command));
    if(cmd == NULL) {
        return parse_error(ER_OTHER, "Failed to allocate memory");
    }

    cmd->op = OP_COUNT;
    // Stub - actual parsing logic would go here

    return cmd;
}

/**
 * Parse a CREATE_INDEX operation command
 * CREATE_INDEX <DB> <FIELD>
 */
static Command* parse_create_index(const char* input) {
    Command* cmd = (Command*)malloc(sizeof(Command));
    if(cmd == NULL) {
        return parse_error(ER_OTHER, "Failed to allocate memory");
    }

    cmd->op = OP_CREATE_INDEX;
    // Stub - actual parsing logic would go here

    return cmd;
}

/**
 * Determine which operation the command string contains
 */
static Operation determine_operation(const char* input) {
    // Skip any leading whitespace
    while(*input && (*input == ' ' || *input == '\t')) input++;

    size_t len_input = strlen(input);

    // Check each command string
    for(int i = 0; i < COMMAND_LENGTH; i++) {
        size_t len_command = strlen(COMMAND_STRINGS[i]);
        size_t len         = len_input > len_command ? len_command : len_input;

        if(strncasecmp(input, COMMAND_STRINGS[i], len) == 0) {
            return (Operation)i;
        }
    }

    return OP_ERROR;
}

static Command* parse_error(ParseError errorCode, const char* detail) {
    Command* cmd = (Command*)malloc(sizeof(Command));
    if(cmd == NULL) {
        return NULL;
    }

    cmd->op = OP_ERROR;  // Set operation to error

    // Format error message with detail if provided
    if(detail != NULL && errorCode != ER_INVALID_COMMAND) {
        // Handle error codes that require formatting with additional details
        switch(errorCode) {
            case ER_MISSING_ARGUMENT:
            case ER_UNEXPECTED_ARGUMENT:
            case ER_INVALID_IDENTIFIER:
            case ER_INVALID_DATA_TYPE:
            case ER_SYNTAX_ERROR:
                snprintf(cmd->data.error,
                         MAX_ERROR_LENGTH,
                         ERROR_MESSAGES[errorCode],
                         detail);
                break;
            default:
                // For errors without formatting, just copy the error message
                strncpy(cmd->data.error,
                        ERROR_MESSAGES[errorCode],
                        MAX_ERROR_LENGTH - 1);
                cmd->data.error[MAX_ERROR_LENGTH - 1] =
                    '\0';  // Ensure null termination
                break;
        }
    } else {
        // No detail provided or it's the invalid command error, just use the
        // error message directly
        strncpy(cmd->data.error,
                ERROR_MESSAGES[errorCode],
                MAX_ERROR_LENGTH - 1);
        cmd->data.error[MAX_ERROR_LENGTH - 1] =
            '\0';  // Ensure null termination
    }

    return cmd;
}

/**
 * Split input string at the opening parenthesis
 * Returns 0 on success, -1 for unmatched parens, -2 for memory error, -3 for
 * missing paren Caller must free before_paren and inside_paren
 */
static int split_at_paren(const char* input,
                          char**      before_paren,
                          char**      inside_paren) {
    const char* ptr = skip_whitespace(input);

    // Find opening parenthesis
    const char* open_paren = strchr(ptr, '(');
    if(open_paren == NULL) {
        return -3;  // No opening paren found
    }

    // Find matching closing parenthesis
    const char* p           = open_paren + 1;
    int         paren_depth = 1;
    int         in_string   = 0;

    while(*p && paren_depth > 0) {
        if(*p == '"' && (p == open_paren + 1 || *(p - 1) != '\\')) {
            in_string = !in_string;
        } else if(!in_string) {
            if(*p == '(') {
                paren_depth++;
            } else if(*p == ')') {
                paren_depth--;
            }
        }
        p++;
    }

    if(paren_depth != 0) {
        return -1;  // Unmatched parenthesis
    }

    const char* close_paren = p - 1;

    // Allocate and copy the part before the parenthesis
    size_t before_len = open_paren - ptr;
    *before_paren     = (char*)malloc(before_len + 1);
    if(*before_paren == NULL) {
        return -2;
    }
    strncpy(*before_paren, ptr, before_len);
    (*before_paren)[before_len] = '\0';

    // Allocate and copy the part inside the parentheses (without parens)
    size_t inside_len = close_paren - (open_paren + 1);
    *inside_paren     = (char*)malloc(inside_len + 1);
    if(*inside_paren == NULL) {
        free(*before_paren);
        *before_paren = NULL;
        return -2;
    }
    strncpy(*inside_paren, open_paren + 1, inside_len);
    (*inside_paren)[inside_len] = '\0';

    return 0;
}

/**
 * Tokenize arguments string into dbName and key
 * Returns 0 on success, -1 for missing dbName, -2 for missing key, -3 for
 * memory error Caller must free db_name and key_str
 */
static int tokenize_args(const char* args_str, char** db_name, char** key_str) {
    const char* ptr = skip_whitespace(args_str);

    // Check if we have any content
    if(*ptr == '\0') {
        return -1;  // Missing database name
    }

    // Extract database name (first token)
    const char* start = ptr;
    while(*ptr && !isspace((unsigned char)*ptr)) {
        ptr++;
    }

    size_t db_len = ptr - start;
    if(db_len == 0) {
        return -1;  // Missing database name
    }

    *db_name = (char*)malloc(db_len + 1);
    if(*db_name == NULL) {
        return -3;
    }
    strncpy(*db_name, start, db_len);
    (*db_name)[db_len] = '\0';

    // Skip whitespace to find key
    ptr = skip_whitespace(ptr);

    if(*ptr == '\0') {
        return -2;  // Missing key
    }

    // Extract key (second token)
    start = ptr;
    while(*ptr && !isspace((unsigned char)*ptr)) {
        ptr++;
    }

    size_t key_len = ptr - start;
    if(key_len == 0) {
        return -2;  // Missing key
    }

    *key_str = (char*)malloc(key_len + 1);
    if(*key_str == NULL) {
        return -3;
    }
    strncpy(*key_str, start, key_len);
    (*key_str)[key_len] = '\0';

    return 0;
}

/**
 * Free a values array and all allocated strings within it
 */
static void free_values_array(Data* values, int count) {
    if(values == NULL) return;
    for(int v = 0; v < count; v++) {
        if(values[v].size > 0 && values[v].value.s != NULL) {
            free((void*)values[v].value.s);
        }
    }
    free(values);
}

/**
 * Tokenize a single argument from the args string
 * Returns 0 on success, -1 for missing arg, -2 for memory error
 * Caller must free arg
 */
static int tokenize_single_arg(const char* args_str, char** arg) {
    const char* ptr = skip_whitespace(args_str);

    // Check if we have any content
    if(*ptr == '\0') {
        return -1;  // Missing argument
    }

    // Extract the argument (first token)
    const char* start = ptr;
    while(*ptr && !isspace((unsigned char)*ptr)) {
        ptr++;
    }

    size_t arg_len = ptr - start;
    if(arg_len == 0) {
        return -1;  // Missing argument
    }

    *arg = (char*)malloc(arg_len + 1);
    if(*arg == NULL) {
        return -2;
    }
    strncpy(*arg, start, arg_len);
    (*arg)[arg_len] = '\0';

    return 0;
}

/**
 * Free a fields array (no dynamic strings inside Field struct currently)
 */
static void free_fields_array(Field* fields, int count) {
    (void)count;  // Currently unused, but kept for consistency
    if(fields == NULL) return;
    free(fields);
}

/**
 * Parse a single field definition (type name) from the input string
 * Returns 0 on success, negative on error:
 *   -3 for missing field type
 *   -4 for invalid data type
 *   -5 for missing field name
 *   -6 for invalid field name
 * Advances ptr past the parsed field
 */
static int parse_single_field(const char** ptr, Field* out_field) {
    const char* p = skip_whitespace(*ptr);

    // Parse field type
    char typeStr[TYPE_STR_LENGTH];
    int  i = 0;
    while(*p && !isspace((unsigned char)*p) && *p != ',' && *p != ')' &&
          i < TYPE_STR_LENGTH - 1) {
        typeStr[i++] = *p++;
    }
    typeStr[i] = '\0';

    if(typeStr[0] == '\0') {
        return -3;  // Missing field type
    }

    FieldType fieldType;
    if(parse_field_type(typeStr, &fieldType) != 0) {
        return -4;  // Invalid data type
    }

    p = skip_whitespace(p);

    // Parse field name
    char fieldName[MAX_FIELD_NAME_LENGTH];
    i = 0;
    while(*p && !isspace((unsigned char)*p) && *p != ',' && *p != ')' &&
          i < MAX_FIELD_NAME_LENGTH - 1) {
        fieldName[i++] = *p++;
    }
    fieldName[i] = '\0';

    if(fieldName[0] == '\0') {
        return -5;  // Missing field name
    }

    if(!is_valid_identifier(fieldName)) {
        return -6;  // Invalid field name
    }

    // Store the field
    out_field->type = fieldType;
    // fieldName is already null-terminated, safe to copy
    size_t len = strlen(fieldName);
    memcpy(out_field->name, fieldName, len + 1);  // +1 for null terminator

    *ptr = p;
    return 0;
}

/**
 * Tokenize fields from inside parentheses string and validate each field
 * Returns 0 on success, negative on error:
 *   -1 for memory allocation failure
 *   -2 for empty fields
 *   -3 for missing field type
 *   -4 for invalid data type
 *   -5 for missing field name
 *   -6 for invalid field name
 *   -7 for syntax error (expected ',' or end)
 * Caller must free fields array on success
 */
static int tokenize_fields(const char* fields_str,
                           Field**     fields,
                           int*        field_count) {
    // Count the number of fields manually since we already stripped the parens
    const char* p          = fields_str;
    int         count      = 0;
    int         hasContent = 0;

    while(*p) {
        if(*p == ',') {
            count++;
        } else if(!isspace((unsigned char)*p)) {
            hasContent = 1;
        }
        p++;
    }

    if(hasContent) {
        count++;  // Add 1 for the last item
    }

    if(count == 0) {
        return -2;  // No fields provided
    }

    // Allocate fields array
    *fields = (Field*)malloc(sizeof(Field) * count);
    if(*fields == NULL) {
        return -1;
    }

    // Parse each field
    const char* ptr        = fields_str;
    int         fieldIndex = 0;

    while(fieldIndex < count) {
        ptr = skip_whitespace(ptr);

        int result = parse_single_field(&ptr, &(*fields)[fieldIndex]);
        if(result != 0) {
            free_fields_array(*fields, fieldIndex);
            *fields = NULL;
            return result;
        }

        fieldIndex++;

        // Skip whitespace and find comma or end
        ptr = skip_whitespace(ptr);
        if(*ptr == ',') {
            ptr++;  // Skip comma
        } else if(*ptr == '\0') {
            break;  // End of fields
        } else {
            free_fields_array(*fields, fieldIndex);
            *fields = NULL;
            return -7;  // Expected ',' or end
        }
    }

    *field_count = count;
    return 0;
}

/**
 * Parse a single value from the input string
 * Returns 0 on success, negative on error:
 *   -3 for unterminated string
 *   -4 for invalid double
 *   -5 for invalid integer
 *   -6 for invalid value
 * Advances ptr past the parsed value
 */
static int parse_single_value(const char** ptr, Data* out_value) {
    const char* p = skip_whitespace(*ptr);

    out_value->size    = 0;
    out_value->value.i = 0;

    if(*p == '"') {
        // Parse string value
        char* str = parse_string(&p);
        if(str == NULL) {
            return -3;  // Unterminated string
        }
        out_value->value.s = str;
        out_value->size    = strlen(str);
    } else if(strncasecmp(p, "true", 4) == 0 &&
              (p[4] == ',' || p[4] == ')' || isspace((unsigned char)p[4]) ||
               p[4] == '\0')) {
        // Parse boolean true
        out_value->value.b  = 1;
        out_value->size     = 0;
        p                  += 4;
    } else if(strncasecmp(p, "false", 5) == 0 &&
              (p[5] == ',' || p[5] == ')' || isspace((unsigned char)p[5]) ||
               p[5] == '\0')) {
        // Parse boolean false
        out_value->value.b  = 0;
        out_value->size     = 0;
        p                  += 5;
    } else if(*p == '-' || *p == '+' || isdigit((unsigned char)*p)) {
        // Parse number (int or double)
        int isDouble = 0;

        // Check if it's a double by looking for '.' or 'e'/'E'
        const char* scan = p;
        if(*scan == '-' || *scan == '+') scan++;
        while(*scan && (isdigit((unsigned char)*scan) || *scan == '.' ||
                        *scan == 'e' || *scan == 'E')) {
            if(*scan == '.' || *scan == 'e' || *scan == 'E') {
                isDouble = 1;
            }
            scan++;
        }

        if(isDouble) {
            double d;
            if(!parse_double(&p, &d)) {
                return -4;  // Invalid double
            }
            out_value->value.d = d;
            out_value->size    = -1;  // Use -1 to indicate double
        } else {
            int intVal;
            if(!parse_int(&p, &intVal)) {
                return -5;  // Invalid integer
            }
            out_value->value.i = intVal;
            out_value->size    = -2;  // Use -2 to indicate int
        }
    } else {
        return -6;  // Invalid value
    }

    *ptr = p;
    return 0;
}

/**
 * Tokenize values from inside parentheses string and validate each value
 * Returns 0 on success, negative on error:
 *   -1 for memory allocation failure
 *   -2 for empty values
 *   -3 for unterminated string
 *   -4 for invalid double
 *   -5 for invalid integer
 *   -6 for invalid value
 *   -7 for syntax error (expected ',' or end)
 * Caller must free values array on success
 */
static int tokenize_values(const char* values_str,
                           Data**      values,
                           int*        value_count) {
    // Count the number of values manually since we already stripped the parens
    const char* p          = values_str;
    int         count      = 0;
    int         hasContent = 0;
    int         inString   = 0;

    while(*p) {
        if(*p == '"') {
            inString   = !inString;
            hasContent = 1;
        } else if(!inString) {
            if(*p == ',') {
                count++;
            } else if(!isspace((unsigned char)*p)) {
                hasContent = 1;
            }
        } else {
            hasContent = 1;
        }
        p++;
    }

    if(hasContent) {
        count++;  // Add 1 for the last item
    }

    if(count == 0) {
        return -2;  // No values provided
    }

    // Allocate values array
    *values = (Data*)malloc(sizeof(Data) * count);
    if(*values == NULL) {
        return -1;
    }

    // Initialize values
    for(int v = 0; v < count; v++) {
        (*values)[v].size    = 0;
        (*values)[v].value.i = 0;
    }

    // Parse each value
    const char* ptr        = values_str;
    int         valueIndex = 0;

    while(valueIndex < count) {
        ptr = skip_whitespace(ptr);

        int result = parse_single_value(&ptr, &(*values)[valueIndex]);
        if(result != 0) {
            free_values_array(*values, valueIndex);
            *values = NULL;
            return result;
        }

        valueIndex++;

        // Skip whitespace and find comma or end
        ptr = skip_whitespace(ptr);
        if(*ptr == ',') {
            ptr++;  // Skip comma
        } else if(*ptr == '\0') {
            break;  // End of values
        } else {
            free_values_array(*values, valueIndex);
            *values = NULL;
            return -7;  // Expected ',' or end
        }
    }

    *value_count = count;
    return 0;
}

/**
 * Parse a single update value from the input string (with ignore flag support)
 * Returns 0 on success, negative on error:
 *   -3 for unterminated string
 *   -4 for invalid double
 *   -5 for invalid integer
 *   -6 for invalid value
 * Sets is_ignored to 1 if the value is '_' (ignore marker)
 * Advances ptr past the parsed value
 */
static int parse_update_value(const char** ptr,
                              Data*        out_value,
                              int*         is_ignored) {
    const char* p = skip_whitespace(*ptr);

    out_value->size    = 0;
    out_value->value.i = 0;
    *is_ignored        = 0;

    // Check for ignore marker '_'
    if(*p == '_' && (p[1] == ',' || p[1] == ')' ||
                     isspace((unsigned char)p[1]) || p[1] == '\0')) {
        *is_ignored = 1;
        *ptr        = p + 1;
        return 0;
    }

    // Otherwise, parse as a regular value
    int result = parse_single_value(&p, out_value);
    *ptr       = p;
    return result;
}

/**
 * Tokenize update values from inside parentheses string with ignore flag
 * support Returns 0 on success, negative on error: -1 for memory allocation
 * failure -2 for empty values -3 for unterminated string -4 for invalid double
 *   -5 for invalid integer
 *   -6 for invalid value
 *   -7 for syntax error (expected ',' or end)
 * Caller must free values array and ignore_flags array on success
 */
static int tokenize_update_values(const char* values_str,
                                  Data**      values,
                                  int**       ignore_flags,
                                  int*        value_count) {
    // Count the number of values manually since we already stripped the parens
    const char* p          = values_str;
    int         count      = 0;
    int         hasContent = 0;
    int         inString   = 0;

    while(*p) {
        if(*p == '"') {
            inString   = !inString;
            hasContent = 1;
        } else if(!inString) {
            if(*p == ',') {
                count++;
            } else if(!isspace((unsigned char)*p)) {
                hasContent = 1;
            }
        } else {
            hasContent = 1;
        }
        p++;
    }

    if(hasContent) {
        count++;  // Add 1 for the last item
    }

    if(count == 0) {
        return -2;  // No values provided
    }

    // Allocate values array
    *values = (Data*)malloc(sizeof(Data) * count);
    if(*values == NULL) {
        return -1;
    }

    // Allocate ignore flags array
    *ignore_flags = (int*)malloc(sizeof(int) * count);
    if(*ignore_flags == NULL) {
        free(*values);
        *values = NULL;
        return -1;
    }

    // Initialize values and flags
    for(int v = 0; v < count; v++) {
        (*values)[v].size    = 0;
        (*values)[v].value.i = 0;
        (*ignore_flags)[v]   = 0;
    }

    // Parse each value
    const char* ptr        = values_str;
    int         valueIndex = 0;

    while(valueIndex < count) {
        ptr = skip_whitespace(ptr);

        int result = parse_update_value(&ptr,
                                        &(*values)[valueIndex],
                                        &(*ignore_flags)[valueIndex]);
        if(result != 0) {
            free_values_array(*values, valueIndex);
            *values = NULL;
            free(*ignore_flags);
            *ignore_flags = NULL;
            return result;
        }

        valueIndex++;

        // Skip whitespace and find comma or end
        ptr = skip_whitespace(ptr);
        if(*ptr == ',') {
            ptr++;  // Skip comma
        } else if(*ptr == '\0') {
            break;  // End of values
        } else {
            free_values_array(*values, valueIndex);
            *values = NULL;
            free(*ignore_flags);
            *ignore_flags = NULL;
            return -7;  // Expected ',' or end
        }
    }

    *value_count = count;
    return 0;
}

/**
 * Helper function to parse a field type string into FieldType enum
 * Returns -1 if the type is invalid
 */
static int parse_field_type(const char* typeStr, FieldType* outType) {
    if(strcasecmp(typeStr, "int") == 0) {
        *outType = TYPE_INT;
        return 0;
    } else if(strcasecmp(typeStr, "double") == 0) {
        *outType = TYPE_DOUBLE;
        return 0;
    } else if(strcasecmp(typeStr, "bool") == 0) {
        *outType = TYPE_BOOL;
        return 0;
    } else if(strcasecmp(typeStr, "string") == 0) {
        *outType = TYPE_STRING;
        return 0;
    }
    return -1;
}

/**
 * Helper function to check if a string is a valid identifier
 * (starts with letter or underscore, followed by alphanumeric or underscore)
 */
static int is_valid_identifier(const char* str) {
    if(str == NULL || *str == '\0') {
        return 0;
    }
    if(!isalpha((unsigned char)*str) && *str != '_') {
        return 0;
    }
    for(const char* p = str + 1; *p != '\0'; p++) {
        if(!isalnum((unsigned char)*p) && *p != '_') {
            return 0;
        }
    }
    return 1;
}

/**
 * Helper function to skip whitespace
 */
static const char* skip_whitespace(const char* str) {
    while(*str && (*str == ' ' || *str == '\t')) {
        str++;
    }
    return str;
}

/**
 * Helper function to parse an integer from the input
 * Returns 1 on success, 0 on failure
 * Advances the pointer past the parsed integer
 */
static int parse_int(const char** ptr, int* outValue) {
    const char* p   = *ptr;
    int         neg = 0;

    if(*p == '-') {
        neg = 1;
        p++;
    } else if(*p == '+') {
        p++;
    }

    if(!isdigit((unsigned char)*p)) {
        return 0;
    }

    int value = 0;
    while(isdigit((unsigned char)*p)) {
        value = value * 10 + (*p - '0');
        p++;
    }

    *outValue = neg ? -value : value;
    *ptr      = p;
    return 1;
}

/**
 * Helper function to parse a double from the input
 * Returns 1 on success, 0 on failure
 * Advances the pointer past the parsed double
 */
static int parse_double(const char** ptr, double* outValue) {
    char*  endptr;
    double value = strtod(*ptr, &endptr);

    if(endptr == *ptr) {
        return 0;
    }

    *outValue = value;
    *ptr      = endptr;
    return 1;
}

/**
 * Helper function to parse a quoted string from the input
 * Returns a newly allocated string on success, NULL on failure
 * Advances the pointer past the closing quote
 */
static char* parse_string(const char** ptr) {
    const char* p = *ptr;

    if(*p != '"') {
        return NULL;
    }
    p++;  // Skip opening quote

    // Find the length of the string (handling escape sequences)
    const char* start = p;
    int         len   = 0;
    while(*p && *p != '"') {
        if(*p == '\\' && *(p + 1)) {
            p += 2;  // Skip escape sequence
            len++;
        } else {
            p++;
            len++;
        }
    }

    if(*p != '"') {
        return NULL;  // Unterminated string
    }

    // Allocate and copy the string
    char* str = (char*)malloc(len + 1);
    if(str == NULL) {
        return NULL;
    }

    p     = start;
    int i = 0;
    while(*p && *p != '"') {
        if(*p == '\\' && *(p + 1)) {
            p++;  // Skip backslash
            switch(*p) {
                case 'n':  str[i++] = '\n'; break;
                case 't':  str[i++] = '\t'; break;
                case 'r':  str[i++] = '\r'; break;
                case '\\': str[i++] = '\\'; break;
                case '"':  str[i++] = '"'; break;
                default:   str[i++] = *p; break;
            }
            p++;
        } else {
            str[i++] = *p++;
        }
    }
    str[i] = '\0';

    *ptr = p + 1;  // Skip closing quote
    return str;
}
