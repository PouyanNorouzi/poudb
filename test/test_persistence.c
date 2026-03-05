#include <criterion/criterion.h>
#include <criterion/new/assert.h>
#include <stdio.h>
#include <string.h>

#include "db/data_model.h"
#include "db/operations.h"
#include "db/parser.h"
#include "db/persistence.h"

#define TEST_DATA_FILE "/tmp/poudb_test_persist.dat"

static void setup(void) {
    init_db_storage();
    remove(TEST_DATA_FILE);
}

static void teardown(void) {
    free_db_storage();
    remove(TEST_DATA_FILE);
}

/* -------------------------------------------------------------------------
 * Helper: create and populate a "users" database
 * ---------------------------------------------------------------------- */
static void create_users_db(void) {
    Command* cmd =
        parse_command("CREATE users (string name, int age, bool active)");
    cr_assert_not_null(cmd);
    CommandResult* res = execute_command(cmd);
    cr_assert_not_null(res);
    cr_assert_eq(res->code, 3);
    free_command_result(res);
    free_command(cmd, 0);

    cmd = parse_command("ADD users * (\"Alice\", 30, true)");
    cr_assert_not_null(cmd);
    res = execute_command(cmd);
    cr_assert_not_null(res);
    cr_assert(res->code > 0);
    free_command_result(res);
    free_command(cmd, 1);

    cmd = parse_command("ADD users * (\"Bob\", 25, false)");
    cr_assert_not_null(cmd);
    res = execute_command(cmd);
    cr_assert_not_null(res);
    cr_assert(res->code > 0);
    free_command_result(res);
    free_command(cmd, 1);
}

/* =========================================================================
 * Test: save creates the file
 * ====================================================================== */
TestSuite(persistence_save, .init = setup, .fini = teardown);

Test(persistence_save, file_is_created) {
    create_users_db();
    int rc = save_db_storage(TEST_DATA_FILE);
    cr_assert_eq(rc, 0);

    FILE* f = fopen(TEST_DATA_FILE, "rb");
    cr_assert_not_null(f);
    fclose(f);
}

Test(persistence_save, null_filepath_returns_error) {
    int rc = save_db_storage(NULL);
    cr_assert_eq(rc, -1);
}

/* =========================================================================
 * Test: save + load round-trip
 * ====================================================================== */
TestSuite(persistence_roundtrip, .init = setup, .fini = teardown);

Test(persistence_roundtrip, empty_storage) {
    int rc = save_db_storage(TEST_DATA_FILE);
    cr_assert_eq(rc, 0);

    free_db_storage();
    init_db_storage();

    int loaded = load_db_storage(TEST_DATA_FILE);
    cr_assert_eq(loaded, 0);
}

Test(persistence_roundtrip, single_db_schema) {
    create_users_db();
    cr_assert_eq(save_db_storage(TEST_DATA_FILE), 0);

    free_db_storage();
    init_db_storage();

    int loaded = load_db_storage(TEST_DATA_FILE);
    cr_assert_eq(loaded, 1);

    DB* db = find_db("users");
    cr_assert_not_null(db);
    /* key + name + age + active */
    cr_assert_eq(db->fieldsCount, 4);
    cr_assert_str_eq(db->fields[0].name, "key");
    cr_assert_eq(db->fields[0].type, TYPE_INT);
    cr_assert_str_eq(db->fields[1].name, "name");
    cr_assert_eq(db->fields[1].type, TYPE_STRING);
    cr_assert_str_eq(db->fields[2].name, "age");
    cr_assert_eq(db->fields[2].type, TYPE_INT);
    cr_assert_str_eq(db->fields[3].name, "active");
    cr_assert_eq(db->fields[3].type, TYPE_BOOL);
}

Test(persistence_roundtrip, row_count_preserved) {
    create_users_db();
    cr_assert_eq(save_db_storage(TEST_DATA_FILE), 0);

    free_db_storage();
    init_db_storage();

    cr_assert_eq(load_db_storage(TEST_DATA_FILE), 1);

    DB* db = find_db("users");
    cr_assert_not_null(db);
    cr_assert_eq(db->rowsCount, 2);
}

