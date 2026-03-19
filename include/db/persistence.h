#ifndef PERSISTENCE_H
#define PERSISTENCE_H

/**
 * Save all in-memory databases to a snapshot file.
 *
 * @param snapshotPath Destination file path, or NULL for default.
 * @return 0 on success, -1 on failure.
 */
int persistence_save_all(const char* snapshotPath);

/**
 * Load all databases from a snapshot file into in-memory storage.
 *
 * @param snapshotPath Source file path, or NULL for default.
 * @return 0 on success, -1 on failure.
 */
int persistence_load_all(const char* snapshotPath);

#endif /* PERSISTENCE_H */
