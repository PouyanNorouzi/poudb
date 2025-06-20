#include "db/parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#define COMMAND_LENGTH 9

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

/**
 * Parse a command string into a Command structure
 */
Command* parse_command(const char* input) {
    if(input == NULL || *input == '\0') {
        return parse_error(ER_INVALID_COMMAND, NULL);
    }

    // Determine which operation to parse
    Operation op = determine_operation(input);

    // Call the appropriate parsing function
    switch(op) {
        case OP_CREATE:       return parse_create(input);
        case OP_ADD:          return parse_add(input);
        case OP_UP:           return parse_up(input);
        case OP_GET:          return parse_get(input);
        case OP_DEL:          return parse_del(input);
        case OP_GET_ALL:      return parse_get_all(input);
        case OP_SEARCH:       return parse_search(input);
        case OP_COUNT:        return parse_count(input);
        case OP_CREATE_INDEX: return parse_create_index(input);
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
    // Stub - actual parsing logic would go here

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

    // Check each command string
    for(int i = 0; i < COMMAND_LENGTH; i++) {
        size_t len = strlen(COMMAND_STRINGS[i]);
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
