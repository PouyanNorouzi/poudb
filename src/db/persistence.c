#include "db/persistence.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "db/data_model.h"

/* Magic bytes written at the start of every data file */
static const char POUDB_MAGIC[6] = {'P', 'O', 'U', 'D', 'B', '\0'};
#define POUDB_VERSION ((uint32_t)1)

/* -------------------------------------------------------------------------
 * Internal helpers
 * ---------------------------------------------------------------------- */

/** Write exactly n bytes, return 0 on success, -1 on error. */
static int write_bytes(FILE* f, const void* buf, size_t n) {
    if(fwrite(buf, 1, n, f) != n) {
        perror("fwrite");
        return -1;
    }
    return 0;
}

/** Read exactly n bytes, return 0 on success, -1 on error. */
static int read_bytes(FILE* f, void* buf, size_t n) {
    if(fread(buf, 1, n, f) != n) {
        if(!feof(f)) {
            perror("fread");
        }
        return -1;
    }
    return 0;
}

/* -------------------------------------------------------------------------
 * save_db_storage
 * ---------------------------------------------------------------------- */

/**
 * Iterate the internal linked-list via find_db / add_db indirectly by using
 * the public API.  Because the linked list is private we expose a helper that
 * lets us visit every DB: we rely on the exported symbols only.
 *
 * File layout (all multi-byte integers are native-endian):
 *   [6]  magic  "POUDB\0"
 *   [4]  version  (uint32_t = 1)
 *   [4]  db_count (int32_t)
 *   For each DB:
 *     [64] name  (char[MAX_DB_NAME_LENGTH])
 *     [4]  fieldsCount (int32_t)  -- includes implicit key field
 *     [4]  nextKey     (int32_t)
 *     For each field:
 *       [64] field.name (char[MAX_FIELD_NAME_LENGTH])
 *       [4]  field.type (int32_t)
 *     [4]  rowsCount (int32_t)
 *     For each row:
 *       [4]  valueCount (int32_t)
 *       For each value (index 0 = key):
 *         [4]  size (int32_t)
 *         value data:
 *           TYPE_INT:    [4] int32_t
 *           TYPE_DOUBLE: [8] double
 *           TYPE_BOOL:   [4] int32_t  (0 or 1)
 *           TYPE_STRING: [4] uint32_t length  +  [length] chars (no NUL)
 */

/* We need access to every DB in the list.  The data_model only exposes
 * find_db(name) and remove_db(name).  To iterate we use a helper stored in
 * data_model.c that we declare here as an internal cross-unit accessor. */

/* Forward-declare the iterator helper that data_model exposes for us: */
extern void iter_db_storage(void (*callback)(DB* db, void* ctx), void* ctx);

typedef struct {
    FILE* f;
    int   error;
    int   count;
} SaveCtx;

static int write_db(FILE* f, DB* db);

static void save_one_db(DB* db, void* ctx) {
    SaveCtx* c = (SaveCtx*)ctx;
    if(c->error) {
        return;
    }
    if(write_db(c->f, db) != 0) {
        c->error = 1;
    } else {
        c->count++;
    }
}

static int write_value(FILE* f, FieldType type, const Data* d) {
    int32_t  iv;
    double   dv;
    int32_t  bv;
    uint32_t slen;

    switch(type) {
        case TYPE_INT:
            iv = (int32_t)d->value.i;
            return write_bytes(f, &iv, sizeof(iv));
        case TYPE_DOUBLE:
            dv = d->value.d;
            return write_bytes(f, &dv, sizeof(dv));
        case TYPE_BOOL:
            bv = d->value.b ? 1 : 0;
            return write_bytes(f, &bv, sizeof(bv));
        case TYPE_STRING:
            slen = (d->value.s != NULL) ? (uint32_t)strlen(d->value.s) : 0;
            if(write_bytes(f, &slen, sizeof(slen)) != 0) {
                return -1;
            }
            if(slen > 0) {
                return write_bytes(f, d->value.s, slen);
            }
            return 0;
        default:
            return -1;
    }
}

