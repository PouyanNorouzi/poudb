#include "db/persistence.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "db/data_model.h"

#define SNAPSHOT_MAGIC       "PDBSNAP"
#define SNAPSHOT_MAGIC_SIZE  7
#define SNAPSHOT_VERSION     1U
#define NULL_STRING_SENTINEL UINT32_MAX
#define DEFAULT_SNAPSHOT     "poudb.snapshot"

typedef struct {
    FILE* fp;
} SaveCtx;

typedef struct {
    uint32_t count;
} CountCtx;

/* Resolve caller-provided path or return default snapshot file. */
static const char* effective_path(const char* snapshotPath);
/* Primitive binary writers used by snapshot encoding. */
static int write_u8(FILE* fp, uint8_t v);
static int write_u16(FILE* fp, uint16_t v);
static int write_u32(FILE* fp, uint32_t v);
static int write_i32(FILE* fp, int32_t v);
static int write_f64(FILE* fp, double v);
/* Primitive binary readers used by snapshot decoding. */
static int read_u8(FILE* fp, uint8_t* out);
static int read_u16(FILE* fp, uint16_t* out);
static int read_u32(FILE* fp, uint32_t* out);
static int read_i32(FILE* fp, int32_t* out);
static int read_f64(FILE* fp, double* out);
/* String serialization helpers (with explicit NULL sentinel support). */
static int   write_string(FILE* fp, const char* s);
static char* read_string(FILE* fp);
/* Row/database serialization callbacks used by db_for_each. */
static int write_row_values(FILE* fp, DB* db, Row* row);
static int write_db_callback(DB* db, void* ctx);
static int count_db_callback(DB* db, void* ctx);
/* Schema and row decoding helpers for restore path. */
static int  read_field_definitions(FILE*    fp,
                                   Field**  fieldsOut,
                                   uint32_t fieldCount);
static void free_pending_values(Data*    values,
                                Field*   fields,
                                uint32_t fieldCount,
                                uint32_t readCount);
static int  read_row_values(FILE*    fp,
                            DB*      db,
                            uint32_t fieldCount,
                            Data*    valuesOut);

/**
 * Persist all in-memory databases to disk using an atomic replace strategy.
 */
int persistence_save_all(const char* snapshotPath) {
    /* Resolve effective snapshot file path (caller-provided or default). */
    const char* path = effective_path(snapshotPath);

    /* Use temporary file + rename pattern for atomic writes. */
    char tmpPath[512];
    if(snprintf(tmpPath, sizeof(tmpPath), "%s.tmp", path) >=
       (int)sizeof(tmpPath)) {
        return -1;
    }

    /* Open temporary file for binary writing. */
    FILE* fp = fopen(tmpPath, "wb");
    if(fp == NULL) {
        return -1;
    }

    /* Write snapshot magic (format identifier and version check). */
    if(fwrite(SNAPSHOT_MAGIC, 1, SNAPSHOT_MAGIC_SIZE, fp) !=
       SNAPSHOT_MAGIC_SIZE) {
        fclose(fp);
        remove(tmpPath);
        return -1;
    }

    if(write_u32(fp, SNAPSHOT_VERSION) != 0) {
        fclose(fp);
        remove(tmpPath);
        return -1;
    }

    /* Count total databases before encoding (for validation on load). */
    CountCtx countCtx = {0};
    if(db_for_each(count_db_callback, &countCtx) != 0) {
        fclose(fp);
        remove(tmpPath);
        return -1;
    }

    if(write_u32(fp, countCtx.count) != 0) {
        fclose(fp);
        remove(tmpPath);
        return -1;
    }

    /* Iterate all databases and encode schema + rows. */
    SaveCtx saveCtx = {.fp = fp};
    if(db_for_each(write_db_callback, &saveCtx) != 0) {
        fclose(fp);
        remove(tmpPath);
        return -1;
    }

    /* Ensure all buffered data is written to disk. */
    if(fflush(fp) != 0) {
        fclose(fp);
        remove(tmpPath);
        return -1;
    }

    /* Close file handle. */
    if(fclose(fp) != 0) {
        remove(tmpPath);
        return -1;
    }

    /* Atomically replace old snapshot with new snapshot (or create if missing).
     */
    if(rename(tmpPath, path) != 0) {
        remove(tmpPath);
        return -1;
    }

    return 0;
}

/**
 * Load all databases from a snapshot file and rebuild in-memory state.
 */
