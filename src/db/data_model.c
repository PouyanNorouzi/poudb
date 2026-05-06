#include "db/data_model.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils/hashmap.h"
#include "utils/log.h"

/**
 * Node structure for the database linked list
 */
typedef struct DBNode {
    DB*            db;   /* Pointer to the database */
    struct DBNode* next; /* Pointer to next node */
} DBNode;

typedef struct DBIndexEntry {
    Data                 value;
    int*                 keys;
    int                  keyCount;
    int                  keyCapacity;
    struct DBIndexEntry* next;
} DBIndexEntry;

struct DBFieldIndex {
    int            enabled;
    int            bucketCount;
    FieldType      type;
    DBIndexEntry** buckets;
};

/**
 * Static head pointer for the linked list (encapsulated)
 */
static DBNode* db_list_head = NULL;

static Row* find_row(DB* db, int key);
static int  rebuild_enabled_indexes(DB* db);
static int  rebuild_single_index(DB* db, int fieldIdx);
static void clear_index(DBFieldIndex* index);
static int  ensure_index_initialized(DBFieldIndex* index, FieldType type);
static FieldType    array_elem_type(FieldType type);
static unsigned int hash_data_value(const Data* value, FieldType type);
static int  data_value_equals(const Data* a, const Data* b, FieldType type);
static int  clone_data_value(Data* dst, const Data* src, FieldType type);
static void free_data_value(Data* value, FieldType type);
static DBIndexEntry* find_index_entry(DBFieldIndex* index,
                                      const Data*   value,
                                      unsigned int  bucket);
static int           add_key_to_entry(DBIndexEntry* entry, int key);
static void          free_row_values(DB* db, Row* row);
static void          free_row_cb(Row* row, void* ctx);

