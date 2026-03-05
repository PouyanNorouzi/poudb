#include "db/data_model.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Node structure for the database linked list
 */
typedef struct DBNode {
    DB*            db;   /* Pointer to the database */
    struct DBNode* next; /* Pointer to next node */
} DBNode;

/**
 * Static head pointer for the linked list (encapsulated)
 */
static DBNode* db_list_head = NULL;

static Row* find_row(DB* db, int key);

void init_db_storage(void) { db_list_head = NULL; }

void iter_db_storage(void (*callback)(DB* db, void* ctx), void* ctx) {
    DBNode* current = db_list_head;
    while(current != NULL) {
        if(current->db != NULL) {
            callback(current->db, ctx);
        }
        current = current->next;
    }
}

void free_db_storage(void) {
    DBNode* current = db_list_head;
    DBNode* next;

    while(current != NULL) {
        next = current->next;

        if(current->db != NULL) {
            db_free(current->db);
        }

        free(current);
        current = next;
    }

    db_list_head = NULL;
}

int add_db(DB* db) {
    if(db == NULL) {
        return -1;
    }

    // Check if database with same name already exists
    if(find_db(db->name) != NULL) {
        fprintf(stderr, "Database '%s' already exists\n", db->name);
        return -2;
    }

    // Create new node
    DBNode* node = (DBNode*)malloc(sizeof(DBNode));
    if(node == NULL) {
        perror("malloc");
        return -3;
    }

    node->db     = db;
    node->next   = db_list_head;
    db_list_head = node;

    return 0;
}

DB* find_db(const char* name) {
    if(name == NULL) {
        return NULL;
    }

    DBNode* current = db_list_head;

    while(current != NULL) {
        if(current->db != NULL && strcmp(current->db->name, name) == 0) {
            return current->db;
        }
        current = current->next;
    }

    return NULL;
}

int remove_db(const char* name) {
    if(name == NULL) {
        return -1;
    }

    DBNode* current = db_list_head;
    DBNode* prev    = NULL;

    while(current != NULL) {
        if(current->db != NULL && strcmp(current->db->name, name) == 0) {
            // Remove from list
            if(prev == NULL) {
                // Removing head
                db_list_head = current->next;
            } else {
                prev->next = current->next;
            }

            // Free the database and node
            db_free(current->db);
            free(current);

            return 0;
        }

        prev    = current;
        current = current->next;
    }

    return -1;  // Not found
}

DB* db_create(const char* name, Field* fields, int fieldsCount) {
    if(name == NULL || fields == NULL || fieldsCount <= 0) {
        return NULL;
    }

    DB* db = (DB*)malloc(sizeof(DB));
    if(db == NULL) {
        perror("malloc");
        return NULL;
    }

    // Copy database name
    strncpy(db->name, name, MAX_DB_NAME_LENGTH - 1);
    db->name[MAX_DB_NAME_LENGTH - 1] = '\0';

    // Allocate fields array with extra slot for auto-generated key field
    int totalFields = fieldsCount + 1;
    db->fields      = (Field*)malloc(sizeof(Field) * totalFields);
    if(db->fields == NULL) {
        perror("malloc");
        free(db);
        return NULL;
    }

    // First field is always the auto-generated key
    strncpy(db->fields[0].name, "key", MAX_FIELD_NAME_LENGTH - 1);
    db->fields[0].name[MAX_FIELD_NAME_LENGTH - 1] = '\0';
    db->fields[0].type                            = TYPE_INT;

    // Copy user-provided fields after the key field
    memcpy(&db->fields[1], fields, sizeof(Field) * fieldsCount);
    db->fieldsCount = totalFields;

    // Pre-allocate rows array
    db->rows = (Row*)malloc(sizeof(Row) * INITIAL_ROW_CAPACITY);
    if(db->rows == NULL) {
        perror("malloc");
        free(db->fields);
        free(db);
        return NULL;
    }

    db->rowsCount    = 0;
    db->rowsCapacity = INITIAL_ROW_CAPACITY;
    db->nextKey      = 1;

    return db;
}