int persistence_load_all(const char* snapshotPath) {
    /* Resolve effective snapshot file path. */
    const char* path = effective_path(snapshotPath);

    /* Open snapshot for binary reading (missing file is not an error). */
    FILE* fp = fopen(path, "rb");
    if(fp == NULL) {
        if(errno == ENOENT) {
            return 0; /* No snapshot yet; start with empty state. */
        }
        return -1;
    }

    /* Verify snapshot format identifier and version. */
    char magic[SNAPSHOT_MAGIC_SIZE];
    if(fread(magic, 1, SNAPSHOT_MAGIC_SIZE, fp) != SNAPSHOT_MAGIC_SIZE ||
       memcmp(magic, SNAPSHOT_MAGIC, SNAPSHOT_MAGIC_SIZE) != 0) {
        fclose(fp);
        return -1; /* Invalid or corrupted snapshot file. */
    }

    uint32_t version;
    if(read_u32(fp, &version) != 0 || version != SNAPSHOT_VERSION) {
        fclose(fp);
        return -1; /* Version mismatch; cannot load. */
    }

    /* Read database count. */
    uint32_t dbCount;
    if(read_u32(fp, &dbCount) != 0) {
        fclose(fp);
        return -1;
    }

    /* Reconstruct each database from snapshot. */
    for(uint32_t dbIdx = 0; dbIdx < dbCount; dbIdx++) {
        /* Read database name. */
        uint16_t dbNameLen;
        if(read_u16(fp, &dbNameLen) != 0 || dbNameLen >= MAX_DB_NAME_LENGTH) {
            fclose(fp);
            free_db_storage();
            init_db_storage();
            return -1;
        }

        char dbName[MAX_DB_NAME_LENGTH];
        if(dbNameLen > 0 && fread(dbName, 1, dbNameLen, fp) != dbNameLen) {
            fclose(fp);
            free_db_storage();
            init_db_storage();
            return -1;
        }
        dbName[dbNameLen] = '\0';

        /* Read field count and schema. */
        uint32_t fieldCount;
        if(read_u32(fp, &fieldCount) != 0 || fieldCount == 0) {
            fclose(fp);
            free_db_storage();
            init_db_storage();
            return -1;
        }

        Field* fields = NULL;
        if(read_field_definitions(fp, &fields, fieldCount) != 0) {
            fclose(fp);
            free_db_storage();
            init_db_storage();
            return -1;
        }

        /* Recreate database with schema and register in storage. */
        DB* db = db_create(dbName, fields, (int)fieldCount);
        if(db == NULL || add_db(db) != 0) {
            if(db != NULL) {
                db_free(db);
            }
            free(fields);
            fclose(fp);
            free_db_storage();
            init_db_storage();
            return -1;
        }

        /* Read and restore row count. */
        uint32_t rowCount;
        if(read_u32(fp, &rowCount) != 0) {
            fclose(fp);
            free_db_storage();
            init_db_storage();
            return -1;
        }

        /* Restore each row. */
        for(uint32_t rowIdx = 0; rowIdx < rowCount; rowIdx++) {
            /* Read key and typed field values. */
            int32_t key;
            if(read_i32(fp, &key) != 0) {
                fclose(fp);
                free_db_storage();
                init_db_storage();
                return -1;
            }

            Data* values = (Data*)calloc((size_t)fieldCount, sizeof(Data));
            if(values == NULL) {
                fclose(fp);
                free_db_storage();
                init_db_storage();
                return -1;
            }

            /* Decode row field values. */
            if(read_row_values(fp, db, fieldCount, values) != 0) {
                free(values);
                free(fields);
                fclose(fp);
                free_db_storage();
                init_db_storage();
                return -1;
            }

            /* Insert reconstructed row into in-memory database. */
            int addRc = db_add_row(db, (int)key, values, (int)fieldCount);
            if(addRc != 0) {
                free_pending_values(values,
                                    &db->fields[1],
                                    fieldCount,
                                    fieldCount);
                free(values);
                free(fields);
                fclose(fp);
                free_db_storage();
                init_db_storage();
                return -1;
            }

            free(values);
        }

        free(fields);
    }

    /* Snapshot loaded successfully. */
    fclose(fp);
    return 0;
}

/* Resolve caller-provided path or return default snapshot file. */
static const char* effective_path(const char* snapshotPath) {
    if(snapshotPath != NULL && snapshotPath[0] != '\0') {
        return snapshotPath;
    }
    return DEFAULT_SNAPSHOT;
}

/* Primitive binary writer for uint8_t. */
static int write_u8(FILE* fp, uint8_t v) {
    return fwrite(&v, sizeof(v), 1, fp) == 1 ? 0 : -1;
}

