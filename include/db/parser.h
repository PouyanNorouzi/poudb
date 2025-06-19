#ifndef PARSER_H
#define PARSER_H

#include "data_model.h"

/**
 * Enumeration of supported database operations
 */
typedef enum {
    OP_CREATE,      /* Create a database */
    OP_ADD,         /* Add a record to database */
    OP_UP,          /* Update a record in database */
    OP_GET,         /* Get a record from database */
    OP_DEL,         /* Delete a record from database */
    OP_GET_ALL,     /* Get all records from database */
    OP_SEARCH,      /* Search records with criteria */
    OP_COUNT,       /* Count records in database */
    OP_CREATE_INDEX /* Create an index on database */
} Operation;

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

#endif /* PARSER_H */
