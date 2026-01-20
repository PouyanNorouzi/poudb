#include "db/parser.h"

#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#define COMMAND_LENGTH 9
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

static int parse_field_type(const char* typeStr, FieldType* outType);
static int is_valid_identifier(const char* str);
static const char* skip_whitespace(const char* str);

/**
 * Parse a command string into a Command structure
 */
Command* parse_command(const char* input) {
    if(input == NULL || *input == '\0') {
        return parse_error(ER_INVALID_COMMAND, NULL);
    }

    // Skip leading whitespace
    while(*input && (*input == ' ' || *input == '\t')) input++;

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

    cmd->op = OP_CREATE;
    cmd->data.create.fields = NULL;
    cmd->data.create.fieldCount = 0;

    // Skip leading whitespace
    const char* ptr = skip_whitespace(input);

    // Parse database name
    if(*ptr == '\0' || *ptr == '(') {
        free(cmd);
        return parse_error(ER_MISSING_ARGUMENT, "database name");
    }

    // Extract database name
    char dbName[MAX_DB_NAME_LENGTH];
    int i = 0;
    while(*ptr && *ptr != ' ' && *ptr != '\t' && *ptr != '(' && i < MAX_DB_NAME_LENGTH - 1) {
        dbName[i++] = *ptr++;
    }
    dbName[i] = '\0';

    if(!is_valid_identifier(dbName)) {
        free(cmd);
        return parse_error(ER_INVALID_IDENTIFIER, dbName);
    }

    strncpy(cmd->data.create.dbName, dbName, MAX_DB_NAME_LENGTH - 1);
    cmd->data.create.dbName[MAX_DB_NAME_LENGTH - 1] = '\0';

    // Skip whitespace and find opening parenthesis
    ptr = skip_whitespace(ptr);
    if(*ptr != '(') {
        free(cmd);
        return parse_error(ER_INVALID_CREATE_FORMAT, NULL);
    }
    ptr++; // Skip '('

    // Count fields first by counting commas + 1 (if there's content)
    const char* countPtr = ptr;
    int fieldCount = 0;
    int parenDepth = 1;
    int hasContent = 0;

    while(*countPtr && parenDepth > 0) {
        if(*countPtr == '(') {
            parenDepth++;
        } else if(*countPtr == ')') {
            parenDepth--;
        } else if(*countPtr == ',' && parenDepth == 1) {
            fieldCount++;
        } else if(!isspace((unsigned char)*countPtr)) {
            hasContent = 1;
        }
        countPtr++;
    }

    if(parenDepth != 0) {
        free(cmd);
        return parse_error(ER_SYNTAX_ERROR, "unmatched parenthesis");
    }

    if(hasContent) {
        fieldCount++; // Add 1 for the last field (no trailing comma)
    }

    if(fieldCount == 0) {
        free(cmd);
        return parse_error(ER_MISSING_ARGUMENT, "field definitions");
    }

    // Allocate fields array
    Field* fields = (Field*)malloc(sizeof(Field) * fieldCount);
    if(fields == NULL) {
        free(cmd);
        return parse_error(ER_OTHER, "Failed to allocate memory for fields");
    }

    // Parse each field definition
    int fieldIndex = 0;
    while(fieldIndex < fieldCount) {
        ptr = skip_whitespace(ptr);

        // Parse field type
        char typeStr[TYPE_STR_LENGTH];
        i = 0;
        while(*ptr && *ptr != ' ' && *ptr != '\t' && *ptr != ',' && *ptr != ')' && i < TYPE_STR_LENGTH - 1) {
            typeStr[i++] = *ptr++;
        }
        typeStr[i] = '\0';

        if(typeStr[0] == '\0') {
            free(fields);
            free(cmd);
            return parse_error(ER_MISSING_ARGUMENT, "field type");
        }

        FieldType fieldType;
        if(parse_field_type(typeStr, &fieldType) != 0) {
            free(fields);
            free(cmd);
            return parse_error(ER_INVALID_DATA_TYPE, typeStr);
        }

        ptr = skip_whitespace(ptr);

        // Parse field name
        char fieldName[MAX_FIELD_NAME_LENGTH];
        i = 0;
        while(*ptr && *ptr != ' ' && *ptr != '\t' && *ptr != ',' && *ptr != ')' && i < MAX_FIELD_NAME_LENGTH - 1) {
            fieldName[i++] = *ptr++;
        }
        fieldName[i] = '\0';

        if(fieldName[0] == '\0') {
            free(fields);
            free(cmd);
            return parse_error(ER_MISSING_ARGUMENT, "field name");
        }

        if(!is_valid_identifier(fieldName)) {
            free(fields);
            free(cmd);
            return parse_error(ER_INVALID_IDENTIFIER, fieldName);
        }

        // Store the field
        fields[fieldIndex].type = fieldType;
        strncpy(fields[fieldIndex].name, fieldName, MAX_FIELD_NAME_LENGTH - 1);
        fields[fieldIndex].name[MAX_FIELD_NAME_LENGTH - 1] = '\0';

        fieldIndex++;

        // Skip whitespace and find comma or closing paren
        ptr = skip_whitespace(ptr);
        if(*ptr == ',') {
            ptr++; // Skip comma
        } else if(*ptr == ')') {
            break; // End of field list
        } else if(*ptr != '\0') {
            free(fields);
            free(cmd);
            return parse_error(ER_SYNTAX_ERROR, "expected ',' or ')'");
        }
    }

    cmd->data.create.fields = fields;
    cmd->data.create.fieldCount = fieldCount;

    return cmd;
}

/**
 * Parse an ADD operation command
 * ADD <DB> <KEY> (<VALUES>)
 */
static Command* parse_add(const char* input) {
    Command* cmd = (Command*)malloc(sizeof(Command));
    if(cmd == NULL) {
        return parse_error(ER_OTHER, "Failed to allocate memory");
    }

    cmd->op = OP_ADD;
    // Stub - actual parsing logic would go here

    return cmd;
}

/**
 * Parse an UP (update) operation command
 * UP <DB> <KEY> (<VALUES>)
 */
static Command* parse_up(const char* input) {
    Command* cmd = (Command*)malloc(sizeof(Command));
    if(cmd == NULL) {
        return parse_error(ER_OTHER, "Failed to allocate memory");
    }

    cmd->op = OP_UP;
    // Stub - actual parsing logic would go here

    return cmd;
}

/**
 * Parse a GET operation command
 * GET <DB> <KEY> (<FIELDS>)
 */
static Command* parse_get(const char* input) {
    Command* cmd = (Command*)malloc(sizeof(Command));
    if(cmd == NULL) {
        return parse_error(ER_OTHER, "Failed to allocate memory");
    }

    cmd->op = OP_GET;
    // Stub - actual parsing logic would go here

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
        size_t len = len_input > len_command ? len_command : len_input;

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