static int write_db(FILE* f, DB* db) {
    int32_t  iv;
    int32_t  ftype;

    /* name */
    if(write_bytes(f, db->name, MAX_DB_NAME_LENGTH) != 0) {
        return -1;
    }

    /* fieldsCount */
    iv = (int32_t)db->fieldsCount;
    if(write_bytes(f, &iv, sizeof(iv)) != 0) {
        return -1;
    }

    /* nextKey */
    iv = (int32_t)db->nextKey;
    if(write_bytes(f, &iv, sizeof(iv)) != 0) {
        return -1;
    }

    /* fields */
    for(int fi = 0; fi < db->fieldsCount; fi++) {
        if(write_bytes(f, db->fields[fi].name, MAX_FIELD_NAME_LENGTH) != 0) {
            return -1;
        }
        ftype = (int32_t)db->fields[fi].type;
        if(write_bytes(f, &ftype, sizeof(ftype)) != 0) {
            return -1;
        }
    }

    /* rowsCount */
    iv = (int32_t)db->rowsCount;
    if(write_bytes(f, &iv, sizeof(iv)) != 0) {
        return -1;
    }

    /* rows */
    for(int ri = 0; ri < db->rowsCount; ri++) {
        Row* row = &db->rows[ri];
        iv       = (int32_t)row->valueCount;
        if(write_bytes(f, &iv, sizeof(iv)) != 0) {
            return -1;
        }
        for(int vi = 0; vi < row->valueCount; vi++) {
            int32_t sz = (int32_t)row->values[vi].size;
            if(write_bytes(f, &sz, sizeof(sz)) != 0) {
                return -1;
            }
            if(write_value(f, db->fields[vi].type, &row->values[vi]) != 0) {
                return -1;
            }
        }
    }

    return 0;
}

int save_db_storage(const char* filepath) {
    if(filepath == NULL) {
        return -1;
    }

    FILE* f = fopen(filepath, "wb");
    if(f == NULL) {
        perror("fopen");
        return -1;
    }

    /* Write header */
    if(write_bytes(f, POUDB_MAGIC, sizeof(POUDB_MAGIC)) != 0) {
        fclose(f);
        return -1;
    }
    uint32_t ver = POUDB_VERSION;
    if(write_bytes(f, &ver, sizeof(ver)) != 0) {
        fclose(f);
        return -1;
    }

    /* Reserve space for db_count; we will seek back and fill it in. */
    long count_offset = ftell(f);
    int32_t db_count  = 0;
    if(write_bytes(f, &db_count, sizeof(db_count)) != 0) {
        fclose(f);
        return -1;
    }

    /* Write each DB */
    SaveCtx ctx = {f, 0, 0};
    iter_db_storage(save_one_db, &ctx);

    if(ctx.error) {
        fclose(f);
        return -1;
    }

    /* Seek back and write real count */
    if(fseek(f, count_offset, SEEK_SET) != 0) {
        perror("fseek");
        fclose(f);
        return -1;
    }
    db_count = (int32_t)ctx.count;
    if(write_bytes(f, &db_count, sizeof(db_count)) != 0) {
        fclose(f);
        return -1;
    }

    fclose(f);
    return 0;
}

/* -------------------------------------------------------------------------
 * load_db_storage
 * ---------------------------------------------------------------------- */

static int read_value(FILE* f, FieldType type, Data* d) {
    int32_t  iv;
    double   dv;
    int32_t  bv;
    uint32_t slen;
    char*    sbuf;

    switch(type) {
        case TYPE_INT:
            if(read_bytes(f, &iv, sizeof(iv)) != 0) {
                return -1;
            }
            d->value.i = (int)iv;
            d->size    = sizeof(int);
            return 0;
        case TYPE_DOUBLE:
            if(read_bytes(f, &dv, sizeof(dv)) != 0) {
                return -1;
            }
            d->value.d = dv;
            d->size    = sizeof(double);
            return 0;
        case TYPE_BOOL:
            if(read_bytes(f, &bv, sizeof(bv)) != 0) {
                return -1;
            }
            d->value.b = (bv != 0);
            d->size    = sizeof(int);
            return 0;
        case TYPE_STRING:
            if(read_bytes(f, &slen, sizeof(slen)) != 0) {
                return -1;
            }
            if(slen == 0) {
                d->value.s = NULL;
                d->size    = 0;
                return 0;
            }
            sbuf = (char*)malloc(slen + 1);
            if(sbuf == NULL) {
                perror("malloc");
                return -1;
            }
            if(read_bytes(f, sbuf, slen) != 0) {
                free(sbuf);
                return -1;
            }
            sbuf[slen]  = '\0';
            d->value.s  = sbuf;
            d->size     = (int)slen;
            return 0;
        default:
            return -1;
    }
}

