#include "utils/hashmap.h"

#include <stdlib.h>

typedef struct RowNode {
    int             key;
    Row             row;
    struct RowNode* next;
} RowNode;

struct RowHashMap {
    RowNode** buckets;
    int       bucketCount;
};

static unsigned int hash_key(int key);
static int          should_rehash(int size, int bucketCount);
static int          rehash_rows(RowHashMap* map, int newBucketCount);

RowHashMap* row_hashmap_create(int bucketCount) {
    if(bucketCount <= 0) {
        return NULL;
    }

    RowHashMap* map = (RowHashMap*)malloc(sizeof(RowHashMap));
    if(map == NULL) {
        return NULL;
    }

    map->buckets = (RowNode**)calloc((size_t)bucketCount, sizeof(RowNode*));
    if(map->buckets == NULL) {
        free(map);
        return NULL;
    }

    map->bucketCount = bucketCount;
    return map;
}

void row_hashmap_destroy(RowHashMap* map,
                         void (*free_row_cb)(Row* row, void* ctx),
                         void* ctx) {
    if(map == NULL) {
        return;
    }

    if(map->buckets != NULL) {
        for(int i = 0; i < map->bucketCount; i++) {
            RowNode* node = map->buckets[i];
            while(node != NULL) {
                RowNode* next = node->next;
                if(free_row_cb != NULL) {
                    free_row_cb(&node->row, ctx);
                }
                free(node);
                node = next;
            }
        }
        free(map->buckets);
    }

    free(map);
}

Row* row_hashmap_find(RowHashMap* map, int key) {
    if(map == NULL || map->buckets == NULL || map->bucketCount <= 0) {
        return NULL;
    }

    unsigned int bucket = hash_key(key) % (unsigned int)map->bucketCount;
    RowNode*     node   = map->buckets[bucket];

    while(node != NULL) {
        if(node->key == key) {
            return &node->row;
        }
        node = node->next;
    }

    return NULL;
}

int row_hashmap_insert(RowHashMap* map, int key, Row row, int sizeAfterInsert) {
    if(map == NULL || map->buckets == NULL || map->bucketCount <= 0) {
        return -1;
    }

    RowNode* node = (RowNode*)malloc(sizeof(RowNode));
    if(node == NULL) {
        return -1;
    }

    node->key = key;
    node->row = row;

    unsigned int bucket = hash_key(key) % (unsigned int)map->bucketCount;
    node->next          = map->buckets[bucket];
    map->buckets[bucket] = node;

    if(should_rehash(sizeAfterInsert, map->bucketCount)) {
        int newBucketCount = map->bucketCount * 2;
        if(newBucketCount > map->bucketCount) {
            rehash_rows(map, newBucketCount);
        }
    }

    return 0;
}

int row_hashmap_remove(RowHashMap* map, int key, Row* removedRow) {
    if(map == NULL || map->buckets == NULL || map->bucketCount <= 0 ||
       removedRow == NULL) {
        return -1;
    }

    unsigned int bucket = hash_key(key) % (unsigned int)map->bucketCount;
    RowNode*     node   = map->buckets[bucket];
    RowNode*     prev   = NULL;

    while(node != NULL && node->key != key) {
        prev = node;
        node = node->next;
    }

    if(node == NULL) {
        return -1;
    }

    if(prev == NULL) {
        map->buckets[bucket] = node->next;
    } else {
        prev->next = node->next;
    }

    *removedRow = node->row;
    free(node);
    return 0;
}

Row* row_hashmap_iter_first(RowHashMap* map, DBRowIterator* it) {
    if(it == NULL) {
        return NULL;
    }

    it->bucketIndex = -1;
    it->node        = NULL;
    return row_hashmap_iter_next(map, it);
}

Row* row_hashmap_iter_next(RowHashMap* map, DBRowIterator* it) {
    if(map == NULL || map->buckets == NULL || map->bucketCount <= 0 ||
       it == NULL) {
        return NULL;
    }

    RowNode* node = (RowNode*)it->node;
    if(node != NULL && node->next != NULL) {
        node     = node->next;
        it->node = node;
        return &node->row;
    }

    for(int i = it->bucketIndex + 1; i < map->bucketCount; i++) {
        if(map->buckets[i] != NULL) {
            it->bucketIndex = i;
            it->node        = map->buckets[i];
            return &((RowNode*)it->node)->row;
        }
    }

    it->bucketIndex = map->bucketCount;
    it->node        = NULL;
    return NULL;
}

static unsigned int hash_key(int key) {
    unsigned int x = (unsigned int)key;
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

static int should_rehash(int size, int bucketCount) {
    if(bucketCount <= 0) {
        return 0;
    }
    return (size * 4) >= (bucketCount * 3);
}

static int rehash_rows(RowHashMap* map, int newBucketCount) {
    if(map == NULL || map->buckets == NULL ||
       newBucketCount <= map->bucketCount) {
        return -1;
    }

    RowNode** newBuckets = (RowNode**)calloc((size_t)newBucketCount,
                                             sizeof(RowNode*));
    if(newBuckets == NULL) {
        return -1;
    }

    for(int i = 0; i < map->bucketCount; i++) {
        RowNode* node = map->buckets[i];
        while(node != NULL) {
            RowNode* next   = node->next;
            unsigned int bi = hash_key(node->key) % (unsigned int)newBucketCount;
            node->next      = newBuckets[bi];
            newBuckets[bi]  = node;
            node            = next;
        }
    }

    free(map->buckets);
    map->buckets     = newBuckets;
    map->bucketCount = newBucketCount;
    return 0;
}
