#ifndef DATA_MODEL_H
#define DATA_MODEL_H

#include <stdbool.h>
#define MAX_DB_NAME_LENGTH    64
#define MAX_FIELD_NAME_LENGTH 64

/**
 * Enumeration of supported field types
 */
typedef enum {
    TYPE_INT,    /* Integer type */
    TYPE_DOUBLE, /* Double precision floating point */
    TYPE_BOOL,   /* boolean type */
    TYPE_STRING, /* String type */
} FieldType;

/**
 * Field definition structure
 * Represents a column in a database table
 */
typedef struct {
    char      name[MAX_FIELD_NAME_LENGTH]; /* Name of the field/column */
    FieldType type;                        /* Data type of the field */
} Field;

/**
 * Data structure for storing typed values
 * Provides a union for different data types
 */
typedef struct {
    int size; /* Size of the data (relevant for strings) */
    union {
        int         i; /* Integer value */
        double      d; /* Double value */
        bool        b; /* boolean value */
        const char* s; /* String value (null-terminated) */
    } value;           /* Union of possible data types */
} Data;

/**
 * Row structure representing a database record
 */
typedef struct {
    Data* values;     /* Array of values in this row (values[0] is the key) */
    int   valueCount; /* Number of values in this row */
} Row;

/**
 * Database structure representing a collection/table
 */
typedef struct {
    char   name[MAX_DB_NAME_LENGTH]; /* Name of the database */
    int    fieldsCount;              /* Number of fields/columns */
    Field* fields;    /* Array of field definitions (fields[0] is the key) */
    Row*   rows;      /* Array of rows/records */
    int    rowsCount; /* Number of rows */
} DB;

/**
 * Create a new database with the given name and fields
 *
 * @param name Name of the database
 * @param fields Array of field definitions
 * @param fieldsCount Number of fields
 * @return Pointer to the newly created database, or NULL if creation failed
 */
DB* db_create(const char* name, Field* fields, int fieldsCount);

/**
 * Free all resources associated with a database
 *
 * @param db Pointer to the database to free
 */
void db_free(DB* db);

/**
 * Initialize the database storage system
 * Must be called before any database operations
 */
void init_db_storage(void);

/**
 * Free all databases and cleanup the storage system
 */
void free_db_storage(void);

/**
 * Add a database to the storage system
 *
 * @param db Pointer to the database to add
 * @return 0 on success, -1 if db is NULL, -2 if duplicate name, -3 if malloc failed
 */
int add_db(DB* db);

/**
 * Find a database by name
 *
 * @param name Name of the database to find
 * @return Pointer to the database, or NULL if not found
 */
DB* find_db(const char* name);

/**
 * Remove a database from storage by name
 *
 * @param name Name of the database to remove
 * @return 0 on success, -1 if not found
 */
int remove_db(const char* name);

#endif /* DATA_MODEL_H */
