#include <criterion/criterion.h>
#include <criterion/new/assert.h>

#include "config/runtime_config.h"
#include "main.h"
#include "net.h"

static void assert_default_config(const RuntimeConfig* cfg) {
    cr_assert_not_null(cfg);
    cr_assert_eq(cfg->port, DEFAULT_PORT);
    cr_assert_eq(cfg->maxConnections, DEFAULT_MAX_CONNECTIONS);
    cr_assert_eq(cfg->autosaveIntervalMs, DEFAULT_AUTOSAVE_INTERVAL_MS);
    cr_assert_eq(cfg->autosaveEnabled, 1);
    cr_assert_str_eq(cfg->snapshotPath, DEFAULT_SNAPSHOT_PATH);
}

Test(runtime_config, defaults_used_without_options) {
    RuntimeConfig cfg;
    char          arg0[] = "poudb";
    char*         argv[] = {arg0, NULL};

    cr_assert_eq(runtime_config_init(&cfg, 1, argv), 0);
    assert_default_config(&cfg);
}

Test(runtime_config, valid_config_file_is_loaded) {
    RuntimeConfig cfg;
    char          arg0[] = "poudb";
    char          arg1[] = "--config";
    char          arg2[] = "test/configs/valid.conf";
    char*         argv[] = {arg0, arg1, arg2, NULL};

    cr_assert_eq(runtime_config_init(&cfg, 3, argv), 0);
    cr_assert_eq(cfg.port, 4020);
    cr_assert_eq(cfg.maxConnections, 11);
    cr_assert_eq(cfg.autosaveIntervalMs, 45000LL);
    cr_assert_eq(cfg.autosaveEnabled, 0);
    cr_assert_str_eq(cfg.snapshotPath, "test/configs/custom.snapshot");
}

Test(runtime_config, config_path_equals_form_is_supported) {
    RuntimeConfig cfg;
    char          arg0[] = "poudb";
    char          arg1[] = "--config=test/configs/valid.conf";
    char*         argv[] = {arg0, arg1, NULL};

    cr_assert_eq(runtime_config_init(&cfg, 2, argv), 0);
    cr_assert_eq(cfg.port, 4020);
    cr_assert_eq(cfg.maxConnections, 11);
}

Test(runtime_config, cli_overrides_config_values) {
    RuntimeConfig cfg;
    char          arg0[]  = "poudb";
    char          arg1[]  = "--config";
    char          arg2[]  = "test/configs/valid.conf";
    char          arg3[]  = "--port";
    char          arg4[]  = "7777";
    char          arg5[]  = "--snapshot";
    char          arg6[]  = "override.snapshot";
    char          arg7[]  = "--autosave-interval";
    char          arg8[]  = "12345";
    char          arg9[]  = "--max-connections";
    char          arg10[] = "3";
    char          arg11[] = "--autosave";
    char          arg12[] = "on";
    char*         argv[]  = {arg0,
                            arg1,
                            arg2,
                            arg3,
                            arg4,
                            arg5,
                            arg6,
                            arg7,
                            arg8,
                            arg9,
                            arg10,
                            arg11,
                            arg12,
                            NULL};

    cr_assert_eq(runtime_config_init(&cfg, 13, argv), 0);
    cr_assert_eq(cfg.port, 7777);
    cr_assert_eq(cfg.maxConnections, 3);
    cr_assert_eq(cfg.autosaveIntervalMs, 12345LL);
    cr_assert_eq(cfg.autosaveEnabled, 1);
    cr_assert_str_eq(cfg.snapshotPath, "override.snapshot");
}

Test(runtime_config, help_returns_success_signal) {
    RuntimeConfig cfg;
    char          arg0[] = "poudb";
    char          arg1[] = "--help";
    char*         argv[] = {arg0, arg1, NULL};

    cr_assert_eq(runtime_config_init(&cfg, 2, argv), 1);
}

Test(runtime_config, missing_config_argument_fails) {
    RuntimeConfig cfg;
    char          arg0[] = "poudb";
    char          arg1[] = "--config";
    char*         argv[] = {arg0, arg1, NULL};

    cr_assert_eq(runtime_config_init(&cfg, 2, argv), -1);
}

Test(runtime_config, invalid_cli_port_fails) {
    RuntimeConfig cfg;
    char          arg0[] = "poudb";
    char          arg1[] = "--port";
    char          arg2[] = "70000";
    char*         argv[] = {arg0, arg1, arg2, NULL};

    cr_assert_eq(runtime_config_init(&cfg, 3, argv), -1);
}

Test(runtime_config, invalid_cli_max_connections_fails) {
    RuntimeConfig cfg;
    char          arg0[] = "poudb";
    char          arg1[] = "--max-connections";
    char          arg2[] = "0";
    char*         argv[] = {arg0, arg1, arg2, NULL};

    cr_assert_eq(runtime_config_init(&cfg, 3, argv), -1);
}

