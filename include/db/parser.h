#ifndef PARSER_H
#define PARSER_H

#include "auth.h"
#include "data_model.h"

#define MAX_ERROR_LENGTH 128

/**
 * Enumeration of supported database operations
 */
typedef enum {
    OP_CREATE,       /* Create a database */
    OP_ADD,          /* Add a record to database */
    OP_UP,           /* Update a record in database */
    OP_GET,          /* Get a record from database */
    OP_DEL,          /* Delete a record from database */
    OP_GET_ALL,      /* Get all records from database */
    OP_SEARCH,       /* Search records with criteria */
    OP_COUNT,        /* Count records in database */
    OP_CREATE_INDEX, /* Create an index on database */
    OP_AUTH,         /* Authenticate with a token */
    OP_ADD_KEY,      /* Add an auth key (admin only) */
    OP_DEL_KEY,      /* Delete an auth key (admin only) */
    OP_LIST_KEYS,    /* List auth keys (admin only) */
    OP_WHOAMI,       /* Return the name of the authenticated user */
    OP_ERROR         /* When an error has occured when parsing */
} Operation;

typedef enum {
    ER_INVALID_COMMAND,       /* Unrecognized command syntax */
    ER_INVALID_CREATE_FORMAT, /* Invalid format for CREATE command */
    ER_INVALID_ADD_FORMAT,    /* Invalid format for ADD command */
    ER_INVALID_UPDATE_FORMAT, /* Invalid format for UPDATE command */
    ER_INVALID_GET_FORMAT,    /* Invalid format for GET command */
    ER_INVALID_DELETE_FORMAT, /* Invalid format for DELETE command */
    ER_INVALID_SEARCH_FORMAT, /* Invalid format for SEARCH command */
    ER_INVALID_COUNT_FORMAT,  /* Invalid format for COUNT command */
    ER_INVALID_INDEX_FORMAT,  /* Invalid format for CREATE INDEX command */
    ER_INVALID_AUTH_FORMAT,   /* Invalid format for AUTH command */
    ER_INVALID_ADD_KEY_FORMAT, /* Invalid format for ADD_KEY command */
    ER_INVALID_DEL_KEY_FORMAT, /* Invalid format for DEL_KEY command */
    ER_MISSING_ARGUMENT,      /* Required argument is missing */
    ER_UNEXPECTED_ARGUMENT,   /* Unexpected extra argument provided */
    ER_INVALID_IDENTIFIER,    /* Invalid identifier name (db, table, field) */
    ER_INVALID_DATA_TYPE,     /* Invalid or unsupported data type */
    ER_SYNTAX_ERROR,          /* General syntax error in command */
    ER_OTHER /*When the error is about smth else like memory allocation */
} ParseError;

/**
 * Data structure for OP_CREATE operation
 * CREATE <DB> (int smth, string smthElse, ...)
 */
typedef struct {
    char   dbName[MAX_DB_NAME_LENGTH]; /* Name of the database to create */
    Field* fields;                     /* Array of field definitions */
    int    fieldCount;                 /* Number of fields in the schema */
} CreateData;

/**
 * Data structure for OP_ADD operation
 * ADD <DB> <KEY> (<VALUES>)
 */
typedef struct {
    char  dbName[MAX_DB_NAME_LENGTH]; /* Name of the database to add to */
    int   key;        /* Key value, can be ignored if auto-generated */
    Data* values;     /* Array of values (excluding the key) */
    int   valueCount; /* Number of values */
    int   autoKey;    /* Flag indicating if key should be auto-generated */
} AddData;

/**
 * Data structure for OP_UP operation
 * UP <DB> <KEY> (<VALUES>)
 */
typedef struct {
    char  dbName[MAX_DB_NAME_LENGTH]; /* Name of the database to update */
    int   key;                        /* Key of the record to update */
    Data* values;      /* Array of values (some may be ignored) */
    int   valueCount;  /* Number of values */
    int*  ignoreFlags; /* Array of flags indicating which fields to ignore */
} UpdateData;

/**
 * Data structure for OP_GET operation
 * GET <DB> <KEY> (<FIELDS>)
 */
