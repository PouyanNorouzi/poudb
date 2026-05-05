#include "config/runtime_config.h"

#include <getopt.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include <ini.h>

#include "main.h"
#include "net.h"
#include "utils/log.h"
#include "utils/parse.h"

typedef struct {
    RuntimeConfig* cfg;
    int            error;
} IniContext;

static int  pre_scan_config_path(int argc, char* argv[], char* outPath, size_t outSize);
static int  load_config_file(RuntimeConfig* cfg, const char* configPath);
static int  config_ini_handler(void*       user,
                               const char* section,
                               const char* name,
                               const char* value);
static int  apply_cli_overrides(RuntimeConfig* cfg, int argc, char* argv[]);

int runtime_config_init(RuntimeConfig* cfg, int argc, char* argv[]) {
    char configPath[RUNTIME_CONFIG_PATH_MAX] = {0};

    if(cfg == NULL) {
        return -1;
    }

    cfg->port               = DEFAULT_PORT;
    cfg->maxConnections     = DEFAULT_MAX_CONNECTIONS;
    cfg->autosaveIntervalMs = DEFAULT_AUTOSAVE_INTERVAL_MS;
    cfg->autosaveEnabled    = 1;
    cfg->logLevel           = LOG_INFO;
    util_copy_string(cfg->snapshotPath,
                     sizeof(cfg->snapshotPath),
                     DEFAULT_SNAPSHOT_PATH);

    if(pre_scan_config_path(argc, argv, configPath, sizeof(configPath)) != 0) {
        return -1;
    }

    if(configPath[0] != '\0') {
        if(load_config_file(cfg, configPath) != 0) {
            return -1;
        }
    }

    return apply_cli_overrides(cfg, argc, argv);
}

void runtime_config_print_usage(const char* programName) {
    const char* prog = (programName != NULL) ? programName : "poudb";

    printf("Usage: %s [options]\n", prog);
    printf("\n");
    printf("Options:\n");
    printf("  -h, --help                    Show this help and exit\n");
    printf("  -f, --config PATH             Load INI config file\n");
    printf("  -p, --port PORT               TCP port (1-65535)\n");
    printf("  -s, --snapshot PATH           Snapshot file path\n");
    printf("  -i, --autosave-interval MS    Autosave interval in milliseconds\n");
    printf("  -m, --max-connections N       Maximum simultaneous clients\n");
    printf("  -a, --autosave on|off         Enable/disable autosave\n");
    printf("  -l, --log-level LEVEL         Log verbosity: debug/info/warn/error/silent\n");
    printf("\n");
    printf("Precedence: CLI > config file > built-in defaults\n");
}

static int pre_scan_config_path(int argc, char* argv[], char* outPath, size_t outSize) {
    int i;

    if(outPath == NULL || outSize == 0) {
        return -1;
    }

    outPath[0] = '\0';

    for(i = 1; i < argc; i++) {
        const char* arg = argv[i];

        if(strcmp(arg, "-f") == 0 || strcmp(arg, "--config") == 0) {
            if(i + 1 >= argc) {
                log_error("Missing value for %s", arg);
                return -1;
            }

            if(util_copy_string(outPath, outSize, argv[i + 1]) != 0 ||
               outPath[0] == '\0') {
                log_error("Invalid config path");
                return -1;
            }
            i++;
            continue;
        }

        if(strncmp(arg, "--config=", 9) == 0) {
            const char* value = arg + 9;
            if(util_copy_string(outPath, outSize, value) != 0 ||
               outPath[0] == '\0') {
                log_error("Invalid config path");
                return -1;
            }
        }
    }

    return 0;
}

static int load_config_file(RuntimeConfig* cfg, const char* configPath) {
    IniContext ctx;
    int        parseRc;

    if(cfg == NULL || configPath == NULL || configPath[0] == '\0') {
        return -1;
    }

    ctx.cfg   = cfg;
    ctx.error = 0;

    parseRc = ini_parse(configPath, config_ini_handler, &ctx);
    if(parseRc < 0) {
        log_error("Failed to read config file: %s", configPath);
        return -1;
    }

    if(parseRc > 0) {
        log_error("Config parse error at line %d in %s", parseRc, configPath);
        return -1;
    }

    if(ctx.error) {
        return -1;
    }

    return 0;
}

