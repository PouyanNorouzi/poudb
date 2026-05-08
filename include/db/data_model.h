#ifndef DATA_MODEL_H
#define DATA_MODEL_H

#include <stdbool.h>
#include <time.h>
#define MAX_DB_NAME_LENGTH    64
#define MAX_FIELD_NAME_LENGTH 64
#define INITIAL_ROW_CAPACITY  16
#define MAX_ROW_CAPACITY      1048576 /* 1M rows max */
#define MAX_FIELDS_PER_DB     64     /* Max user-defined fields per DB (excluding auto-key) */
#define MAX_DB_COUNT          64     /* Max number of databases */
#define MAX_STRING_VALUE_LENGTH 4096 /* Max string value length in bytes (post-unescape) */
#define MAX_ARRAY_LENGTH      1024   /* Max elements per array field */

/* Forward declaration — Data and ArrayData reference each other */
typedef struct ArrayData ArrayData;

/**
 * Enumeration of supported field types
 */
typedef enum {
    TYPE_INT,          /* Integer type */
    TYPE_DOUBLE,       /* Double precision floating point */
    TYPE_BOOL,         /* boolean type */
    TYPE_STRING,       /* String type */
    TYPE_INT_ARRAY,    /* Homogeneous int array */
    TYPE_DOUBLE_ARRAY, /* Homogeneous double array */
    TYPE_BOOL_ARRAY,   /* Homogeneous bool array */
    TYPE_STRING_ARRAY, /* Homogeneous string array */
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
    int size; /* Type tag: -2=int, -1=double, -3=bool, >=0=string length,
               * -4=int[], -5=double[], -6=bool[], -7=string[] */
    union {
        int         i; /* Integer value */
        double      d; /* Double value */
        bool        b; /* boolean value */
        const char* s; /* String value (null-terminated) */
        ArrayData*  a; /* Array value (when size <= -4) */
    } value;           /* Union of possible data types */
} Data;

/**
 * Array value — a homogeneous, bounded sequence of scalar elements.
 * When is_append=1, elements is NULL and append_value carries the single
 * element to push onto the end of the stored row's array.
 */
struct ArrayData {
    int   count;        /* Number of elements (0 when is_append=1) */
    int   is_append;    /* 1 = append one element, 0 = full replacement */
    Data* elements;     /* Element array (NULL when is_append=1) */
    Data  append_value; /* Value to append (valid when is_append=1) */
};

/**
 * Row structure representing a database record
 */
typedef struct {
    Data*  values;     /* Array of values in this row (values[0] is the key) */
    int    valueCount; /* Number of values in this row */
    time_t created_at; /* Unix timestamp when row was inserted */
    time_t updated_at; /* Unix timestamp when row was last updated */
} Row;

/**
 * Opaque hashmap handle for row storage
 */
typedef struct RowHashMap   RowHashMap;
typedef struct DBFieldIndex DBFieldIndex;

/**
 * Iterator state for traversing rows in hashmap storage
 */
typedef struct {
    int   bucketIndex;
    void* node;
} DBRowIterator;

/**
 * Database structure representing a collection/table
 */
typedef struct {
    char        name[MAX_DB_NAME_LENGTH]; /* Name of the database */
    int         fieldsCount;              /* Number of fields/columns */
    Field*      fields; /* Array of field definitions (fields[0] is the key) */
    RowHashMap* rowMap; /* Internal hashmap for rows */
    DBFieldIndex* indexes;   /* Per-field index metadata */
    int           rowsCount; /* Number of rows */
    int           nextKey;   /* Next auto-generated key value */
} DB;

typedef int (*DBVisitor)(DB* db, void* ctx);

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
 * @return 0 on success, -1 if db is NULL, -2 if duplicate name, -3 if malloc
 * failed
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

/**
 * Visit every database in storage
 *
 * @param visitor Callback invoked for each DB
 * @param ctx Opaque pointer passed to visitor
 * @return 0 on success, -1 on invalid input, or callback return value
 */