/* Primitive binary writer for uint16_t. */
static int write_u16(FILE* fp, uint16_t v) {
    return fwrite(&v, sizeof(v), 1, fp) == 1 ? 0 : -1;
}

/* Primitive binary writer for uint32_t. */
static int write_u32(FILE* fp, uint32_t v) {
    return fwrite(&v, sizeof(v), 1, fp) == 1 ? 0 : -1;
}

/* Primitive binary writer for int32_t. */
static int write_i32(FILE* fp, int32_t v) {
    return fwrite(&v, sizeof(v), 1, fp) == 1 ? 0 : -1;
}

/* Primitive binary writer for double. */
static int write_f64(FILE* fp, double v) {
    return fwrite(&v, sizeof(v), 1, fp) == 1 ? 0 : -1;
}

/* Primitive binary reader for uint8_t. */
static int read_u8(FILE* fp, uint8_t* out) {
    return fread(out, sizeof(*out), 1, fp) == 1 ? 0 : -1;
}

/* Primitive binary reader for uint16_t. */
static int read_u16(FILE* fp, uint16_t* out) {
    return fread(out, sizeof(*out), 1, fp) == 1 ? 0 : -1;
}

/* Primitive binary reader for uint32_t. */
static int read_u32(FILE* fp, uint32_t* out) {
    return fread(out, sizeof(*out), 1, fp) == 1 ? 0 : -1;
}

/* Primitive binary reader for int32_t. */
static int read_i32(FILE* fp, int32_t* out) {
    return fread(out, sizeof(*out), 1, fp) == 1 ? 0 : -1;
}

/* Primitive binary reader for double. */
static int read_f64(FILE* fp, double* out) {
    return fread(out, sizeof(*out), 1, fp) == 1 ? 0 : -1;
}

/* Serialize nullable string using length prefix plus NULL sentinel. */
static int write_string(FILE* fp, const char* s) {
    if(s == NULL) {
        return write_u32(fp, NULL_STRING_SENTINEL);
    }

    size_t len = strlen(s);
    if(len > UINT32_MAX) {
        return -1;
    }

    if(write_u32(fp, (uint32_t)len) != 0) {
        return -1;
    }

    if(len > 0 && fwrite(s, 1, len, fp) != len) {
        return -1;
    }

    return 0;
}

/* Deserialize nullable string using length prefix plus NULL sentinel. */
static char* read_string(FILE* fp) {
    uint32_t len;
    if(read_u32(fp, &len) != 0) {
        return NULL;
    }

    if(len == NULL_STRING_SENTINEL) {
        return (char*)-1;
    }

    char* s = (char*)malloc((size_t)len + 1);
    if(s == NULL) {
        return NULL;
    }

    if(len > 0 && fread(s, 1, len, fp) != len) {
        free(s);
        return NULL;
    }

    s[len] = '\0';
    return s;
}

/* Write a row payload excluding key (key is written by caller). */
static int write_row_values(FILE* fp, DB* db, Row* row) {
    for(int i = 1; i < db->fieldsCount; i++) {
        FieldType type = db->fields[i].type;
        Data*     v    = &row->values[i];

        switch(type) {
            case TYPE_INT:
                if(write_i32(fp, (int32_t)v->value.i) != 0) {
                    return -1;
                }
                break;
            case TYPE_DOUBLE:
                if(write_f64(fp, v->value.d) != 0) {
                    return -1;
                }
                break;
            case TYPE_BOOL:
                if(write_u8(fp, (uint8_t)(v->value.b ? 1 : 0)) != 0) {
                    return -1;
                }
                break;
            case TYPE_STRING:
                if(write_string(fp, v->value.s) != 0) {
                    return -1;
                }
                break;
            default: return -1;
        }
    }

    return 0;
}

/* Serialize a single DB schema and all rows for db_for_each. */
static int write_db_callback(DB* db, void* ctx) {
    SaveCtx* saveCtx = (SaveCtx*)ctx;
    FILE*    fp      = saveCtx->fp;

    uint16_t dbNameLen = (uint16_t)strnlen(db->name, MAX_DB_NAME_LENGTH);
    if(write_u16(fp, dbNameLen) != 0) {
        return -1;
    }
    if(dbNameLen > 0 && fwrite(db->name, 1, dbNameLen, fp) != dbNameLen) {
        return -1;
    }

    uint32_t userFieldCount = (uint32_t)(db->fieldsCount - 1);
    if(write_u32(fp, userFieldCount) != 0) {
        return -1;
    }

    for(int i = 1; i < db->fieldsCount; i++) {
        uint16_t fieldNameLen =
            (uint16_t)strnlen(db->fields[i].name, MAX_FIELD_NAME_LENGTH);
        if(write_u16(fp, fieldNameLen) != 0) {
            return -1;
        }
        if(fieldNameLen > 0 &&
           fwrite(db->fields[i].name, 1, fieldNameLen, fp) != fieldNameLen) {
            return -1;
        }
        if(write_u8(fp, (uint8_t)db->fields[i].type) != 0) {
            return -1;
        }
    }

    if(write_u32(fp, (uint32_t)db->rowsCount) != 0) {
        return -1;
    }

    DBRowIterator it;
    Row*          row = db_iter_first(db, &it);
    while(row != NULL) {
        int32_t key = (int32_t)row->values[0].value.i;
        if(write_i32(fp, key) != 0) {
            return -1;
        }
        if(write_row_values(fp, db, row) != 0) {
            return -1;
        }
        row = db_iter_next(db, &it);
    }

    return 0;
}

