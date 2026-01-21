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

/**
 * Initialize the database storage system
 */
void init_db_storage(void) { db_list_head = NULL; }

/**
 * Free all databases and cleanup the storage system
 */
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

/**
 * Add a database to the storage system
 */
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

/**
 * Find a database by name
 */
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

/**
 * Remove a database from storage by name
 */
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

/**
 * Create a new database with the given name and fields
 */
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

    // Allocate and copy fields
    db->fields = (Field*)malloc(sizeof(Field) * fieldsCount);
    if(db->fields == NULL) {
        perror("malloc");
        free(db);
        return NULL;
    }

    memcpy(db->fields, fields, sizeof(Field) * fieldsCount);
    db->fieldsCount = fieldsCount;

    // Initialize rows
    db->rows      = NULL;
    db->rowsCount = 0;

    return db;
}

/**
 * Free all resources associated with a database
 */
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