Test(runtime_config, invalid_cli_autosave_interval_fails) {
    RuntimeConfig cfg;
    char          arg0[] = "poudb";
    char          arg1[] = "--autosave-interval";
    char          arg2[] = "0";
    char*         argv[] = {arg0, arg1, arg2, NULL};

    cr_assert_eq(runtime_config_init(&cfg, 3, argv), -1);
}

Test(runtime_config, invalid_cli_autosave_value_fails) {
    RuntimeConfig cfg;
    char          arg0[] = "poudb";
    char          arg1[] = "--autosave";
    char          arg2[] = "maybe";
    char*         argv[] = {arg0, arg1, arg2, NULL};

    cr_assert_eq(runtime_config_init(&cfg, 3, argv), -1);
}

Test(runtime_config, empty_cli_snapshot_fails) {
    RuntimeConfig cfg;
    char          arg0[] = "poudb";
    char          arg1[] = "--snapshot";
    char          arg2[] = "";
    char*         argv[] = {arg0, arg1, arg2, NULL};

    cr_assert_eq(runtime_config_init(&cfg, 3, argv), -1);
}

Test(runtime_config, missing_config_file_fails) {
    RuntimeConfig cfg;
    char          arg0[] = "poudb";
    char          arg1[] = "--config";
    char          arg2[] = "test/configs/does_not_exist.conf";
    char*         argv[] = {arg0, arg1, arg2, NULL};

    cr_assert_eq(runtime_config_init(&cfg, 3, argv), -1);
}

Test(runtime_config, invalid_port_in_config_fails) {
    RuntimeConfig cfg;
    char          arg0[] = "poudb";
    char          arg1[] = "--config";
    char          arg2[] = "test/configs/invalid_port.conf";
    char*         argv[] = {arg0, arg1, arg2, NULL};

    cr_assert_eq(runtime_config_init(&cfg, 3, argv), -1);
}

Test(runtime_config, invalid_bool_in_config_fails) {
    RuntimeConfig cfg;
    char          arg0[] = "poudb";
    char          arg1[] = "--config";
    char          arg2[] = "test/configs/invalid_bool.conf";
    char*         argv[] = {arg0, arg1, arg2, NULL};

    cr_assert_eq(runtime_config_init(&cfg, 3, argv), -1);
}

Test(runtime_config, invalid_max_connections_in_config_fails) {
    RuntimeConfig cfg;
    char          arg0[] = "poudb";
    char          arg1[] = "--config";
    char          arg2[] = "test/configs/invalid_max_connections.conf";
    char*         argv[] = {arg0, arg1, arg2, NULL};

    cr_assert_eq(runtime_config_init(&cfg, 3, argv), -1);
}

Test(runtime_config, invalid_autosave_interval_in_config_fails) {
    RuntimeConfig cfg;
    char          arg0[] = "poudb";
    char          arg1[] = "--config";
    char          arg2[] = "test/configs/invalid_interval.conf";
    char*         argv[] = {arg0, arg1, arg2, NULL};

    cr_assert_eq(runtime_config_init(&cfg, 3, argv), -1);
}

Test(runtime_config, empty_snapshot_path_in_config_fails) {
    RuntimeConfig cfg;
    char          arg0[] = "poudb";
    char          arg1[] = "--config";
    char          arg2[] = "test/configs/invalid_snapshot.conf";
    char*         argv[] = {arg0, arg1, arg2, NULL};

    cr_assert_eq(runtime_config_init(&cfg, 3, argv), -1);
}

Test(runtime_config, malformed_config_fails) {
    RuntimeConfig cfg;
    char          arg0[] = "poudb";
    char          arg1[] = "--config";
    char          arg2[] = "test/configs/malformed.conf";
    char*         argv[] = {arg0, arg1, arg2, NULL};

    cr_assert_eq(runtime_config_init(&cfg, 3, argv), -1);
}

Test(runtime_config, unknown_keys_are_ignored) {
    RuntimeConfig cfg;
    char          arg0[] = "poudb";
    char          arg1[] = "--config";
    char          arg2[] = "test/configs/unknown_keys.conf";
    char*         argv[] = {arg0, arg1, arg2, NULL};

    cr_assert_eq(runtime_config_init(&cfg, 3, argv), 0);
    cr_assert_eq(cfg.port, 4500);
    cr_assert_eq(cfg.maxConnections, 9);
    cr_assert_eq(cfg.autosaveEnabled, 1);
    cr_assert_eq(cfg.autosaveIntervalMs, 60000LL);
    cr_assert_str_eq(cfg.snapshotPath, "test/configs/unknown.snapshot");
}