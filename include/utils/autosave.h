#ifndef UTILS_AUTOSAVE_H
#define UTILS_AUTOSAVE_H

typedef struct {
    long long lastSaveMs;
    long long intervalMs;
    int       enabled;
} AutosaveState;

/**
 * Initialize periodic autosave state using a monotonic clock.
 *
 * @param state Pointer to autosave state to initialize
 * @param intervalMs Autosave interval in milliseconds
 * @return 0 on success, -1 if autosave could not be initialized
 */
int autosave_init(AutosaveState* state, long long intervalMs);

/**
 * Trigger a periodic snapshot save when the autosave interval elapses.
 *
 * @param state Pointer to autosave state
 * @param snapshotPath Snapshot destination, or NULL for default
 */
void autosave_maybe_run(AutosaveState* state, const char* snapshotPath);

#endif  // UTILS_AUTOSAVE_H