int load_db_storage(const char* filepath) {
    if(filepath == NULL) {
        return -1;
    }

    FILE* f = fopen(filepath, "rb");
    if(f == NULL) {
        if(errno == ENOENT) {
            /* No data file yet — that is fine */
            return 0;
        }
        perror("fopen");
        return -1;
    }

    /* Read and verify magic */
    char magic[6];
    if(read_bytes(f, magic, sizeof(magic)) != 0 ||
       memcmp(magic, POUDB_MAGIC, sizeof(POUDB_MAGIC)) != 0) {
        fprintf(stderr, "persistence: invalid or corrupt data file\n");
        fclose(f);
        return -1;
    }

    /* Version */
    uint32_t ver;
    if(read_bytes(f, &ver, sizeof(ver)) != 0 || ver != POUDB_VERSION) {
        fprintf(stderr,
                "persistence: unsupported file version %u\n",
                (unsigned)ver);
        fclose(f);
        return -1;
    }

    /* Number of databases */
    int32_t db_count;
    if(read_bytes(f, &db_count, sizeof(db_count)) != 0 || db_count < 0) {
        fprintf(stderr, "persistence: failed to read database count\n");
        fclose(f);
        return -1;
    }

    int loaded = 0;
    for(int di = 0; di < db_count; di++) {
        char    name[MAX_DB_NAME_LENGTH];
        int32_t fieldsCount;
        int32_t nextKey;

        if(read_bytes(f, name, MAX_DB_NAME_LENGTH) != 0) {
            goto read_error;
        }
        name[MAX_DB_NAME_LENGTH - 1] = '\0';

        if(read_bytes(f, &fieldsCount, sizeof(fieldsCount)) != 0 ||
           fieldsCount < 1) {
            goto read_error;
        }
        if(read_bytes(f, &nextKey, sizeof(nextKey)) != 0) {
            goto read_error;
        }

        /* Read fields (index 0 is the implicit key field) */
        Field* fields = (Field*)malloc(sizeof(Field) * (size_t)fieldsCount);
        if(fields == NULL) {
            perror("malloc");
            goto read_error;
        }
        for(int fi = 0; fi < fieldsCount; fi++) {
            int32_t ftype;
            if(read_bytes(f, fields[fi].name, MAX_FIELD_NAME_LENGTH) != 0) {
                free(fields);
                goto read_error;
            }
            fields[fi].name[MAX_FIELD_NAME_LENGTH - 1] = '\0';
            if(read_bytes(f, &ftype, sizeof(ftype)) != 0) {
                free(fields);
                goto read_error;
            }
            fields[fi].type = (FieldType)ftype;
        }

        /*
         * db_create() adds an extra key field at index 0, but we already
         * have the full fields array (including the key at [0]) from disk.
         * We pass fields[1..] (user fields only) so that db_create() adds
         * the key field back in the normal way.
         */
        int userFieldCount = fieldsCount - 1;
        DB* db = db_create(name, &fields[1], userFieldCount);
        free(fields);
        if(db == NULL) {
            fprintf(stderr,
                    "persistence: failed to recreate db '%s'\n",
                    name);
            goto read_error;
        }

        /* Read rows */
        int32_t rowsCount;
        if(read_bytes(f, &rowsCount, sizeof(rowsCount)) != 0 ||
           rowsCount < 0) {
            db_free(db);
            goto read_error;
        }

        for(int ri = 0; ri < rowsCount; ri++) {
            int32_t valueCount;
            if(read_bytes(f, &valueCount, sizeof(valueCount)) != 0 ||
               valueCount != db->fieldsCount) {
                db_free(db);
                goto read_error;
            }

            Data* vals = (Data*)malloc(sizeof(Data) * (size_t)valueCount);
            if(vals == NULL) {
                perror("malloc");
                db_free(db);
                goto read_error;
            }
            memset(vals, 0, sizeof(Data) * (size_t)valueCount);

            int read_ok = 1;
            for(int vi = 0; vi < valueCount; vi++) {
                int32_t sz;
                if(read_bytes(f, &sz, sizeof(sz)) != 0) {
                    read_ok = 0;
                    break;
                }
                if(read_value(f, db->fields[vi].type, &vals[vi]) != 0) {
                    read_ok = 0;
                    break;
                }
            }

            if(!read_ok) {
                /* Free any strings already read */
                for(int vi = 0; vi < valueCount; vi++) {
                    if(db->fields[vi].type == TYPE_STRING &&
                       vals[vi].value.s != NULL) {
                        free((void*)vals[vi].value.s);
                    }
                }
                free(vals);
                db_free(db);
                goto read_error;
            }

            /*
             * vals[0] holds the stored key (TYPE_INT).
             * db_add_row() expects (db, key, user_values, userCount).
             * Pass the actual key and user values (vals[1..]).
             */
            int row_key = vals[0].value.i;
            int rc = db_add_row(db, row_key, &vals[1], valueCount - 1);
            free(vals);   /* db_add_row copies/takes ownership of strings */
            if(rc != 0) {
                db_free(db);
                goto read_error;
            }
        }

        /* Restore nextKey – db_add_row may have advanced it */
        db->nextKey = (int)nextKey;

        if(add_db(db) != 0) {
            db_free(db);
            goto read_error;
        }
        loaded++;
    }

    fclose(f);
    return loaded;

read_error:
    fprintf(stderr, "persistence: error reading data file\n");
    fclose(f);
    return -1;
}