void db_free(DB* db) {
    if(db == NULL) {
        return;
    }

    // Free all rows
    if(db->rows != NULL) {
        for(int i = 0; i < db->rowsCount; i++) {
            if(db->rows[i].values != NULL) {
                // Free string values
                for(int j = 0; j < db->rows[i].valueCount; j++) {
                    if(db->fields[j].type == TYPE_STRING &&
                       db->rows[i].values[j].value.s != NULL) {
                        free((void*)db->rows[i].values[j].value.s);
                    }
                }
                free(db->rows[i].values);
            }
        }
        free(db->rows);
    }

    // Free fields
    if(db->fields != NULL) {
        free(db->fields);
    }

    // Free the database itself
    free(db);
}

int db_add_row(DB* db, int key, Data* values, int valueCount) {
    if(db == NULL || values == NULL) {
        return -1;
    }

    // valueCount should match fieldsCount - 1 (excluding key field)
    if(valueCount != db->fieldsCount - 1) {
        fprintf(stderr,
                "Value count (%d) doesn't match fields count (%d)\n",
                valueCount,
                db->fieldsCount - 1);
        return -2;
    }

    // Check if we've reached max capacity
    if(db->rowsCount >= MAX_ROW_CAPACITY) {
        fprintf(stderr,
                "Maximum row capacity (%d) reached\n",
                MAX_ROW_CAPACITY);
        return -3;
    }

    // Check if we need to grow the array
    if(db->rowsCount >= db->rowsCapacity) {
        int newCapacity = db->rowsCapacity * 2;
        if(newCapacity > MAX_ROW_CAPACITY) {
            newCapacity = MAX_ROW_CAPACITY;
        }

        Row* newRows = (Row*)realloc(db->rows, sizeof(Row) * newCapacity);
        if(newRows == NULL) {
            perror("realloc");
            return -4;
        }

        db->rows         = newRows;
        db->rowsCapacity = newCapacity;
    }

    // Determine the actual key to use
    int actualKey;
    if(key < 0) {
        // Auto-generate key
        actualKey = db->nextKey;
        db->nextKey++;
    } else {
        actualKey = key;
        // Update nextKey if this key is >= current nextKey
        if(actualKey >= db->nextKey) {
            db->nextKey = actualKey + 1;
        }
    }

    // Allocate memory for the row's values (key + user values)
    int   totalValues = db->fieldsCount;
    Data* rowValues   = (Data*)malloc(sizeof(Data) * totalValues);
    if(rowValues == NULL) {
        perror("malloc");
        return -4;
    }

    // Set the key as first value
    rowValues[0].size    = sizeof(int);
    rowValues[0].value.i = actualKey;

    // Copy user values (transfer ownership of strings - no duplication needed)
    for(int i = 0; i < valueCount; i++) {
        int fieldIdx              = i + 1;  // Skip key field
        rowValues[fieldIdx].size  = values[i].size;
        rowValues[fieldIdx].value = values[i].value;
    }

    // Add the row
    db->rows[db->rowsCount].values     = rowValues;
    db->rows[db->rowsCount].valueCount = totalValues;
    db->rowsCount++;

    return 0;
}

Row* db_get_row(DB* db, int key) {
    if(db == NULL) {
        return NULL;
    }

    // Find the row with the matching key
    Row* sourceRow = find_row(db, key);
    if(sourceRow == NULL) {
        return NULL;  // Row not found
    }

    // Allocate the new row structure
    Row* newRow = (Row*)malloc(sizeof(Row));
    if(newRow == NULL) {
        perror("malloc");
        return NULL;
    }

    newRow->valueCount = sourceRow->valueCount;

    // Allocate the values array
    newRow->values = (Data*)malloc(sizeof(Data) * newRow->valueCount);
    if(newRow->values == NULL) {
        perror("malloc");
        free(newRow);
        return NULL;
    }

    // Deep copy each value
    for(int i = 0; i < newRow->valueCount; i++) {
        newRow->values[i].size = sourceRow->values[i].size;

        if(db->fields[i].type == TYPE_STRING) {
            // Deep copy string
            const char* srcStr = sourceRow->values[i].value.s;
            if(srcStr != NULL) {
                char* newStr = (char*)malloc(strlen(srcStr) + 1);
                if(newStr == NULL) {
                    perror("malloc");
                    // Free already copied strings
                    for(int j = 0; j < i; j++) {
                        if(db->fields[j].type == TYPE_STRING &&
                           newRow->values[j].value.s != NULL) {
                            free((void*)newRow->values[j].value.s);
                        }
                    }
                    free(newRow->values);
                    free(newRow);
                    return NULL;
                }
                strcpy(newStr, srcStr);
                newRow->values[i].value.s = newStr;
            } else {
                newRow->values[i].value.s = NULL;
            }
        } else {
            // Copy non-string values directly
            newRow->values[i].value = sourceRow->values[i].value;
        }
    }

    return newRow;
}