Test(persistence_roundtrip, row_values_preserved) {
    create_users_db();
    cr_assert_eq(save_db_storage(TEST_DATA_FILE), 0);

    free_db_storage();
    init_db_storage();

    cr_assert_eq(load_db_storage(TEST_DATA_FILE), 1);

    DB* db = find_db("users");
    cr_assert_not_null(db);

    /* Row with key=1 should be Alice, 30, true */
    Row* row = db_get_row(db, 1);
    cr_assert_not_null(row);
    cr_assert_eq(row->values[0].value.i, 1);
    cr_assert_str_eq(row->values[1].value.s, "Alice");
    cr_assert_eq(row->values[2].value.i, 30);
    cr_assert_eq(row->values[3].value.b, true);
    db_free_row(db, row);

    /* Row with key=2 should be Bob, 25, false */
    row = db_get_row(db, 2);
    cr_assert_not_null(row);
    cr_assert_eq(row->values[0].value.i, 2);
    cr_assert_str_eq(row->values[1].value.s, "Bob");
    cr_assert_eq(row->values[2].value.i, 25);
    cr_assert_eq(row->values[3].value.b, false);
    db_free_row(db, row);
}

Test(persistence_roundtrip, nextkey_preserved) {
    create_users_db();
    DB* db_before = find_db("users");
    cr_assert_not_null(db_before);
    int expected_next = db_before->nextKey;

    cr_assert_eq(save_db_storage(TEST_DATA_FILE), 0);

    free_db_storage();
    init_db_storage();
    cr_assert_eq(load_db_storage(TEST_DATA_FILE), 1);

    DB* db = find_db("users");
    cr_assert_not_null(db);
    cr_assert_eq(db->nextKey, expected_next);
}

Test(persistence_roundtrip, multiple_dbs) {
    /* Create users db */
    create_users_db();

    /* Create a products db */
    Command* cmd = parse_command("CREATE products (string title, double price)");
    cr_assert_not_null(cmd);
    CommandResult* res = execute_command(cmd);
    cr_assert_not_null(res);
    free_command_result(res);
    free_command(cmd, 0);

    cmd = parse_command("ADD products * (\"Widget\", 9.99)");
    cr_assert_not_null(cmd);
    res = execute_command(cmd);
    cr_assert_not_null(res);
    cr_assert(res->code > 0);
    free_command_result(res);
    free_command(cmd, 1);

    cr_assert_eq(save_db_storage(TEST_DATA_FILE), 0);

    free_db_storage();
    init_db_storage();

    int loaded = load_db_storage(TEST_DATA_FILE);
    cr_assert_eq(loaded, 2);

    cr_assert_not_null(find_db("users"));
    cr_assert_not_null(find_db("products"));
}

/* =========================================================================
 * Test: load from missing file
 * ====================================================================== */
TestSuite(persistence_load, .init = setup, .fini = teardown);

Test(persistence_load, missing_file_returns_zero) {
    int loaded = load_db_storage("/tmp/poudb_nonexistent_file_xyz.dat");
    cr_assert_eq(loaded, 0);
}

Test(persistence_load, null_filepath_returns_error) {
    int loaded = load_db_storage(NULL);
    cr_assert_eq(loaded, -1);
}

/* =========================================================================
 * Test: integer-only database
 * ====================================================================== */
TestSuite(persistence_types, .init = setup, .fini = teardown);

Test(persistence_types, int_double_fields) {
    Command* cmd = parse_command("CREATE metrics (int count, double value)");
    cr_assert_not_null(cmd);
    CommandResult* res = execute_command(cmd);
    cr_assert_not_null(res);
    free_command_result(res);
    free_command(cmd, 0);

    cmd = parse_command("ADD metrics 10 (42, 3.14)");
    cr_assert_not_null(cmd);
    res = execute_command(cmd);
    cr_assert_not_null(res);
    cr_assert(res->code > 0);
    free_command_result(res);
    free_command(cmd, 1);

    cr_assert_eq(save_db_storage(TEST_DATA_FILE), 0);
    free_db_storage();
    init_db_storage();
    cr_assert_eq(load_db_storage(TEST_DATA_FILE), 1);

    DB* db = find_db("metrics");
    cr_assert_not_null(db);
    Row* row = db_get_row(db, 10);
    cr_assert_not_null(row);
    cr_assert_eq(row->values[1].value.i, 42);
    /* Compare double with a small tolerance */
    double diff = row->values[2].value.d - 3.14;
    cr_assert(diff > -0.0001 && diff < 0.0001);
    db_free_row(db, row);
}
