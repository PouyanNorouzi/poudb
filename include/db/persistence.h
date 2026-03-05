#ifndef PERSISTENCE_H
#define PERSISTENCE_H

#define POUDB_DEFAULT_DATA_FILE "poudb.dat"

/**
 * Save all databases in the storage system to a file.
 *
 * @param filepath Path to the file to write (will be created or overwritten)
 * @return 0 on success, -1 on error
 */
int save_db_storage(const char* filepath);

/**
 * Load databases from a file into the storage system.
 * Must be called after init_db_storage().
 *
 * @param filepath Path to the file to read
 * @return Number of databases loaded on success, -1 on error,
 *         0 if file does not exist (treated as empty)
 */
int load_db_storage(const char* filepath);

#endif /* PERSISTENCE_H */
