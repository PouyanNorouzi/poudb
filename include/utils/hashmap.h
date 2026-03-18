#ifndef HASHMAP_H
#define HASHMAP_H

#include "db/data_model.h"

RowHashMap* row_hashmap_create(int bucketCount);

void row_hashmap_destroy(RowHashMap* map,
                         void (*free_row_cb)(Row* row, void* ctx),
                         void* ctx);

Row* row_hashmap_find(RowHashMap* map, int key);

int row_hashmap_insert(RowHashMap* map, int key, Row row, int sizeAfterInsert);

int row_hashmap_remove(RowHashMap* map, int key, Row* removedRow);

Row* row_hashmap_iter_first(RowHashMap* map, DBRowIterator* it);

Row* row_hashmap_iter_next(RowHashMap* map, DBRowIterator* it);

#endif /* HASHMAP_H */