int db_update_row(DB*   db,
                  int   key,
                  Data* values,
                  int   valueCount,
                  int*  ignoreFlags) {
    if(db == NULL || values == NULL) {
        return -1;
    }

    // valueCount should match fieldsCount - 1 (excluding key field)
    if(valueCount != db->fieldsCount - 1) {
        fprintf(stderr,
                "Value count (%d) doesn't match fields count (%d)\n",
                valueCount,
                db->fieldsCount - 1);
        return -2;
    }

    // Find the row with the matching key
    Row* targetRow = find_row(db, key);
    if(targetRow == NULL) {
        return -3;  // Row not found
    }

    // Update each value (skip key field at index 0)
    for(int i = 0; i < valueCount; i++) {
        // Check if this field should be ignored
        if(ignoreFlags != NULL && ignoreFlags[i]) {
            continue;
        }

        int fieldIdx = i + 1;  // Skip key field

        // Handle string fields - free old string and copy new one
        if(db->fields[fieldIdx].type == TYPE_STRING) {
            // Free old string if it exists
            if(targetRow->values[fieldIdx].value.s != NULL) {
                free((void*)targetRow->values[fieldIdx].value.s);
            }

            // Copy new string
            const char* srcStr = values[i].value.s;
            if(srcStr != NULL) {
                char* newStr = (char*)malloc(strlen(srcStr) + 1);
                if(newStr == NULL) {
                    perror("malloc");
                    return -4;
                }
                strcpy(newStr, srcStr);
                targetRow->values[fieldIdx].value.s = newStr;
            } else {
                targetRow->values[fieldIdx].value.s = NULL;
            }
            targetRow->values[fieldIdx].size = values[i].size;
        } else {
            // Copy non-string values directly
            targetRow->values[fieldIdx].size  = values[i].size;
            targetRow->values[fieldIdx].value = values[i].value;
        }
    }

    return 0;
}

int db_delete_row(DB* db, int key) {
    if(db == NULL) {
        return -1;
    }

    // Find the row with the matching key
    Row* rowToDelete = find_row(db, key);
    if(rowToDelete == NULL) {
        return -2;  // Row not found
    }

    // Calculate the index for shifting
    int rowIndex = rowToDelete - db->rows;

    // Free the row's values (including strings)
    if(rowToDelete->values != NULL) {
        for(int j = 0; j < rowToDelete->valueCount; j++) {
            if(db->fields[j].type == TYPE_STRING &&
               rowToDelete->values[j].value.s != NULL) {
                free((void*)rowToDelete->values[j].value.s);
            }
        }
        free(rowToDelete->values);
    }

    // Shift all subsequent rows down by one
    for(int i = rowIndex; i < db->rowsCount - 1; i++) {
        db->rows[i] = db->rows[i + 1];
    }

    db->rowsCount--;

    return 0;
}

void db_free_row(DB* db, Row* row) {
    if(row == NULL) {
        return;
    }

    if(row->values != NULL) {
        // Free string values if db is provided
        if(db != NULL) {
            for(int i = 0; i < row->valueCount; i++) {
                if(db->fields[i].type == TYPE_STRING &&
                   row->values[i].value.s != NULL) {
                    free((void*)row->values[i].value.s);
                }
            }
        }
        free(row->values);
    }

    free(row);
}

static Row* find_row(DB* db, int key) {
    if(db == NULL) {
        return NULL;
    }

    for(int i = 0; i < db->rowsCount; i++) {
        if(db->rows[i].values != NULL && db->rows[i].values[0].value.i == key) {
            return &db->rows[i];
        }
    }

    return NULL;
}
