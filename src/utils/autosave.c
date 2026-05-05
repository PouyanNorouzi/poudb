#include "utils/autosave.h"

#include <time.h>

#include "db/persistence.h"
#include "utils/log.h"

static long long monotonic_time_ms(void) {
    struct timespec ts;

    if(clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return -1;
    }

    return (long long)ts.tv_sec * 1000LL + (long long)ts.tv_nsec / 1000000LL;
}

int autosave_init(AutosaveState* state, long long intervalMs) {
    if(state == NULL || intervalMs <= 0) {
        return -1;
    }

    state->intervalMs = intervalMs;
    state->lastSaveMs = monotonic_time_ms();
    state->enabled    = (state->lastSaveMs != -1);

    if(!state->enabled) {
        return -1;
    }

    return 0;
}

void autosave_maybe_run(AutosaveState* state, const char* snapshotPath) {
    if(state == NULL || !state->enabled) {
        return;
    }

    long long nowMs = monotonic_time_ms();
    if(nowMs == -1) {
        log_warn("Failed to read monotonic autosave timer; autosave disabled");
        state->enabled = 0;
        return;
    }

    if(nowMs - state->lastSaveMs < state->intervalMs) {
        return;
    }

    if(persistence_save_all(snapshotPath) != 0) {
        log_warn("Failed to save periodic snapshot");
    }

    state->lastSaveMs = nowMs;
}
