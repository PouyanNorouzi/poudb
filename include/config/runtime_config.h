#ifndef CONFIG_RUNTIME_CONFIG_H
#define CONFIG_RUNTIME_CONFIG_H

#define DEFAULT_AUTOSAVE_INTERVAL_MS 30000LL
#define DEFAULT_SNAPSHOT_PATH        "poudb.snapshot"
#define RUNTIME_CONFIG_PATH_MAX      512

typedef struct {
    int       port;
    int       maxConnections;
    long long autosaveIntervalMs;
    int       autosaveEnabled;
    char      snapshotPath[RUNTIME_CONFIG_PATH_MAX];
} RuntimeConfig;

/**
 * Parse runtime settings from defaults, config file, and CLI arguments.
 * Precedence is CLI > config file > built-in defaults.
 *
 * Return values:
 *   0: success, cfg is initialized
 *   1: help printed, caller should exit success
 *  -1: invalid input or parse failure
 */
int runtime_config_init(RuntimeConfig* cfg, int argc, char* argv[]);

void runtime_config_print_usage(const char* programName);

#endif /* CONFIG_RUNTIME_CONFIG_H */