int db_for_each(DBVisitor visitor, void* ctx);

/**
 * Add a row to a database
 *
 * @param db Pointer to the database
 * @param key The key for the row, or negative for auto-generated key
 * @param values Array of Data values for the row (excluding key)
 * @param valueCount Number of values in the row (excluding key)
 * @return 0 on success, -1 if db is NULL, -2 if valueCount mismatch,
 *         -3 if max capacity reached, -4 if malloc failed, -5 if key exists
 */
int db_add_row(DB* db, int key, Data* values, int valueCount);

/**
 * Get a row from a database by key
 * Returns a deep copy of the row that the caller must free
 *
 * @param db Pointer to the database
 * @param key The key of the row to retrieve
 * @return Pointer to a copied Row, or NULL if not found or on error
 *         (caller must free the returned Row and its values)
 */
Row* db_get_row(DB* db, int key);

/**
 * Update a row in a database by key
 *
 * @param db Pointer to the database
 * @param key The key of the row to update
 * @param values Array of Data values for the row (excluding key)
 * @param valueCount Number of values in the array
 * @param ignoreFlags Array of flags (1 = skip this field, 0 = update it), or
 * NULL to update all
 * @return 0 on success, -1 if db/values is NULL, -2 if valueCount mismatch,
 *         -3 if row not found, -4 if malloc failed
 */
int db_update_row(DB*   db,
                  int   key,
                  Data* values,
                  int   valueCount,
                  int*  ignoreFlags);

/**
 * Delete a row from a database by key
 *
 * @param db Pointer to the database
 * @param key The key of the row to delete
 * @return 0 on success, -1 if db is NULL, -2 if row not found
 */
int db_delete_row(DB* db, int key);

/**
 * Overwrite the stored timestamps on an existing row
 * Used by persistence to restore original timestamps after loading
 *
 * @param db Pointer to the database
 * @param key Key of the row to update
 * @param created_at Original creation timestamp to restore
 * @param updated_at Original update timestamp to restore
 * @return 0 on success, -1 if db is NULL, -2 if row not found
 */
int db_set_row_timestamps(DB* db, int key, time_t created_at, time_t updated_at);

/**
 * Free a row that was returned by db_get_row
 *
 * @param db Pointer to the database (needed for field type info)
 * @param row Pointer to the row to free
 */
void db_free_row(DB* db, Row* row);

/**
 * Iterate rows in a database (unordered)
 *
 * @param db Pointer to the database
 * @param it Pointer to iterator state
 * @return First row pointer or NULL if no rows
 */
Row* db_iter_first(DB* db, DBRowIterator* it);

/**
 * Advance row iterator
 *
 * @param db Pointer to the database
 * @param it Pointer to iterator state
 * @return Next row pointer or NULL if end reached
 */
Row* db_iter_next(DB* db, DBRowIterator* it);

/**
 * Check whether an index exists on a field
 *
 * @param db Pointer to the database
 * @param fieldIdx Field index in db->fields
 * @return 1 if index exists, 0 if not, -1 on invalid input
 */
int db_has_index(DB* db, int fieldIdx);

/**
 * Create an index on a field and build it from existing rows
 *
 * @param db Pointer to the database
 * @param fieldIdx Field index in db->fields
 * @return 0 on success, -1 on invalid input, -2 if already indexed, -3 on
 * malloc failure
 */
int db_create_index(DB* db, int fieldIdx);

/**
 * Collect matching rows for a field equality lookup using an index
 *
 * @param db Pointer to the database
 * @param fieldIdx Field index in db->fields
 * @param value Value to search for
 * @param rowsOut Output array of internal Row pointers (caller must free the
 * array only)
 * @param rowCountOut Number of rows in rowsOut
 * @return 0 on success, -1 on invalid input, -2 if no index exists, -3 on
 * malloc failure
 */
int db_index_collect_rows(DB*    db,
                          int    fieldIdx,
                          Data*  value,
                          Row*** rowsOut,
                          int*   rowCountOut);

#endif /* DATA_MODEL_H */