/* Count databases in memory via db_for_each callback. */
static int count_db_callback(DB* db, void* ctx) {
    (void)db;
    CountCtx* countCtx = (CountCtx*)ctx;
    countCtx->count++;
    return 0;
}

/* Read field definitions for one DB schema from snapshot. */
static int read_field_definitions(FILE*    fp,
                                  Field**  fieldsOut,
                                  uint32_t fieldCount) {
    Field* fields = (Field*)malloc(sizeof(Field) * (size_t)fieldCount);
    if(fields == NULL) {
        return -1;
    }

    for(uint32_t i = 0; i < fieldCount; i++) {
        uint16_t fieldLen;
        uint8_t  type;

        if(read_u16(fp, &fieldLen) != 0 || fieldLen >= MAX_FIELD_NAME_LENGTH) {
            free(fields);
            return -1;
        }

        if(fieldLen > 0 && fread(fields[i].name, 1, fieldLen, fp) != fieldLen) {
            free(fields);
            return -1;
        }
        fields[i].name[fieldLen] = '\0';

        if(read_u8(fp, &type) != 0 || type > TYPE_STRING) {
            free(fields);
            return -1;
        }
        fields[i].type = (FieldType)type;
    }

    *fieldsOut = fields;
    return 0;
}

/* Free already-decoded string values when row decoding fails mid-way. */
static void free_pending_values(Data*    values,
                                Field*   fields,
                                uint32_t fieldCount,
                                uint32_t readCount) {
    (void)fieldCount;
    for(uint32_t i = 0; i < readCount; i++) {
        if(fields[i].type == TYPE_STRING && values[i].value.s != NULL) {
            free((void*)values[i].value.s);
        }
    }
}

/* Decode one row payload (excluding key) according to DB field types. */
static int read_row_values(FILE*    fp,
                           DB*      db,
                           uint32_t fieldCount,
                           Data*    valuesOut) {
    for(uint32_t i = 0; i < fieldCount; i++) {
        FieldType type = db->fields[i + 1].type;
        switch(type) {
            case TYPE_INT: {
                int32_t v;
                if(read_i32(fp, &v) != 0) {
                    free_pending_values(valuesOut,
                                        &db->fields[1],
                                        fieldCount,
                                        i);
                    return -1;
                }
                valuesOut[i].size    = sizeof(int);
                valuesOut[i].value.i = (int)v;
                break;
            }
            case TYPE_DOUBLE: {
                double v;
                if(read_f64(fp, &v) != 0) {
                    free_pending_values(valuesOut,
                                        &db->fields[1],
                                        fieldCount,
                                        i);
                    return -1;
                }
                valuesOut[i].size    = sizeof(double);
                valuesOut[i].value.d = v;
                break;
            }
            case TYPE_BOOL: {
                uint8_t v;
                if(read_u8(fp, &v) != 0) {
                    free_pending_values(valuesOut,
                                        &db->fields[1],
                                        fieldCount,
                                        i);
                    return -1;
                }
                valuesOut[i].size    = sizeof(uint8_t);
                valuesOut[i].value.b = (v != 0);
                break;
            }
            case TYPE_STRING: {
                char* s = read_string(fp);
                if(s == NULL) {
                    free_pending_values(valuesOut,
                                        &db->fields[1],
                                        fieldCount,
                                        i);
                    return -1;
                }
                if(s == (char*)-1) {
                    valuesOut[i].size    = 0;
                    valuesOut[i].value.s = NULL;
                } else {
                    valuesOut[i].size    = (int)strlen(s);
                    valuesOut[i].value.s = s;
                }
                break;
            }
            default:
                free_pending_values(valuesOut, &db->fields[1], fieldCount, i);
                return -1;
        }
    }

    return 0;
}