typedef struct {
    char   dbName[MAX_DB_NAME_LENGTH]; /* Name of the database to get from */
    int    key;                        /* Key of the record to retrieve */
    char** fields;     /* Array of field names to retrieve (NULL for all) */
    int    fieldCount; /* Number of fields */
} GetData;

/**
 * Data structure for OP_DEL operation
 * DEL <DB> <KEY>
 */
typedef struct {
    char dbName[MAX_DB_NAME_LENGTH]; /* Name of the database to delete from */
    int  key;                        /* Key of the record to delete */
} DeleteData;

/**
 * Data structure for OP_GET_ALL operation
 * GET_ALL <DB> (<FIELDS>)
 */
typedef struct {
    char dbName[MAX_DB_NAME_LENGTH]; /* Name of the database to get all from */
    char** fields;     /* Array of field names to retrieve (NULL for all) */
    int    fieldCount; /* Number of fields */
} GetAllData;

/**
 * Data structure for OP_SEARCH operation
 * SEARCH <DB> <FIELD> <VALUE> (<FIELDS>)
 */
typedef struct {
    char   dbName[MAX_DB_NAME_LENGTH]; /* Name of the database to search in */
    char   fieldName[MAX_FIELD_NAME_LENGTH]; /* Field name to search on */
    Data   value;                            /* Value to search for */
    char** returnFields; /* Array of field names to return (NULL for all) */
    int    fieldCount;   /* Number of return fields */
} SearchData;

/**
 * Data structure for OP_COUNT operation
 * COUNT <DB>
 */
typedef struct {
    char dbName[MAX_DB_NAME_LENGTH]; /* Name of the database to count in */
} CountData;

/**
 * Data structure for OP_CREATE_INDEX operation
 * CREATE_INDEX <DB> <FIELD>
 */
typedef struct {
    char dbName[MAX_DB_NAME_LENGTH]; /* Name of the database to create index on
                                      */
    char fieldName[MAX_FIELD_NAME_LENGTH]; /* Field name to index */
} CreateIndexData;

/**
 * Data structure for OP_AUTH operation
 * AUTH <token>
 */
typedef struct {
    char token[AUTH_TOKEN_BUF_SIZE]; /* Token to authenticate with */
} AuthData;

/**
 * Data structure for OP_ADD_KEY operation
 * ADD_KEY <name> admin|readonly
 */
typedef struct {
    char    name[AUTH_KEY_NAME_MAX]; /* Name label for this key */
    KeyRole role;                    /* ROLE_ADMIN or ROLE_READONLY */
} AddKeyData;

/**
 * Data structure for OP_DEL_KEY operation
 * DEL_KEY <name>
 */
typedef struct {
    char name[AUTH_KEY_NAME_MAX]; /* Name of the key to delete */
} DelKeyData;

/**
 * Data structure for OP_WHOAMI operation
 * WHOAMI (no arguments)
 */
typedef struct {
    char name[AUTH_KEY_NAME_MAX]; /* Filled by main.c from ConnectionManager */
} WhoamiData;

/**
 * Command structure representing a parsed command
 */
typedef struct {
    Operation op; /* Operation type */
    union {
        CreateData create;
        AddData    add;
        UpdateData update;
        GetData    get;
        DeleteData delete;
        GetAllData      get_all;
        SearchData      search;
        CountData       count;
        CreateIndexData create_index;
        AuthData        auth;
        AddKeyData      add_key;
        DelKeyData      del_key;
        WhoamiData      whoami;
        char            error[MAX_ERROR_LENGTH];
    } data; /* Operation-specific data */
} Command;

/**
 * Parse a command string into a Command structure
 *
 * @param input The command string to parse
 * @return Pointer to the parsed command, or NULL if parsing failed
 *         (caller must free this memory when done)
 */
Command* parse_command(const char* input);

/**
 * Free all memory associated with a Command structure
 * Note: For ADD/UPDATE operations, string values are transferred to the
 * database so this only frees the container arrays, not the strings themselves
 *
 * @param cmd Pointer to the command to free
 * @param strings_transferred If true, string values have been transferred to DB
 *                            and should not be freed
 */
void free_command(Command* cmd, int strings_transferred);

#endif /* PARSER_H */