static int config_ini_handler(void*       user,
                              const char* section,
                              const char* name,
                              const char* value) {
    IniContext*   ctx = (IniContext*)user;
    RuntimeConfig* cfg;
    int            parsedInt;
    long long      parsedLong;

    if(ctx == NULL || ctx->cfg == NULL || section == NULL || name == NULL || value == NULL) {
        return 0;
    }

    cfg = ctx->cfg;

    if(strcmp(section, "server") == 0 && strcmp(name, "port") == 0) {
        if(util_parse_int_in_range(value, 1, 65535, &parsedInt) != 0) {
            log_error("Invalid config value server.port: %s", value);
            ctx->error = 1;
        } else {
            cfg->port = parsedInt;
        }
        return 1;
    }

    if(strcmp(section, "server") == 0 && strcmp(name, "max_connections") == 0) {
        if(util_parse_int_in_range(value, 1, INT_MAX, &parsedInt) != 0) {
            log_error("Invalid config value server.max_connections: %s", value);
            ctx->error = 1;
        } else {
            cfg->maxConnections = parsedInt;
        }
        return 1;
    }

    if(strcmp(section, "storage") == 0 && strcmp(name, "snapshot_path") == 0) {
        if(util_copy_string(cfg->snapshotPath,
                            sizeof(cfg->snapshotPath),
                            value) != 0 ||
           cfg->snapshotPath[0] == '\0') {
            log_error("Invalid config value storage.snapshot_path");
            ctx->error = 1;
        }
        return 1;
    }

    if(strcmp(section, "autosave") == 0 && strcmp(name, "enabled") == 0) {
        if(util_parse_bool(value, &parsedInt) != 0) {
            log_error("Invalid config value autosave.enabled: %s", value);
            ctx->error = 1;
        } else {
            cfg->autosaveEnabled = parsedInt;
        }
        return 1;
    }

    if(strcmp(section, "autosave") == 0 && strcmp(name, "interval_ms") == 0) {
        if(util_parse_positive_ll(value, &parsedLong) != 0) {
            log_error("Invalid config value autosave.interval_ms: %s", value);
            ctx->error = 1;
        } else {
            cfg->autosaveIntervalMs = parsedLong;
        }
        return 1;
    }

    if(strcmp(section, "logging") == 0 && strcmp(name, "level") == 0) {
        cfg->logLevel = log_level_from_string(value);
        return 1;
    }

    log_warn("Ignoring unknown config key %s.%s", section, name);
    return 1;
}

static int apply_cli_overrides(RuntimeConfig* cfg, int argc, char* argv[]) {
    int option;
    int parsedInt;
    int parsedBool;
    long long parsedLong;

    static const struct option longOptions[] = {
        {"help", no_argument, NULL, 'h'},
        {"config", required_argument, NULL, 'f'},
        {"port", required_argument, NULL, 'p'},
        {"snapshot", required_argument, NULL, 's'},
        {"autosave-interval", required_argument, NULL, 'i'},
        {"max-connections", required_argument, NULL, 'm'},
        {"autosave", required_argument, NULL, 'a'},
        {"log-level", required_argument, NULL, 'l'},
        {0, 0, 0, 0}};

    if(cfg == NULL) {
        return -1;
    }

    optind = 1;
    while((option = getopt_long(argc, argv, "hf:p:s:i:m:a:l:", longOptions, NULL)) != -1) {
        switch(option) {
            case 'h':
                runtime_config_print_usage(argv[0]);
                return 1;
            case 'f':
                /* Already handled in pre-scan so precedence is stable. */
                break;
            case 'p':
                if(util_parse_int_in_range(optarg, 1, 65535, &parsedInt) != 0) {
                    log_error("Invalid --port value: %s", optarg);
                    return -1;
                }
                cfg->port = parsedInt;
                break;
            case 's':
                if(util_copy_string(cfg->snapshotPath,
                                    sizeof(cfg->snapshotPath),
                                    optarg) != 0 ||
                   cfg->snapshotPath[0] == '\0') {
                    log_error("Invalid --snapshot value");
                    return -1;
                }
                break;
            case 'i':
                if(util_parse_positive_ll(optarg, &parsedLong) != 0) {
                    log_error("Invalid --autosave-interval value: %s", optarg);
                    return -1;
                }
                cfg->autosaveIntervalMs = parsedLong;
                break;
            case 'm':
                if(util_parse_int_in_range(optarg, 1, INT_MAX, &parsedInt) != 0) {
                    log_error("Invalid --max-connections value: %s", optarg);
                    return -1;
                }
                cfg->maxConnections = parsedInt;
                break;
            case 'a':
                if(util_parse_bool(optarg, &parsedBool) != 0) {
                    log_error("Invalid --autosave value: %s", optarg);
                    return -1;
                }
                cfg->autosaveEnabled = parsedBool;
                break;
            case 'l':
                cfg->logLevel = log_level_from_string(optarg);
                break;
            default:
                runtime_config_print_usage(argv[0]);
                return -1;
        }
    }

    if(optind < argc) {
        log_error("Unknown positional argument: %s", argv[optind]);
        return -1;
    }

    return 0;
}