void init_db_storage(void) { db_list_head = NULL; }

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
        log_warn("Database '%s' already exists", db->name);
        return -2;
    }

    // Check global database count limit
    int count = 0;
    DBNode* cur = db_list_head;
    while(cur != NULL) {
        count++;
        cur = cur->next;
    }
    if(count >= MAX_DB_COUNT) {
        log_warn("Database count limit (%d) reached", MAX_DB_COUNT);
        return -4;
    }

    // Create new node
    DBNode* node = (DBNode*)malloc(sizeof(DBNode));
    if(node == NULL) {
        log_errno("malloc");
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

int db_for_each(DBVisitor visitor, void* ctx) {
    if(visitor == NULL) {
        return -1;
    }

    DBNode* current = db_list_head;
    while(current != NULL) {
        if(current->db != NULL) {
            int rc = visitor(current->db, ctx);
            if(rc != 0) {
                return rc;
            }
        }
        current = current->next;
    }

    return 0;
}

DB* db_create(const char* name, Field* fields, int fieldsCount) {
    if(name == NULL || fields == NULL || fieldsCount <= 0) {
        return NULL;
    }

    DB* db = (DB*)malloc(sizeof(DB));
    if(db == NULL) {
        log_errno("malloc");
        return NULL;
    }

    // Copy database name
    strncpy(db->name, name, MAX_DB_NAME_LENGTH - 1);
    db->name[MAX_DB_NAME_LENGTH - 1] = '\0';

    // Allocate fields array with extra slot for auto-generated key field
    int totalFields = fieldsCount + 1;
    db->fields      = (Field*)malloc(sizeof(Field) * totalFields);
    if(db->fields == NULL) {
        log_errno("malloc");
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

    // Initialize row hashmap buckets
    db->rowMap = row_hashmap_create(INITIAL_ROW_CAPACITY);
    if(db->rowMap == NULL) {
        log_errno("malloc");
        free(db->fields);
        free(db);
        return NULL;
    }

    db->rowsCount = 0;
    db->nextKey   = 1;

    db->indexes =
        (DBFieldIndex*)calloc((size_t)db->fieldsCount, sizeof(DBFieldIndex));
    if(db->indexes == NULL) {
        log_errno("calloc");
        row_hashmap_destroy(db->rowMap, free_row_cb, db);
        free(db->fields);
        free(db);
        return NULL;
    }

    return db;
}

void db_free(DB* db) {
    if(db == NULL) {
        return;
    }

    // Free all rows and hashmap internals
    if(db->rowMap != NULL) {
        row_hashmap_destroy(db->rowMap, free_row_cb, db);
    }

    // Free fields
    if(db->fields != NULL) {
        free(db->fields);
    }

    if(db->indexes != NULL) {
        for(int i = 0; i < db->fieldsCount; i++) {
            clear_index(&db->indexes[i]);
        }
        free(db->indexes);
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
        log_warn("Value count (%d) doesn't match fields count (%d)",
                valueCount,
                db->fieldsCount - 1);
        return -2;
    }

    // Check if we've reached max capacity
    if(db->rowsCount >= MAX_ROW_CAPACITY) {
        log_warn("Maximum row capacity (%d) reached", MAX_ROW_CAPACITY);
        return -3;
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

    // Enforce unique keys in hashmap storage
    if(find_row(db, actualKey) != NULL) {
        log_warn("Key '%d' already exists", actualKey);
        return -5;
    }

    // Allocate memory for the row's values (key + user values)
    int   totalValues = db->fieldsCount;
    Data* rowValues   = (Data*)malloc(sizeof(Data) * totalValues);
    if(rowValues == NULL) {
        log_errno("malloc");
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

    Row newRow;
    newRow.values     = rowValues;
    newRow.valueCount = totalValues;

    if(row_hashmap_insert(db->rowMap, actualKey, newRow, db->rowsCount + 1) !=
       0) {
        free(rowValues);
        return -4;
    }
    db->rowsCount++;

    if(rebuild_enabled_indexes(db) != 0) {
        // Keep row insertion successful even if index rebuild fails.
        // Failing indexes are disabled by rebuild_enabled_indexes.
        log_warn("Failed to rebuild indexes after ADD");
    }

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
        log_errno("malloc");
        return NULL;
    }

    newRow->valueCount = sourceRow->valueCount;

    // Allocate the values array
    newRow->values = (Data*)malloc(sizeof(Data) * newRow->valueCount);
    if(newRow->values == NULL) {
        log_errno("malloc");
        free(newRow);
        return NULL;
    }

    // Deep copy each value
    for(int i = 0; i < newRow->valueCount; i++) {
        if(clone_data_value(&newRow->values[i],
                            &sourceRow->values[i],
                            db->fields[i].type) != 0) {
            log_errno("clone_data_value");
            for(int j = 0; j < i; j++) {
                free_data_value(&newRow->values[j], db->fields[j].type);
            }
            free(newRow->values);
            free(newRow);
            return NULL;
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
        log_warn("Value count (%d) doesn't match fields count (%d)",
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

        int       fieldIdx  = i + 1; /* Skip key field */
        FieldType fieldType = db->fields[fieldIdx].type;
        int is_array = (fieldType == TYPE_INT_ARRAY  ||
                        fieldType == TYPE_DOUBLE_ARRAY ||
                        fieldType == TYPE_BOOL_ARRAY  ||
                        fieldType == TYPE_STRING_ARRAY);

        if(is_array && values[i].value.a != NULL &&
           values[i].value.a->is_append) {
            /* Append one element to the stored array */
            ArrayData* existing  = targetRow->values[fieldIdx].value.a;
            int        old_count = (existing != NULL) ? existing->count : 0;

            if(old_count >= MAX_ARRAY_LENGTH) {
                log_warn("Array field '%s' is already at max length (%d)",
                         db->fields[fieldIdx].name, MAX_ARRAY_LENGTH);
                return -5;
            }

            FieldType  elemType  = array_elem_type(fieldType);
            int        new_count = old_count + 1;
            ArrayData* newArr    = (ArrayData*)malloc(sizeof(ArrayData));
            if(newArr == NULL) {
                log_errno("malloc");
                return -4;
            }
            newArr->count     = new_count;
            newArr->is_append = 0;
            newArr->elements  =
                (Data*)malloc(sizeof(Data) * (size_t)new_count);
            if(newArr->elements == NULL) {
                free(newArr);
                log_errno("malloc");
                return -4;
            }

            for(int j = 0; j < old_count; j++) {
                if(clone_data_value(&newArr->elements[j],
                                    &existing->elements[j],
                                    elemType) != 0) {
                    for(int k = 0; k < j; k++) {
                        free_data_value(&newArr->elements[k], elemType);
                    }
                    free(newArr->elements);
                    free(newArr);
                    log_errno("clone_data_value");
                    return -4;
                }
            }

            if(clone_data_value(&newArr->elements[old_count],
                                &values[i].value.a->append_value,
                                elemType) != 0) {
                for(int k = 0; k < old_count; k++) {
                    free_data_value(&newArr->elements[k], elemType);
                }
                free(newArr->elements);
                free(newArr);
                log_errno("clone_data_value");
                return -4;
            }

            free_data_value(&targetRow->values[fieldIdx], fieldType);
            targetRow->values[fieldIdx].size    = values[i].size;
            targetRow->values[fieldIdx].value.a = newArr;

        } else {
            /* Full replacement for all types (scalar and full array) */
            free_data_value(&targetRow->values[fieldIdx], fieldType);
            if(clone_data_value(&targetRow->values[fieldIdx],
                                &values[i],
                                fieldType) != 0) {
                log_errno("clone_data_value");
                return -4;
            }
        }
    }

    if(rebuild_enabled_indexes(db) != 0) {
        // Row update succeeded; index rebuild failures are downgraded to
        // warning and affected indexes are disabled.
        log_warn("Failed to rebuild indexes after UP");
    }

    return 0;
}

int db_delete_row(DB* db, int key) {
    if(db == NULL) {
        return -1;
    }

    Row removedRow;
    if(row_hashmap_remove(db->rowMap, key, &removedRow) != 0) {
        return -2;  // Row not found
    }

    free_row_values(db, &removedRow);
    db->rowsCount--;

    if(rebuild_enabled_indexes(db) != 0) {
        log_warn("Failed to rebuild indexes after DEL");
    }

    return 0;
}

void db_free_row(DB* db, Row* row) {
    if(row == NULL) {
        return;
    }

    if(row->values != NULL) {
        if(db != NULL) {
            for(int i = 0; i < row->valueCount; i++) {
                free_data_value(&row->values[i], db->fields[i].type);
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
    return row_hashmap_find(db->rowMap, key);
}

Row* db_iter_first(DB* db, DBRowIterator* it) {
    if(db == NULL) {
        return NULL;
    }
    return row_hashmap_iter_first(db->rowMap, it);
}

Row* db_iter_next(DB* db, DBRowIterator* it) {
    if(db == NULL) {
        return NULL;
    }
    return row_hashmap_iter_next(db->rowMap, it);
}

int db_has_index(DB* db, int fieldIdx) {
    if(db == NULL || db->indexes == NULL || fieldIdx < 0 ||
       fieldIdx >= db->fieldsCount) {
        return -1;
    }
    return db->indexes[fieldIdx].enabled ? 1 : 0;
}

int db_create_index(DB* db, int fieldIdx) {
    if(db == NULL || db->indexes == NULL || fieldIdx < 0 ||
       fieldIdx >= db->fieldsCount) {
        return -1;
    }

    if(db->indexes[fieldIdx].enabled) {
        return -2;
    }

    if(rebuild_single_index(db, fieldIdx) != 0) {
        return -3;
    }

    return 0;
}

int db_index_collect_rows(DB*    db,
                          int    fieldIdx,
                          Data*  value,
                          Row*** rowsOut,
                          int*   rowCountOut) {
    if(db == NULL || db->indexes == NULL || value == NULL || rowsOut == NULL ||
       rowCountOut == NULL || fieldIdx < 0 || fieldIdx >= db->fieldsCount) {
        return -1;
    }

    *rowsOut     = NULL;
    *rowCountOut = 0;

    DBFieldIndex* index = &db->indexes[fieldIdx];
    if(!index->enabled || index->bucketCount <= 0 || index->buckets == NULL) {
        return -2;
    }

    unsigned int bucket =
        hash_data_value(value, index->type) % (unsigned int)index->bucketCount;
    DBIndexEntry* entry = find_index_entry(index, value, bucket);
    if(entry == NULL || entry->keyCount <= 0) {
        return 0;
    }

    Row** rows = (Row**)malloc(sizeof(Row*) * (size_t)entry->keyCount);
    if(rows == NULL) {
        return -3;
    }

    int count = 0;
    for(int i = 0; i < entry->keyCount; i++) {
        Row* row = find_row(db, entry->keys[i]);
        if(row != NULL) {
            rows[count++] = row;
        }
    }

    if(count == 0) {
        free(rows);
        return 0;
    }

    *rowsOut     = rows;
    *rowCountOut = count;
    return 0;
}

static int rebuild_enabled_indexes(DB* db) {
    if(db == NULL || db->indexes == NULL) {
        return -1;
    }

    int status = 0;
    for(int i = 0; i < db->fieldsCount; i++) {
        if(!db->indexes[i].enabled) {
            continue;
        }

        if(rebuild_single_index(db, i) != 0) {
            db->indexes[i].enabled = 0;
            status                 = -1;
        }
    }

    return status;
}

static int rebuild_single_index(DB* db, int fieldIdx) {
    if(db == NULL || db->indexes == NULL || fieldIdx < 0 ||
       fieldIdx >= db->fieldsCount) {
        return -1;
    }

    DBFieldIndex* index = &db->indexes[fieldIdx];

    clear_index(index);
    if(ensure_index_initialized(index, db->fields[fieldIdx].type) != 0) {
        return -1;
    }

    DBRowIterator it;
    Row*          row = db_iter_first(db, &it);
    while(row != NULL) {
        Data* value = &row->values[fieldIdx];
        int   key   = row->values[0].value.i;

        unsigned int  bucket = hash_data_value(value, index->type) %
                               (unsigned int)index->bucketCount;
        DBIndexEntry* entry  = find_index_entry(index, value, bucket);

        if(entry == NULL) {
            entry = (DBIndexEntry*)calloc(1, sizeof(DBIndexEntry));
            if(entry == NULL) {
                clear_index(index);
                return -1;
            }

            if(clone_data_value(&entry->value, value, index->type) != 0) {
                free(entry);
                clear_index(index);
                return -1;
            }

            entry->next            = index->buckets[bucket];
            index->buckets[bucket] = entry;
        }

        if(add_key_to_entry(entry, key) != 0) {
            clear_index(index);
            return -1;
        }

        row = db_iter_next(db, &it);
    }

    index->enabled = 1;
    return 0;
}

static void clear_index(DBFieldIndex* index) {
    if(index == NULL) {
        return;
    }

    if(index->buckets != NULL) {
        for(int i = 0; i < index->bucketCount; i++) {
            DBIndexEntry* node = index->buckets[i];
            while(node != NULL) {
                DBIndexEntry* next = node->next;
                free_data_value(&node->value, index->type);
                free(node->keys);
                free(node);
                node = next;
            }
        }
        free(index->buckets);
    }

    index->enabled     = 0;
    index->bucketCount = 0;
    index->buckets     = NULL;
}

static int ensure_index_initialized(DBFieldIndex* index, FieldType type) {
    const int defaultBuckets = 32;

    if(index == NULL) {
        return -1;
    }

    index->buckets =
        (DBIndexEntry**)calloc((size_t)defaultBuckets, sizeof(DBIndexEntry*));
    if(index->buckets == NULL) {
        return -1;
    }

    index->bucketCount = defaultBuckets;
    index->type        = type;
    index->enabled     = 0;
    return 0;
}

/* Return the scalar element FieldType that corresponds to an array FieldType. */
static FieldType array_elem_type(FieldType type) {
    switch(type) {
        case TYPE_INT_ARRAY:    return TYPE_INT;
        case TYPE_DOUBLE_ARRAY: return TYPE_DOUBLE;
        case TYPE_BOOL_ARRAY:   return TYPE_BOOL;
        case TYPE_STRING_ARRAY: return TYPE_STRING;
        default:                return TYPE_INT; /* unreachable */
    }
}

static unsigned int hash_data_value(const Data* value, FieldType type) {
    if(value == NULL) {
        return 0U;
    }

    switch(type) {
        case TYPE_INT:    return (unsigned int)value->value.i * 2654435761U;
        case TYPE_DOUBLE: {
            union {
                double   d;
                uint64_t u;
            } bits;
            bits.d = value->value.d;
            return (unsigned int)(bits.u ^ (bits.u >> 32));
        }
        case TYPE_BOOL:   return value->value.b ? 1U : 0U;
        case TYPE_STRING: {
            const unsigned char* s = (const unsigned char*)value->value.s;
            unsigned int         h = 5381U;

            if(s == NULL) {
                return 0U;
            }

            while(*s != '\0') {
                h = ((h << 5) + h) + (unsigned int)(*s);
                s++;
            }
            return h;
        }
        default: return 0U;
    }
}

static int data_value_equals(const Data* a, const Data* b, FieldType type) {
    if(a == NULL || b == NULL) {
        return 0;
    }

    switch(type) {
        case TYPE_INT:    return a->value.i == b->value.i;
        case TYPE_DOUBLE: return a->value.d == b->value.d;
        case TYPE_BOOL:   return a->value.b == b->value.b;
        case TYPE_STRING:
            if(a->value.s == NULL && b->value.s == NULL) {
                return 1;
            }
            if(a->value.s == NULL || b->value.s == NULL) {
                return 0;
            }
            return strcmp(a->value.s, b->value.s) == 0;
        default: return 0;
    }
}

static int clone_data_value(Data* dst, const Data* src, FieldType type) {
    if(dst == NULL || src == NULL) {
        return -1;
    }

    dst->size = src->size;

    switch(type) {
        case TYPE_INT:
        case TYPE_DOUBLE:
        case TYPE_BOOL:
            dst->value = src->value;
            return 0;

        case TYPE_STRING: {
            if(src->value.s == NULL) {
                dst->value.s = NULL;
                return 0;
            }
            char* copy = (char*)malloc(strlen(src->value.s) + 1);
            if(copy == NULL) {
                return -1;
            }
            strcpy(copy, src->value.s);
            dst->value.s = copy;
            return 0;
        }

        case TYPE_INT_ARRAY:
        case TYPE_DOUBLE_ARRAY:
        case TYPE_BOOL_ARRAY:
        case TYPE_STRING_ARRAY: {
            if(src->value.a == NULL) {
                dst->value.a = NULL;
                return 0;
            }
            ArrayData* srcArr = src->value.a;
            ArrayData* dstArr = (ArrayData*)malloc(sizeof(ArrayData));
            if(dstArr == NULL) {
                return -1;
            }
            FieldType elemType        = array_elem_type(type);
            dstArr->count             = srcArr->count;
            dstArr->is_append         = srcArr->is_append;
            dstArr->elements          = NULL;
            dstArr->append_value.size = 0;
            if(srcArr->is_append) {
                if(clone_data_value(&dstArr->append_value,
                                    &srcArr->append_value,
                                    elemType) != 0) {
                    free(dstArr);
                    return -1;
                }
            } else if(srcArr->count > 0) {
                dstArr->elements =
                    (Data*)malloc(sizeof(Data) * (size_t)srcArr->count);
                if(dstArr->elements == NULL) {
                    free(dstArr);
                    return -1;
                }
                for(int i = 0; i < srcArr->count; i++) {
                    if(clone_data_value(&dstArr->elements[i],
                                        &srcArr->elements[i],
                                        elemType) != 0) {
                        for(int j = 0; j < i; j++) {
                            free_data_value(&dstArr->elements[j], elemType);
                        }
                        free(dstArr->elements);
                        free(dstArr);
                        return -1;
                    }
                }
            }
            dst->value.a = dstArr;
            return 0;
        }

        default:
            return -1;
    }
}

static void free_data_value(Data* value, FieldType type) {
    if(value == NULL) {
        return;
    }

    switch(type) {
        case TYPE_STRING:
            if(value->value.s != NULL) {
                free((void*)value->value.s);
                value->value.s = NULL;
            }
            break;

        case TYPE_INT_ARRAY:
        case TYPE_DOUBLE_ARRAY:
        case TYPE_BOOL_ARRAY:
        case TYPE_STRING_ARRAY: {
            if(value->value.a == NULL) {
                break;
            }
            ArrayData* arr      = value->value.a;
            FieldType  elemType = array_elem_type(type);
            if(arr->is_append) {
                free_data_value(&arr->append_value, elemType);
            } else if(arr->elements != NULL) {
                for(int i = 0; i < arr->count; i++) {
                    free_data_value(&arr->elements[i], elemType);
                }
                free(arr->elements);
            }
            free(arr);
            value->value.a = NULL;
            break;
        }

        default:
            break;
    }
}

static DBIndexEntry* find_index_entry(DBFieldIndex* index,
                                      const Data*   value,
                                      unsigned int  bucket) {
    if(index == NULL || index->buckets == NULL ||
       bucket >= (unsigned int)index->bucketCount) {
        return NULL;
    }

    DBIndexEntry* node = index->buckets[bucket];
    while(node != NULL) {
        if(data_value_equals(&node->value, value, index->type)) {
            return node;
        }
        node = node->next;
    }

    return NULL;
}

static int add_key_to_entry(DBIndexEntry* entry, int key) {
    if(entry == NULL) {
        return -1;
    }

    for(int i = 0; i < entry->keyCount; i++) {
        if(entry->keys[i] == key) {
            return 0;
        }
    }

    if(entry->keyCount == entry->keyCapacity) {
        int newCapacity =
            (entry->keyCapacity == 0) ? 4 : entry->keyCapacity * 2;
        int* newKeys =
            (int*)realloc(entry->keys, sizeof(int) * (size_t)newCapacity);
        if(newKeys == NULL) {
            return -1;
        }
        entry->keys        = newKeys;
        entry->keyCapacity = newCapacity;
    }

    entry->keys[entry->keyCount++] = key;
    return 0;
}

static void free_row_values(DB* db, Row* row) {
    if(db == NULL || row == NULL || row->values == NULL) {
        return;
    }

    for(int i = 0; i < row->valueCount; i++) {
        free_data_value(&row->values[i], db->fields[i].type);
    }
    free(row->values);
    row->values     = NULL;
    row->valueCount = 0;
}

static void free_row_cb(Row* row, void* ctx) { free_row_values((DB*)ctx, row); }
