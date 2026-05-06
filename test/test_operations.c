#include <criterion/criterion.h>
#include <criterion/new/assert.h>
#include <string.h>

#include "db/data_model.h"
#include "db/operations.h"
#include "db/parser.h"

/**
 * Test suite for execute_create and execute_add functions
 */

// Setup and teardown functions for database storage
static void setup(void) {
    init_db_storage();
}

static void teardown(void) {
    free_db_storage();
}

static Row* get_row_by_key(DB* db, int key) {
    Row* row = db_get_row(db, key);
    cr_assert_not_null(row);
    return row;
}

// ============================================================================
// execute_create tests
// ============================================================================

TestSuite(execute_create, .init = setup, .fini = teardown);

Test(execute_create, single_int_field) {
    Command* cmd = parse_command("CREATE mydb (int id)");
    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_CREATE);

    CommandResult* result = execute_command(cmd);
    cr_assert_not_null(result);
    cr_assert_eq(result->code, 1);  // 1 field created
    cr_assert_null(result->message);

    // Verify database was created
    DB* db = find_db("mydb");
    cr_assert_not_null(db);
    cr_assert_eq(db->fieldsCount, 2);  // key + 1 user field
    cr_assert_str_eq(db->fields[0].name, "key");
    cr_assert_eq(db->fields[0].type, TYPE_INT);
    cr_assert_str_eq(db->fields[1].name, "id");
    cr_assert_eq(db->fields[1].type, TYPE_INT);

    free(result);
    free_command(cmd, 0);
}

Test(execute_create, multiple_fields) {
    Command* cmd = parse_command(
        "CREATE users (int age, string name, double salary, bool active)");
    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_CREATE);

    CommandResult* result = execute_command(cmd);
    cr_assert_not_null(result);
    cr_assert_eq(result->code, 4);  // 4 fields created
    cr_assert_null(result->message);

    // Verify database structure
    DB* db = find_db("users");
    cr_assert_not_null(db);
    cr_assert_eq(db->fieldsCount, 5);  // key + 4 user fields

    cr_assert_str_eq(db->fields[0].name, "key");
    cr_assert_eq(db->fields[0].type, TYPE_INT);

    cr_assert_str_eq(db->fields[1].name, "age");
    cr_assert_eq(db->fields[1].type, TYPE_INT);

    cr_assert_str_eq(db->fields[2].name, "name");
    cr_assert_eq(db->fields[2].type, TYPE_STRING);

    cr_assert_str_eq(db->fields[3].name, "salary");
    cr_assert_eq(db->fields[3].type, TYPE_DOUBLE);

    cr_assert_str_eq(db->fields[4].name, "active");
    cr_assert_eq(db->fields[4].type, TYPE_BOOL);

    free(result);
    free_command(cmd, 0);
}

Test(execute_create, duplicate_database_fails) {
    Command* cmd1 = parse_command("CREATE mydb (int id)");
    CommandResult* result1 = execute_command(cmd1);
    cr_assert_not_null(result1);
    cr_assert_eq(result1->code, 1);
    free(result1);
    free_command(cmd1, 0);

    // Try to create same database again
    Command* cmd2 = parse_command("CREATE mydb (string name)");
    CommandResult* result2 = execute_command(cmd2);
    cr_assert_not_null(result2);
    cr_assert_eq(result2->code, -2);  // EX_DB_ALREADY_EXISTS
    cr_assert_not_null(result2->message);

    free(result2);
    free_command(cmd2, 0);
}

Test(execute_create, initializes_row_capacity) {
    Command* cmd = parse_command("CREATE testdb (int value)");
    CommandResult* result = execute_command(cmd);
    cr_assert_not_null(result);
    cr_assert_eq(result->code, 1);

    DB* db = find_db("testdb");
    cr_assert_not_null(db);
    cr_assert_eq(db->rowsCount, 0);
    cr_assert_not_null(db->rowMap);
    cr_assert_eq(db->nextKey, 1);

    free(result);
    free_command(cmd, 0);
}

Test(execute_create, case_insensitive_types) {
    Command* cmd = parse_command("CREATE testdb (INT id, STRING name)");
    CommandResult* result = execute_command(cmd);
    cr_assert_not_null(result);
    cr_assert_eq(result->code, 2);

    DB* db = find_db("testdb");
    cr_assert_not_null(db);
    cr_assert_eq(db->fields[1].type, TYPE_INT);
    cr_assert_eq(db->fields[2].type, TYPE_STRING);

    free(result);
    free_command(cmd, 0);
}

Test(execute_create, reserved_key_field_rejected) {
    Command* cmd = parse_command("CREATE testdb (int key)");
    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);  // Parser should reject 'key' field name

    free_command(cmd, 0);
}

// ============================================================================
// execute_add tests
// ============================================================================

TestSuite(execute_add, .init = setup, .fini = teardown);

Test(execute_add, single_int_value_auto_key) {
    // First create the database
    Command* create_cmd = parse_command("CREATE mydb (int value)");
    CommandResult* create_result = execute_command(create_cmd);
    cr_assert_eq(create_result->code, 1);
    free(create_result);
    free_command(create_cmd, 0);

    // Add a row with auto-generated key
    Command* add_cmd = parse_command("ADD mydb * (42)");
    cr_assert_not_null(add_cmd);
    cr_assert_eq(add_cmd->op, OP_ADD);

    CommandResult* add_result = execute_command(add_cmd);
    cr_assert_not_null(add_result);
    cr_assert_eq(add_result->code, 1);  // First auto key is 1
    cr_assert_null(add_result->message);

    // Verify the row was added
    DB* db = find_db("mydb");
    cr_assert_eq(db->rowsCount, 1);
    Row* row = get_row_by_key(db, 1);
    cr_assert_eq(row->values[0].value.i, 1);  // key
    cr_assert_eq(row->values[1].value.i, 42); // value
    db_free_row(db, row);

    free(add_result);
    free_command(add_cmd, 1);  // strings transferred
}

Test(execute_add, single_string_value) {
    Command* create_cmd = parse_command("CREATE mydb (string name)");
    CommandResult* create_result = execute_command(create_cmd);
    cr_assert_eq(create_result->code, 1);
    free(create_result);
    free_command(create_cmd, 0);

    Command* add_cmd = parse_command("ADD mydb * (\"hello\")");
    CommandResult* add_result = execute_command(add_cmd);
    cr_assert_not_null(add_result);
    cr_assert_eq(add_result->code, 1);

    DB* db = find_db("mydb");
    cr_assert_eq(db->rowsCount, 1);
    Row* row = get_row_by_key(db, 1);
    cr_assert_str_eq(row->values[1].value.s, "hello");
    db_free_row(db, row);

    free(add_result);
    free_command(add_cmd, 1);
}

Test(execute_add, explicit_key) {
    Command* create_cmd = parse_command("CREATE mydb (int value)");
    CommandResult* create_result = execute_command(create_cmd);
    cr_assert_eq(create_result->code, 1);
    free(create_result);
    free_command(create_cmd, 0);

    Command* add_cmd = parse_command("ADD mydb 100 (42)");
    CommandResult* add_result = execute_command(add_cmd);
    cr_assert_not_null(add_result);
    cr_assert_eq(add_result->code, 100);  // Returns the key used

    DB* db = find_db("mydb");
    Row* row = get_row_by_key(db, 100);
    cr_assert_eq(row->values[0].value.i, 100);
    db_free_row(db, row);
    cr_assert_eq(db->nextKey, 101);  // nextKey updated

    free(add_result);
    free_command(add_cmd, 1);
}

Test(execute_add, multiple_values_mixed_types) {
    Command* create_cmd = parse_command(
        "CREATE users (int age, string name, double salary, bool active)");
    CommandResult* create_result = execute_command(create_cmd);
    cr_assert_eq(create_result->code, 4);
    free(create_result);
    free_command(create_cmd, 0);

    Command* add_cmd = parse_command(
        "ADD users * (25, \"John Doe\", 50000.50, true)");
    CommandResult* add_result = execute_command(add_cmd);
    cr_assert_not_null(add_result);
    cr_assert_eq(add_result->code, 1);

    DB* db = find_db("users");
    cr_assert_eq(db->rowsCount, 1);
    Row* row = get_row_by_key(db, 1);
    cr_assert_eq(row->values[0].value.i, 1);       // key
    cr_assert_eq(row->values[1].value.i, 25);      // age
    cr_assert_str_eq(row->values[2].value.s, "John Doe"); // name
    cr_assert_float_eq(row->values[3].value.d, 50000.50, 0.01); // salary
    cr_assert_eq(row->values[4].value.b, true);    // active
    db_free_row(db, row);

    free(add_result);
    free_command(add_cmd, 1);
}

Test(execute_add, multiple_rows_auto_key_increments) {
    Command* create_cmd = parse_command("CREATE mydb (int value)");
    CommandResult* create_result = execute_command(create_cmd);
    free(create_result);
    free_command(create_cmd, 0);

    // Add first row
    Command* add_cmd1 = parse_command("ADD mydb * (10)");
    CommandResult* add_result1 = execute_command(add_cmd1);
    cr_assert_eq(add_result1->code, 1);
    free(add_result1);
    free_command(add_cmd1, 1);

    // Add second row
    Command* add_cmd2 = parse_command("ADD mydb * (20)");
    CommandResult* add_result2 = execute_command(add_cmd2);
    cr_assert_eq(add_result2->code, 2);
    free(add_result2);
    free_command(add_cmd2, 1);

    // Add third row
    Command* add_cmd3 = parse_command("ADD mydb * (30)");
    CommandResult* add_result3 = execute_command(add_cmd3);
    cr_assert_eq(add_result3->code, 3);
    free(add_result3);
    free_command(add_cmd3, 1);

    DB* db = find_db("mydb");
    cr_assert_eq(db->rowsCount, 3);
    cr_assert_eq(db->nextKey, 4);
}

Test(execute_add, explicit_key_updates_next_key) {
    Command* create_cmd = parse_command("CREATE mydb (int value)");
    CommandResult* create_result = execute_command(create_cmd);
    free(create_result);
    free_command(create_cmd, 0);

    // Add with explicit key 50
    Command* add_cmd1 = parse_command("ADD mydb 50 (100)");
    CommandResult* add_result1 = execute_command(add_cmd1);
    cr_assert_eq(add_result1->code, 50);
    free(add_result1);
    free_command(add_cmd1, 1);

    DB* db = find_db("mydb");
    cr_assert_eq(db->nextKey, 51);

    // Add with auto key should use 51
    Command* add_cmd2 = parse_command("ADD mydb * (200)");
    CommandResult* add_result2 = execute_command(add_cmd2);
    cr_assert_eq(add_result2->code, 51);
    free(add_result2);
    free_command(add_cmd2, 1);
}

Test(execute_add, database_not_found) {
    Command* add_cmd = parse_command("ADD nonexistent * (42)");
    CommandResult* add_result = execute_command(add_cmd);
    cr_assert_not_null(add_result);
    cr_assert_eq(add_result->code, -1);
    cr_assert_not_null(add_result->message);

    free(add_result);
    free_command(add_cmd, 0);  // Strings not transferred on error
}

Test(execute_add, value_count_mismatch) {
    Command* create_cmd = parse_command("CREATE mydb (int a, int b)");
    CommandResult* create_result = execute_command(create_cmd);
    free(create_result);
    free_command(create_cmd, 0);

    // Try to add only one value when two are expected
    Command* add_cmd = parse_command("ADD mydb * (42)");
    CommandResult* add_result = execute_command(add_cmd);
    cr_assert_not_null(add_result);
    cr_assert_eq(add_result->code, -1);
    cr_assert_not_null(add_result->message);

    free(add_result);
    free_command(add_cmd, 0);  // Strings not transferred on error
}

Test(execute_add, negative_values) {
    Command* create_cmd = parse_command("CREATE mydb (int value, double amount)");
    CommandResult* create_result = execute_command(create_cmd);
    free(create_result);
    free_command(create_cmd, 0);

    Command* add_cmd = parse_command("ADD mydb * (-42, -3.14)");
    CommandResult* add_result = execute_command(add_cmd);
    cr_assert_eq(add_result->code, 1);

    DB* db = find_db("mydb");
    Row* row = get_row_by_key(db, 1);
    cr_assert_eq(row->values[1].value.i, -42);
    cr_assert_float_eq(row->values[2].value.d, -3.14, 0.01);
    db_free_row(db, row);

    free(add_result);
    free_command(add_cmd, 1);
}

Test(execute_add, bool_values) {
    Command* create_cmd = parse_command("CREATE mydb (bool flag1, bool flag2)");
    CommandResult* create_result = execute_command(create_cmd);
    free(create_result);
    free_command(create_cmd, 0);

    Command* add_cmd = parse_command("ADD mydb * (true, false)");
    CommandResult* add_result = execute_command(add_cmd);
    cr_assert_eq(add_result->code, 1);

    DB* db = find_db("mydb");
    Row* row = get_row_by_key(db, 1);
    cr_assert_eq(row->values[1].value.b, true);
    cr_assert_eq(row->values[2].value.b, false);
    db_free_row(db, row);

    free(add_result);
    free_command(add_cmd, 1);
}

Test(execute_add, empty_string) {
    Command* create_cmd = parse_command("CREATE mydb (string text)");
    CommandResult* create_result = execute_command(create_cmd);
    free(create_result);
    free_command(create_cmd, 0);

    Command* add_cmd = parse_command("ADD mydb * (\"\")");
    CommandResult* add_result = execute_command(add_cmd);
    cr_assert_eq(add_result->code, 1);

    DB* db = find_db("mydb");
    Row* row = get_row_by_key(db, 1);
    cr_assert_str_eq(row->values[1].value.s, "");
    db_free_row(db, row);

    free(add_result);
    free_command(add_cmd, 1);
}

Test(execute_add, string_with_spaces) {
    Command* create_cmd = parse_command("CREATE mydb (string text)");
    CommandResult* create_result = execute_command(create_cmd);
    free(create_result);
    free_command(create_cmd, 0);

    Command* add_cmd = parse_command("ADD mydb * (\"hello world with spaces\")");
    CommandResult* add_result = execute_command(add_cmd);
    cr_assert_eq(add_result->code, 1);

    DB* db = find_db("mydb");
    Row* row = get_row_by_key(db, 1);
    cr_assert_str_eq(row->values[1].value.s, "hello world with spaces");
    db_free_row(db, row);

    free(add_result);
    free_command(add_cmd, 1);
}

Test(execute_add, zero_key) {
    Command* create_cmd = parse_command("CREATE mydb (int value)");
    CommandResult* create_result = execute_command(create_cmd);
    free(create_result);
    free_command(create_cmd, 0);

    Command* add_cmd = parse_command("ADD mydb 0 (42)");
    CommandResult* add_result = execute_command(add_cmd);
    cr_assert_eq(add_result->code, 0);

    DB* db = find_db("mydb");
    Row* row = get_row_by_key(db, 0);
    cr_assert_eq(row->values[0].value.i, 0);
    db_free_row(db, row);
    cr_assert_eq(db->nextKey, 1);  // nextKey stays at 1 since 0 < 1

    free(add_result);
    free_command(add_cmd, 1);
}

Test(execute_add, scientific_notation) {
    Command* create_cmd = parse_command("CREATE mydb (double value)");
    CommandResult* create_result = execute_command(create_cmd);
    free(create_result);
    free_command(create_cmd, 0);

    Command* add_cmd = parse_command("ADD mydb * (1.5e3)");
    CommandResult* add_result = execute_command(add_cmd);
    cr_assert_eq(add_result->code, 1);

    DB* db = find_db("mydb");
    Row* row = get_row_by_key(db, 1);
    cr_assert_float_eq(row->values[1].value.d, 1500.0, 0.01);
    db_free_row(db, row);

    free(add_result);
    free_command(add_cmd, 1);
}

// ============================================================================
// execute_up tests
// ============================================================================

TestSuite(execute_up, .init = setup, .fini = teardown);

Test(execute_up, update_single_int_value) {
    // Create database and add a row
    Command* create_cmd = parse_command("CREATE mydb (int value)");
    CommandResult* create_result = execute_command(create_cmd);
    free(create_result);
    free_command(create_cmd, 0);

    Command* add_cmd = parse_command("ADD mydb * (42)");
    CommandResult* add_result = execute_command(add_cmd);
    cr_assert_eq(add_result->code, 1);
    free(add_result);
    free_command(add_cmd, 1);

    // Update the row
    Command* up_cmd = parse_command("UP mydb 1 (100)");
    cr_assert_not_null(up_cmd);
    cr_assert_eq(up_cmd->op, OP_UP);

    CommandResult* up_result = execute_command(up_cmd);
    cr_assert_not_null(up_result);
    cr_assert_eq(up_result->code, 1);  // Returns the key
    cr_assert_null(up_result->message);

    // Verify the update
    DB* db = find_db("mydb");
    Row* row = get_row_by_key(db, 1);
    cr_assert_eq(row->values[1].value.i, 100);
    db_free_row(db, row);

    free(up_result);
    free_command(up_cmd, 1);
}

Test(execute_up, update_single_string_value) {
    Command* create_cmd = parse_command("CREATE mydb (string name)");
    CommandResult* create_result = execute_command(create_cmd);
    free(create_result);
    free_command(create_cmd, 0);

    Command* add_cmd = parse_command("ADD mydb * (\"hello\")");
    CommandResult* add_result = execute_command(add_cmd);
    free(add_result);
    free_command(add_cmd, 1);

    Command* up_cmd = parse_command("UP mydb 1 (\"world\")");
    CommandResult* up_result = execute_command(up_cmd);
    cr_assert_eq(up_result->code, 1);

    DB* db = find_db("mydb");
    Row* row = get_row_by_key(db, 1);
    cr_assert_str_eq(row->values[1].value.s, "world");
    db_free_row(db, row);

    free(up_result);
    free_command(up_cmd, 1);
}

Test(execute_up, update_multiple_values) {
    Command* create_cmd = parse_command(
        "CREATE users (int age, string name, double salary, bool active)");
    CommandResult* create_result = execute_command(create_cmd);
    free(create_result);
    free_command(create_cmd, 0);

    Command* add_cmd = parse_command(
        "ADD users * (25, \"John\", 50000.0, true)");
    CommandResult* add_result = execute_command(add_cmd);
    free(add_result);
    free_command(add_cmd, 1);

    Command* up_cmd = parse_command(
        "UP users 1 (30, \"Jane\", 60000.0, false)");
    CommandResult* up_result = execute_command(up_cmd);
    cr_assert_eq(up_result->code, 1);

    DB* db = find_db("users");
    Row* row = get_row_by_key(db, 1);
    cr_assert_eq(row->values[1].value.i, 30);
    cr_assert_str_eq(row->values[2].value.s, "Jane");
    cr_assert_float_eq(row->values[3].value.d, 60000.0, 0.01);
    cr_assert_eq(row->values[4].value.b, false);
    db_free_row(db, row);

    free(up_result);
    free_command(up_cmd, 1);
}

Test(execute_up, update_with_ignore_flags) {
    Command* create_cmd = parse_command("CREATE mydb (int a, string b, int c)");
    CommandResult* create_result = execute_command(create_cmd);
    free(create_result);
    free_command(create_cmd, 0);

    Command* add_cmd = parse_command("ADD mydb * (10, \"original\", 30)");
    CommandResult* add_result = execute_command(add_cmd);
    free(add_result);
    free_command(add_cmd, 1);

    // Update only first and last fields, ignore middle (string)
    Command* up_cmd = parse_command("UP mydb 1 (20, _, 40)");
    CommandResult* up_result = execute_command(up_cmd);
    cr_assert_eq(up_result->code, 1);

    DB* db = find_db("mydb");
    Row* row = get_row_by_key(db, 1);
    cr_assert_eq(row->values[1].value.i, 20);  // Updated
    cr_assert_str_eq(row->values[2].value.s, "original");  // Unchanged
    cr_assert_eq(row->values[3].value.i, 40);  // Updated
    db_free_row(db, row);

    free(up_result);
    free_command(up_cmd, 1);
}

Test(execute_up, update_ignore_all_fields) {
    Command* create_cmd = parse_command("CREATE mydb (int a, int b)");
    CommandResult* create_result = execute_command(create_cmd);
    free(create_result);
    free_command(create_cmd, 0);

    Command* add_cmd = parse_command("ADD mydb * (10, 20)");
    CommandResult* add_result = execute_command(add_cmd);
    free(add_result);
    free_command(add_cmd, 1);

    // Ignore all fields - row should remain unchanged
    Command* up_cmd = parse_command("UP mydb 1 (_, _)");
    CommandResult* up_result = execute_command(up_cmd);
    cr_assert_eq(up_result->code, 1);

    DB* db = find_db("mydb");
    Row* row = get_row_by_key(db, 1);
    cr_assert_eq(row->values[1].value.i, 10);
    cr_assert_eq(row->values[2].value.i, 20);
    db_free_row(db, row);

    free(up_result);
    free_command(up_cmd, 1);
}

Test(execute_up, update_row_not_found) {
    Command* create_cmd = parse_command("CREATE mydb (int value)");
    CommandResult* create_result = execute_command(create_cmd);
    free(create_result);
    free_command(create_cmd, 0);

    Command* add_cmd = parse_command("ADD mydb * (42)");
    CommandResult* add_result = execute_command(add_cmd);
    free(add_result);
    free_command(add_cmd, 1);

    // Try to update non-existent key
    Command* up_cmd = parse_command("UP mydb 999 (100)");
    CommandResult* up_result = execute_command(up_cmd);
    cr_assert_eq(up_result->code, -3);
    cr_assert_not_null(up_result->message);

    free(up_result);
    free_command(up_cmd, 0);
}

Test(execute_up, update_database_not_found) {
    Command* up_cmd = parse_command("UP nonexistent 1 (42)");
    CommandResult* up_result = execute_command(up_cmd);
    cr_assert_eq(up_result->code, -1);
    cr_assert_not_null(up_result->message);

    free(up_result);
    free_command(up_cmd, 0);
}

Test(execute_up, update_value_count_mismatch) {
    Command* create_cmd = parse_command("CREATE mydb (int a, int b)");
    CommandResult* create_result = execute_command(create_cmd);
    free(create_result);
    free_command(create_cmd, 0);

    Command* add_cmd = parse_command("ADD mydb * (10, 20)");
    CommandResult* add_result = execute_command(add_cmd);
    free(add_result);
    free_command(add_cmd, 1);

    // Try to update with wrong number of values
    Command* up_cmd = parse_command("UP mydb 1 (100)");
    CommandResult* up_result = execute_command(up_cmd);
    cr_assert_eq(up_result->code, -1);
    cr_assert_not_null(up_result->message);

    free(up_result);
    free_command(up_cmd, 0);
}

Test(execute_up, update_negative_values) {
    Command* create_cmd = parse_command("CREATE mydb (int value, double amount)");
    CommandResult* create_result = execute_command(create_cmd);
    free(create_result);
    free_command(create_cmd, 0);

    Command* add_cmd = parse_command("ADD mydb * (10, 20.0)");
    CommandResult* add_result = execute_command(add_cmd);
    free(add_result);
    free_command(add_cmd, 1);

    Command* up_cmd = parse_command("UP mydb 1 (-42, -3.14)");
    CommandResult* up_result = execute_command(up_cmd);
    cr_assert_eq(up_result->code, 1);

    DB* db = find_db("mydb");
    Row* row = get_row_by_key(db, 1);
    cr_assert_eq(row->values[1].value.i, -42);
    cr_assert_float_eq(row->values[2].value.d, -3.14, 0.01);
    db_free_row(db, row);

    free(up_result);
    free_command(up_cmd, 1);
}

Test(execute_up, update_bool_values) {
    Command* create_cmd = parse_command("CREATE mydb (bool flag1, bool flag2)");
    CommandResult* create_result = execute_command(create_cmd);
    free(create_result);
    free_command(create_cmd, 0);

    Command* add_cmd = parse_command("ADD mydb * (true, false)");
    CommandResult* add_result = execute_command(add_cmd);
    free(add_result);
    free_command(add_cmd, 1);

    Command* up_cmd = parse_command("UP mydb 1 (false, true)");
    CommandResult* up_result = execute_command(up_cmd);
    cr_assert_eq(up_result->code, 1);

    DB* db = find_db("mydb");
    Row* row = get_row_by_key(db, 1);
    cr_assert_eq(row->values[1].value.b, false);
    cr_assert_eq(row->values[2].value.b, true);
    db_free_row(db, row);

    free(up_result);
    free_command(up_cmd, 1);
}

Test(execute_up, update_empty_string) {
    Command* create_cmd = parse_command("CREATE mydb (string text)");
    CommandResult* create_result = execute_command(create_cmd);
    free(create_result);
    free_command(create_cmd, 0);

    Command* add_cmd = parse_command("ADD mydb * (\"hello\")");
    CommandResult* add_result = execute_command(add_cmd);
    free(add_result);
    free_command(add_cmd, 1);

    Command* up_cmd = parse_command("UP mydb 1 (\"\")");
    CommandResult* up_result = execute_command(up_cmd);
    cr_assert_eq(up_result->code, 1);

    DB* db = find_db("mydb");
    Row* row = get_row_by_key(db, 1);
    cr_assert_str_eq(row->values[1].value.s, "");
    db_free_row(db, row);

    free(up_result);
    free_command(up_cmd, 1);
}

Test(execute_up, update_with_explicit_key) {
    Command* create_cmd = parse_command("CREATE mydb (int value)");
    CommandResult* create_result = execute_command(create_cmd);
    free(create_result);
    free_command(create_cmd, 0);

    Command* add_cmd = parse_command("ADD mydb 100 (42)");
    CommandResult* add_result = execute_command(add_cmd);
    free(add_result);
    free_command(add_cmd, 1);

    Command* up_cmd = parse_command("UP mydb 100 (999)");
    CommandResult* up_result = execute_command(up_cmd);
    cr_assert_eq(up_result->code, 100);

    DB* db = find_db("mydb");
    Row* row = get_row_by_key(db, 100);
    cr_assert_eq(row->values[1].value.i, 999);
    db_free_row(db, row);

    free(up_result);
    free_command(up_cmd, 1);
}

Test(execute_up, update_multiple_rows_correct_key) {
    Command* create_cmd = parse_command("CREATE mydb (int value)");
    CommandResult* create_result = execute_command(create_cmd);
    free(create_result);
    free_command(create_cmd, 0);

    // Add three rows
    Command* add_cmd1 = parse_command("ADD mydb * (10)");
    CommandResult* add_result1 = execute_command(add_cmd1);
    free(add_result1);
    free_command(add_cmd1, 1);

    Command* add_cmd2 = parse_command("ADD mydb * (20)");
    CommandResult* add_result2 = execute_command(add_cmd2);
    free(add_result2);
    free_command(add_cmd2, 1);

    Command* add_cmd3 = parse_command("ADD mydb * (30)");
    CommandResult* add_result3 = execute_command(add_cmd3);
    free(add_result3);
    free_command(add_cmd3, 1);

    // Update only the second row
    Command* up_cmd = parse_command("UP mydb 2 (200)");
    CommandResult* up_result = execute_command(up_cmd);
    cr_assert_eq(up_result->code, 2);
    free(up_result);
    free_command(up_cmd, 1);

    // Verify only second row changed
    DB* db = find_db("mydb");
    Row* row1 = get_row_by_key(db, 1);
    Row* row2 = get_row_by_key(db, 2);
    Row* row3 = get_row_by_key(db, 3);
    cr_assert_eq(row1->values[1].value.i, 10);   // Unchanged
    cr_assert_eq(row2->values[1].value.i, 200); // Updated
    cr_assert_eq(row3->values[1].value.i, 30);  // Unchanged
    db_free_row(db, row1);
    db_free_row(db, row2);
    db_free_row(db, row3);
}

// ============================================================================
// execute_get tests
// ============================================================================

TestSuite(execute_get, .init = setup, .fini = teardown);

Test(execute_get, get_single_int_field) {
    Command* create_cmd = parse_command("CREATE mydb (int value)");
    CommandResult* create_result = execute_command(create_cmd);
    free_command_result(create_result);
    free_command(create_cmd, 0);

    Command* add_cmd = parse_command("ADD mydb * (42)");
    CommandResult* add_result = execute_command(add_cmd);
    free_command_result(add_result);
    free_command(add_cmd, 1);

    Command* get_cmd = parse_command("GET mydb 1");
    cr_assert_not_null(get_cmd);
    cr_assert_eq(get_cmd->op, OP_GET);

    CommandResult* get_result = execute_command(get_cmd);
    cr_assert_not_null(get_result);
    cr_assert_eq(get_result->code, 1);
    cr_assert_null(get_result->message);
    cr_assert_not_null(get_result->data);

    // Check that table contains key and value
    cr_assert(strstr(get_result->data, "key") != NULL);
    cr_assert(strstr(get_result->data, "value") != NULL);
    cr_assert(strstr(get_result->data, "1") != NULL);
    cr_assert(strstr(get_result->data, "42") != NULL);

    free_command_result(get_result);
    free_command(get_cmd, 0);
}

Test(execute_get, get_all_fields_default) {
    Command* create_cmd = parse_command(
        "CREATE users (int age, string name, double salary, bool active)");
    CommandResult* create_result = execute_command(create_cmd);
    free_command_result(create_result);
    free_command(create_cmd, 0);

    Command* add_cmd = parse_command(
        "ADD users * (25, \"John Doe\", 50000.5, true)");
    CommandResult* add_result = execute_command(add_cmd);
    free_command_result(add_result);
    free_command(add_cmd, 1);

    Command* get_cmd = parse_command("GET users 1");
    CommandResult* get_result = execute_command(get_cmd);
    cr_assert_eq(get_result->code, 1);
    cr_assert_not_null(get_result->data);

    // Check all fields are in table
    cr_assert(strstr(get_result->data, "key") != NULL);
    cr_assert(strstr(get_result->data, "age") != NULL);
    cr_assert(strstr(get_result->data, "name") != NULL);
    cr_assert(strstr(get_result->data, "salary") != NULL);
    cr_assert(strstr(get_result->data, "active") != NULL);

    // Check values
    cr_assert(strstr(get_result->data, "25") != NULL);
    cr_assert(strstr(get_result->data, "John Doe") != NULL);
    cr_assert(strstr(get_result->data, "50000.5") != NULL);
    cr_assert(strstr(get_result->data, "true") != NULL);

    free_command_result(get_result);
    free_command(get_cmd, 0);
}

Test(execute_get, get_specific_fields) {
    Command* create_cmd = parse_command(
        "CREATE users (int age, string name, double salary)");
    CommandResult* create_result = execute_command(create_cmd);
    free_command_result(create_result);
    free_command(create_cmd, 0);

    Command* add_cmd = parse_command("ADD users * (30, \"Jane\", 75000.0)");
    CommandResult* add_result = execute_command(add_cmd);
    free_command_result(add_result);
    free_command(add_cmd, 1);

    // Request only name and salary
    Command* get_cmd = parse_command("GET users 1 (name, salary)");
    CommandResult* get_result = execute_command(get_cmd);
    cr_assert_eq(get_result->code, 1);
    cr_assert_not_null(get_result->data);

    // Check requested fields are present
    cr_assert(strstr(get_result->data, "name") != NULL);
    cr_assert(strstr(get_result->data, "salary") != NULL);
    cr_assert(strstr(get_result->data, "Jane") != NULL);
    cr_assert(strstr(get_result->data, "75000") != NULL);

    free_command_result(get_result);
    free_command(get_cmd, 0);
}

Test(execute_get, get_only_key_field) {
    Command* create_cmd = parse_command("CREATE mydb (int value)");
    CommandResult* create_result = execute_command(create_cmd);
    free_command_result(create_result);
    free_command(create_cmd, 0);

    Command* add_cmd = parse_command("ADD mydb * (100)");
    CommandResult* add_result = execute_command(add_cmd);
    free_command_result(add_result);
    free_command(add_cmd, 1);

    Command* get_cmd = parse_command("GET mydb 1 (key)");
    CommandResult* get_result = execute_command(get_cmd);
    cr_assert_eq(get_result->code, 1);
    cr_assert_not_null(get_result->data);

    cr_assert(strstr(get_result->data, "key") != NULL);
    cr_assert(strstr(get_result->data, "1") != NULL);

    free_command_result(get_result);
    free_command(get_cmd, 0);
}

Test(execute_get, get_row_not_found) {
    Command* create_cmd = parse_command("CREATE mydb (int value)");
    CommandResult* create_result = execute_command(create_cmd);
    free_command_result(create_result);
    free_command(create_cmd, 0);

    Command* add_cmd = parse_command("ADD mydb * (42)");
    CommandResult* add_result = execute_command(add_cmd);
    free_command_result(add_result);
    free_command(add_cmd, 1);

    // Try to get non-existent key
    Command* get_cmd = parse_command("GET mydb 999");
    CommandResult* get_result = execute_command(get_cmd);
    cr_assert_eq(get_result->code, -1);
    cr_assert_not_null(get_result->message);
    cr_assert_null(get_result->data);

    free_command_result(get_result);
    free_command(get_cmd, 0);
}

Test(execute_get, get_database_not_found) {
    Command* get_cmd = parse_command("GET nonexistent 1");
    CommandResult* get_result = execute_command(get_cmd);
    cr_assert_eq(get_result->code, -1);
    cr_assert_not_null(get_result->message);
    cr_assert_null(get_result->data);

    free_command_result(get_result);
    free_command(get_cmd, 0);
}

Test(execute_get, get_invalid_field) {
    Command* create_cmd = parse_command("CREATE mydb (int value)");
    CommandResult* create_result = execute_command(create_cmd);
    free_command_result(create_result);
    free_command(create_cmd, 0);

    Command* add_cmd = parse_command("ADD mydb * (42)");
    CommandResult* add_result = execute_command(add_cmd);
    free_command_result(add_result);
    free_command(add_cmd, 1);

    Command* get_cmd = parse_command("GET mydb 1 (nonexistent)");
    CommandResult* get_result = execute_command(get_cmd);
    cr_assert_eq(get_result->code, -1);
    cr_assert_not_null(get_result->message);

    free_command_result(get_result);
    free_command(get_cmd, 0);
}

Test(execute_get, get_with_explicit_key) {
    Command* create_cmd = parse_command("CREATE mydb (int value)");
    CommandResult* create_result = execute_command(create_cmd);
    free_command_result(create_result);
    free_command(create_cmd, 0);

    Command* add_cmd = parse_command("ADD mydb 100 (42)");
    CommandResult* add_result = execute_command(add_cmd);
    free_command_result(add_result);
    free_command(add_cmd, 1);

    Command* get_cmd = parse_command("GET mydb 100");
    CommandResult* get_result = execute_command(get_cmd);
    cr_assert_eq(get_result->code, 100);
    cr_assert_not_null(get_result->data);

    cr_assert(strstr(get_result->data, "100") != NULL);
    cr_assert(strstr(get_result->data, "42") != NULL);

    free_command_result(get_result);
    free_command(get_cmd, 0);
}

Test(execute_get, get_empty_string_value) {
    Command* create_cmd = parse_command("CREATE mydb (string text)");
    CommandResult* create_result = execute_command(create_cmd);
    free_command_result(create_result);
    free_command(create_cmd, 0);

    Command* add_cmd = parse_command("ADD mydb * (\"\")");
    CommandResult* add_result = execute_command(add_cmd);
    free_command_result(add_result);
    free_command(add_cmd, 1);

    Command* get_cmd = parse_command("GET mydb 1");
    CommandResult* get_result = execute_command(get_cmd);
    cr_assert_eq(get_result->code, 1);
    cr_assert_not_null(get_result->data);

    free_command_result(get_result);
    free_command(get_cmd, 0);
}

Test(execute_get, get_bool_false_value) {
    Command* create_cmd = parse_command("CREATE mydb (bool flag)");
    CommandResult* create_result = execute_command(create_cmd);
    free_command_result(create_result);
    free_command(create_cmd, 0);

    Command* add_cmd = parse_command("ADD mydb * (false)");
    CommandResult* add_result = execute_command(add_cmd);
    free_command_result(add_result);
    free_command(add_cmd, 1);

    Command* get_cmd = parse_command("GET mydb 1");
    CommandResult* get_result = execute_command(get_cmd);
    cr_assert_eq(get_result->code, 1);
    cr_assert_not_null(get_result->data);
    cr_assert(strstr(get_result->data, "false") != NULL);

    free_command_result(get_result);
    free_command(get_cmd, 0);
}

Test(execute_get, get_negative_values) {
    Command* create_cmd = parse_command("CREATE mydb (int value, double amount)");
    CommandResult* create_result = execute_command(create_cmd);
    free_command_result(create_result);
    free_command(create_cmd, 0);

    Command* add_cmd = parse_command("ADD mydb * (-42, -3.14)");
    CommandResult* add_result = execute_command(add_cmd);
    free_command_result(add_result);
    free_command(add_cmd, 1);

    Command* get_cmd = parse_command("GET mydb 1");
    CommandResult* get_result = execute_command(get_cmd);
    cr_assert_eq(get_result->code, 1);
    cr_assert_not_null(get_result->data);
    cr_assert(strstr(get_result->data, "-42") != NULL);
    cr_assert(strstr(get_result->data, "-3.14") != NULL);

    free_command_result(get_result);
    free_command(get_cmd, 0);
}

Test(execute_get, get_from_multiple_rows) {
    Command* create_cmd = parse_command("CREATE mydb (int value)");
    CommandResult* create_result = execute_command(create_cmd);
    free_command_result(create_result);
    free_command(create_cmd, 0);

    // Add three rows
    Command* add_cmd1 = parse_command("ADD mydb * (10)");
    CommandResult* add_result1 = execute_command(add_cmd1);
    free_command_result(add_result1);
    free_command(add_cmd1, 1);

    Command* add_cmd2 = parse_command("ADD mydb * (20)");
    CommandResult* add_result2 = execute_command(add_cmd2);
    free_command_result(add_result2);
    free_command(add_cmd2, 1);

    Command* add_cmd3 = parse_command("ADD mydb * (30)");
    CommandResult* add_result3 = execute_command(add_cmd3);
    free_command_result(add_result3);
    free_command(add_cmd3, 1);

    // Get the second row
    Command* get_cmd = parse_command("GET mydb 2");
    CommandResult* get_result = execute_command(get_cmd);
    cr_assert_eq(get_result->code, 2);
    cr_assert_not_null(get_result->data);
    cr_assert(strstr(get_result->data, "20") != NULL);

    free_command_result(get_result);
    free_command(get_cmd, 0);
}

// ============================================================================
// execute_del tests
// ============================================================================

TestSuite(execute_del, .init = setup, .fini = teardown);

Test(execute_del, delete_single_row) {
    Command* create_cmd = parse_command("CREATE mydb (int value)");
    CommandResult* create_result = execute_command(create_cmd);
    free_command_result(create_result);
    free_command(create_cmd, 0);

    Command* add_cmd = parse_command("ADD mydb * (42)");
    CommandResult* add_result = execute_command(add_cmd);
    free_command_result(add_result);
    free_command(add_cmd, 1);

    Command* del_cmd = parse_command("DEL mydb 1");
    cr_assert_not_null(del_cmd);
    cr_assert_eq(del_cmd->op, OP_DEL);

    CommandResult* del_result = execute_command(del_cmd);
    cr_assert_not_null(del_result);
    cr_assert_eq(del_result->code, 1);
    cr_assert_null(del_result->message);

    // Verify row was deleted
    DB* db = find_db("mydb");
    cr_assert_eq(db->rowsCount, 0);

    free_command_result(del_result);
    free_command(del_cmd, 0);
}

Test(execute_del, delete_from_multiple_rows) {
    Command* create_cmd = parse_command("CREATE mydb (int value)");
    CommandResult* create_result = execute_command(create_cmd);
    free_command_result(create_result);
    free_command(create_cmd, 0);

    // Add three rows
    Command* add_cmd1 = parse_command("ADD mydb * (10)");
    CommandResult* add_result1 = execute_command(add_cmd1);
    free_command_result(add_result1);
    free_command(add_cmd1, 1);

    Command* add_cmd2 = parse_command("ADD mydb * (20)");
    CommandResult* add_result2 = execute_command(add_cmd2);
    free_command_result(add_result2);
    free_command(add_cmd2, 1);

    Command* add_cmd3 = parse_command("ADD mydb * (30)");
    CommandResult* add_result3 = execute_command(add_cmd3);
    free_command_result(add_result3);
    free_command(add_cmd3, 1);

    // Delete the middle row
    Command* del_cmd = parse_command("DEL mydb 2");
    CommandResult* del_result = execute_command(del_cmd);
    cr_assert_eq(del_result->code, 2);
    free_command_result(del_result);
    free_command(del_cmd, 0);

    // Verify only 2 rows remain
    DB* db = find_db("mydb");
    cr_assert_eq(db->rowsCount, 2);
    Row* row1 = get_row_by_key(db, 1);
    Row* row3 = get_row_by_key(db, 3);
    cr_assert_eq(row1->values[0].value.i, 1);
    cr_assert_eq(row1->values[1].value.i, 10);
    cr_assert_eq(row3->values[0].value.i, 3);
    cr_assert_eq(row3->values[1].value.i, 30);
    db_free_row(db, row1);
    db_free_row(db, row3);
}

Test(execute_del, delete_first_row) {
    Command* create_cmd = parse_command("CREATE mydb (int value)");
    CommandResult* create_result = execute_command(create_cmd);
    free_command_result(create_result);
    free_command(create_cmd, 0);

    Command* add_cmd1 = parse_command("ADD mydb * (10)");
    CommandResult* add_result1 = execute_command(add_cmd1);
    free_command_result(add_result1);
    free_command(add_cmd1, 1);

    Command* add_cmd2 = parse_command("ADD mydb * (20)");
    CommandResult* add_result2 = execute_command(add_cmd2);
    free_command_result(add_result2);
    free_command(add_cmd2, 1);

    Command* del_cmd = parse_command("DEL mydb 1");
    CommandResult* del_result = execute_command(del_cmd);
    cr_assert_eq(del_result->code, 1);
    free_command_result(del_result);
    free_command(del_cmd, 0);

    DB* db = find_db("mydb");
    cr_assert_eq(db->rowsCount, 1);
    Row* row = get_row_by_key(db, 2);
    db_free_row(db, row);
}

Test(execute_del, delete_last_row) {
    Command* create_cmd = parse_command("CREATE mydb (int value)");
    CommandResult* create_result = execute_command(create_cmd);
    free_command_result(create_result);
    free_command(create_cmd, 0);

    Command* add_cmd1 = parse_command("ADD mydb * (10)");
    CommandResult* add_result1 = execute_command(add_cmd1);
    free_command_result(add_result1);
    free_command(add_cmd1, 1);

    Command* add_cmd2 = parse_command("ADD mydb * (20)");
    CommandResult* add_result2 = execute_command(add_cmd2);
    free_command_result(add_result2);
    free_command(add_cmd2, 1);

    Command* del_cmd = parse_command("DEL mydb 2");
    CommandResult* del_result = execute_command(del_cmd);
    cr_assert_eq(del_result->code, 2);
    free_command_result(del_result);
    free_command(del_cmd, 0);

    DB* db = find_db("mydb");
    cr_assert_eq(db->rowsCount, 1);
    Row* row = get_row_by_key(db, 1);
    db_free_row(db, row);
}

Test(execute_del, delete_with_string_values) {
    Command* create_cmd = parse_command("CREATE mydb (string name, int age)");
    CommandResult* create_result = execute_command(create_cmd);
    free_command_result(create_result);
    free_command(create_cmd, 0);

    Command* add_cmd = parse_command("ADD mydb * (\"John Doe\", 30)");
    CommandResult* add_result = execute_command(add_cmd);
    free_command_result(add_result);
    free_command(add_cmd, 1);

    Command* del_cmd = parse_command("DEL mydb 1");
    CommandResult* del_result = execute_command(del_cmd);
    cr_assert_eq(del_result->code, 1);
    free_command_result(del_result);
    free_command(del_cmd, 0);

    DB* db = find_db("mydb");
    cr_assert_eq(db->rowsCount, 0);
}

Test(execute_del, delete_row_not_found) {
    Command* create_cmd = parse_command("CREATE mydb (int value)");
    CommandResult* create_result = execute_command(create_cmd);
    free_command_result(create_result);
    free_command(create_cmd, 0);

    Command* add_cmd = parse_command("ADD mydb * (42)");
    CommandResult* add_result = execute_command(add_cmd);
    free_command_result(add_result);
    free_command(add_cmd, 1);

    // Try to delete non-existent key
    Command* del_cmd = parse_command("DEL mydb 999");
    CommandResult* del_result = execute_command(del_cmd);
    cr_assert_eq(del_result->code, -2);
    cr_assert_not_null(del_result->message);

    free_command_result(del_result);
    free_command(del_cmd, 0);
}

Test(execute_del, delete_database_not_found) {
    Command* del_cmd = parse_command("DEL nonexistent 1");
    CommandResult* del_result = execute_command(del_cmd);
    cr_assert_eq(del_result->code, -1);
    cr_assert_not_null(del_result->message);

    free_command_result(del_result);
    free_command(del_cmd, 0);
}

Test(execute_del, delete_with_explicit_key) {
    Command* create_cmd = parse_command("CREATE mydb (int value)");
    CommandResult* create_result = execute_command(create_cmd);
    free_command_result(create_result);
    free_command(create_cmd, 0);

    Command* add_cmd = parse_command("ADD mydb 100 (42)");
    CommandResult* add_result = execute_command(add_cmd);
    free_command_result(add_result);
    free_command(add_cmd, 1);

    Command* del_cmd = parse_command("DEL mydb 100");
    CommandResult* del_result = execute_command(del_cmd);
    cr_assert_eq(del_result->code, 100);

    DB* db = find_db("mydb");
    cr_assert_eq(db->rowsCount, 0);

    free_command_result(del_result);
    free_command(del_cmd, 0);
}

Test(execute_del, delete_all_rows_one_by_one) {
    Command* create_cmd = parse_command("CREATE mydb (int value)");
    CommandResult* create_result = execute_command(create_cmd);
    free_command_result(create_result);
    free_command(create_cmd, 0);

    // Add three rows
    Command* add_cmd1 = parse_command("ADD mydb * (10)");
    CommandResult* add_result1 = execute_command(add_cmd1);
    free_command_result(add_result1);
    free_command(add_cmd1, 1);

    Command* add_cmd2 = parse_command("ADD mydb * (20)");
    CommandResult* add_result2 = execute_command(add_cmd2);
    free_command_result(add_result2);
    free_command(add_cmd2, 1);

    Command* add_cmd3 = parse_command("ADD mydb * (30)");
    CommandResult* add_result3 = execute_command(add_cmd3);
    free_command_result(add_result3);
    free_command(add_cmd3, 1);

    DB* db = find_db("mydb");
    cr_assert_eq(db->rowsCount, 3);

    // Delete all rows
    Command* del_cmd1 = parse_command("DEL mydb 1");
    CommandResult* del_result1 = execute_command(del_cmd1);
    cr_assert_eq(del_result1->code, 1);
    free_command_result(del_result1);
    free_command(del_cmd1, 0);
    cr_assert_eq(db->rowsCount, 2);

    Command* del_cmd2 = parse_command("DEL mydb 2");
    CommandResult* del_result2 = execute_command(del_cmd2);
    cr_assert_eq(del_result2->code, 2);
    free_command_result(del_result2);
    free_command(del_cmd2, 0);
    cr_assert_eq(db->rowsCount, 1);

    Command* del_cmd3 = parse_command("DEL mydb 3");
    CommandResult* del_result3 = execute_command(del_cmd3);
    cr_assert_eq(del_result3->code, 3);
    free_command_result(del_result3);
    free_command(del_cmd3, 0);
    cr_assert_eq(db->rowsCount, 0);
}

Test(execute_del, delete_and_add_again) {
    Command* create_cmd = parse_command("CREATE mydb (int value)");
    CommandResult* create_result = execute_command(create_cmd);
    free_command_result(create_result);
    free_command(create_cmd, 0);

    Command* add_cmd1 = parse_command("ADD mydb * (42)");
    CommandResult* add_result1 = execute_command(add_cmd1);
    free_command_result(add_result1);
    free_command(add_cmd1, 1);

    Command* del_cmd = parse_command("DEL mydb 1");
    CommandResult* del_result = execute_command(del_cmd);
    free_command_result(del_result);
    free_command(del_cmd, 0);

    // Add new row after deletion
    Command* add_cmd2 = parse_command("ADD mydb * (99)");
    CommandResult* add_result2 = execute_command(add_cmd2);
    cr_assert_eq(add_result2->code, 2);  // nextKey continues from 2
    free_command_result(add_result2);
    free_command(add_cmd2, 1);

    DB* db = find_db("mydb");
    cr_assert_eq(db->rowsCount, 1);
    Row* row = get_row_by_key(db, 2);
    cr_assert_eq(row->values[0].value.i, 2);
    cr_assert_eq(row->values[1].value.i, 99);
    db_free_row(db, row);
}

// ============================================================================
// execute_get_all tests
// ============================================================================

TestSuite(execute_get_all, .init = setup, .fini = teardown);

Test(execute_get_all, get_all_single_row) {
    Command* create_cmd = parse_command("CREATE mydb (int value)");
    CommandResult* create_result = execute_command(create_cmd);
    cr_assert_eq(create_result->code, 1);
    free(create_result);
    free_command(create_cmd, 0);

    Command* add_cmd = parse_command("ADD mydb * (42)");
    CommandResult* add_result = execute_command(add_cmd);
    cr_assert_eq(add_result->code, 1);
    free(add_result);
    free_command(add_cmd, 1);

    Command* get_all_cmd = parse_command("GET_ALL mydb");
    cr_assert_not_null(get_all_cmd);
    cr_assert_eq(get_all_cmd->op, OP_GET_ALL);

    CommandResult* result = execute_command(get_all_cmd);
    cr_assert_not_null(result);
    cr_assert_eq(result->code, 1);  // 1 row
    cr_assert_null(result->message);
    cr_assert_not_null(result->data);

    // Check table contains headers and value
    cr_assert(strstr(result->data, "key") != NULL);
    cr_assert(strstr(result->data, "value") != NULL);
    cr_assert(strstr(result->data, "42") != NULL);

    free_command_result(result);
    free_command(get_all_cmd, 0);
}

Test(execute_get_all, get_all_multiple_rows) {
    Command* create_cmd = parse_command("CREATE mydb (int age, string name)");
    CommandResult* create_result = execute_command(create_cmd);
    cr_assert_eq(create_result->code, 2);
    free(create_result);
    free_command(create_cmd, 0);

    Command* add1 = parse_command("ADD mydb * (25, \"Alice\")");
    CommandResult* add_result1 = execute_command(add1);
    free(add_result1);
    free_command(add1, 1);

    Command* add2 = parse_command("ADD mydb * (30, \"Bob\")");
    CommandResult* add_result2 = execute_command(add2);
    free(add_result2);
    free_command(add2, 1);

    Command* add3 = parse_command("ADD mydb * (35, \"Charlie\")");
    CommandResult* add_result3 = execute_command(add3);
    free(add_result3);
    free_command(add3, 1);

    Command* get_all_cmd = parse_command("GET_ALL mydb");
    CommandResult* result = execute_command(get_all_cmd);
    cr_assert_not_null(result);
    cr_assert_eq(result->code, 3);  // 3 rows
    cr_assert_not_null(result->data);

    // Check all rows are present
    cr_assert(strstr(result->data, "Alice") != NULL);
    cr_assert(strstr(result->data, "Bob") != NULL);
    cr_assert(strstr(result->data, "Charlie") != NULL);
    cr_assert(strstr(result->data, "25") != NULL);
    cr_assert(strstr(result->data, "30") != NULL);
    cr_assert(strstr(result->data, "35") != NULL);

    free_command_result(result);
    free_command(get_all_cmd, 0);
}

Test(execute_get_all, get_all_empty_database) {
    Command* create_cmd = parse_command("CREATE mydb (int value)");
    CommandResult* create_result = execute_command(create_cmd);
    cr_assert_eq(create_result->code, 1);
    free(create_result);
    free_command(create_cmd, 0);

    Command* get_all_cmd = parse_command("GET_ALL mydb");
    CommandResult* result = execute_command(get_all_cmd);
    cr_assert_not_null(result);
    cr_assert_eq(result->code, 0);  // 0 rows
    cr_assert_not_null(result->data);
    cr_assert(strstr(result->data, "No rows") != NULL);

    free_command_result(result);
    free_command(get_all_cmd, 0);
}

Test(execute_get_all, get_all_with_specific_fields) {
    Command* create_cmd = parse_command("CREATE mydb (int age, string name, double salary)");
    CommandResult* create_result = execute_command(create_cmd);
    cr_assert_eq(create_result->code, 3);
    free(create_result);
    free_command(create_cmd, 0);

    Command* add1 = parse_command("ADD mydb * (25, \"Alice\", 50000.5)");
    CommandResult* add_result1 = execute_command(add1);
    free(add_result1);
    free_command(add1, 1);

    Command* add2 = parse_command("ADD mydb * (30, \"Bob\", 60000.75)");
    CommandResult* add_result2 = execute_command(add2);
    free(add_result2);
    free_command(add2, 1);

    Command* get_all_cmd = parse_command("GET_ALL mydb (key, name)");
    CommandResult* result = execute_command(get_all_cmd);
    cr_assert_not_null(result);
    cr_assert_eq(result->code, 2);  // 2 rows
    cr_assert_not_null(result->data);

    // Check selected fields are present
    cr_assert(strstr(result->data, "key") != NULL);
    cr_assert(strstr(result->data, "name") != NULL);
    cr_assert(strstr(result->data, "Alice") != NULL);
    cr_assert(strstr(result->data, "Bob") != NULL);

    // Age and salary should not be in the output
    cr_assert(strstr(result->data, "age") == NULL);
    cr_assert(strstr(result->data, "salary") == NULL);

    free_command_result(result);
    free_command(get_all_cmd, 0);
}

Test(execute_get_all, get_all_database_not_found) {
    Command* get_all_cmd = parse_command("GET_ALL nonexistent");
    cr_assert_not_null(get_all_cmd);

    CommandResult* result = execute_command(get_all_cmd);
    cr_assert_not_null(result);
    cr_assert(result->code < 0);
    cr_assert_not_null(result->message);

    free_command_result(result);
    free_command(get_all_cmd, 0);
}

Test(execute_get_all, get_all_invalid_field) {
    Command* create_cmd = parse_command("CREATE mydb (int value)");
    CommandResult* create_result = execute_command(create_cmd);
    cr_assert_eq(create_result->code, 1);
    free(create_result);
    free_command(create_cmd, 0);

    Command* get_all_cmd = parse_command("GET_ALL mydb (invalidfield)");
    CommandResult* result = execute_command(get_all_cmd);
    cr_assert_not_null(result);
    cr_assert(result->code < 0);
    cr_assert_not_null(result->message);

    free_command_result(result);
    free_command(get_all_cmd, 0);
}

Test(execute_get_all, get_all_mixed_types) {
    Command* create_cmd = parse_command("CREATE mydb (int id, double price, bool active, string name)");
    CommandResult* create_result = execute_command(create_cmd);
    cr_assert_eq(create_result->code, 4);
    free(create_result);
    free_command(create_cmd, 0);

    Command* add1 = parse_command("ADD mydb * (100, 19.99, true, \"Product1\")");
    CommandResult* add_result1 = execute_command(add1);
    free(add_result1);
    free_command(add1, 1);

    Command* add2 = parse_command("ADD mydb * (200, 29.99, false, \"Product2\")");
    CommandResult* add_result2 = execute_command(add2);
    free(add_result2);
    free_command(add2, 1);

    Command* get_all_cmd = parse_command("GET_ALL mydb");
    CommandResult* result = execute_command(get_all_cmd);
    cr_assert_not_null(result);
    cr_assert_eq(result->code, 2);
    cr_assert_not_null(result->data);

    // Check all types are formatted correctly
    cr_assert(strstr(result->data, "100") != NULL);
    cr_assert(strstr(result->data, "200") != NULL);
    cr_assert(strstr(result->data, "19.99") != NULL);
    cr_assert(strstr(result->data, "29.99") != NULL);
    cr_assert(strstr(result->data, "true") != NULL);
    cr_assert(strstr(result->data, "false") != NULL);
    cr_assert(strstr(result->data, "Product1") != NULL);
    cr_assert(strstr(result->data, "Product2") != NULL);

    free_command_result(result);
    free_command(get_all_cmd, 0);
}

Test(execute_get_all, get_all_with_empty_strings) {
    Command* create_cmd = parse_command("CREATE mydb (string name)");
    CommandResult* create_result = execute_command(create_cmd);
    cr_assert_eq(create_result->code, 1);
    free(create_result);
    free_command(create_cmd, 0);

    Command* add_cmd = parse_command("ADD mydb * (\"\")");
    CommandResult* add_result = execute_command(add_cmd);
    free(add_result);
    free_command(add_cmd, 1);

    Command* get_all_cmd = parse_command("GET_ALL mydb");
    CommandResult* result = execute_command(get_all_cmd);
    cr_assert_not_null(result);
    cr_assert_eq(result->code, 1);
    cr_assert_not_null(result->data);

    free_command_result(result);
    free_command(get_all_cmd, 0);
}

Test(execute_get_all, get_all_after_delete) {
    Command* create_cmd = parse_command("CREATE mydb (int value)");
    CommandResult* create_result = execute_command(create_cmd);
    free(create_result);
    free_command(create_cmd, 0);

    Command* add1 = parse_command("ADD mydb * (10)");
    CommandResult* add_result1 = execute_command(add1);
    free(add_result1);
    free_command(add1, 1);

    Command* add2 = parse_command("ADD mydb * (20)");
    CommandResult* add_result2 = execute_command(add2);
    free(add_result2);
    free_command(add2, 1);

    Command* add3 = parse_command("ADD mydb * (30)");
    CommandResult* add_result3 = execute_command(add3);
    free(add_result3);
    free_command(add3, 1);

    // Delete middle row
    Command* del_cmd = parse_command("DEL mydb 2");
    CommandResult* del_result = execute_command(del_cmd);
    cr_assert_eq(del_result->code, 2);
    free(del_result);
    free_command(del_cmd, 0);

    Command* get_all_cmd = parse_command("GET_ALL mydb");
    CommandResult* result = execute_command(get_all_cmd);
    cr_assert_not_null(result);
    cr_assert_eq(result->code, 2);  // 2 rows remaining
    cr_assert(strstr(result->data, "10") != NULL);
    cr_assert(strstr(result->data, "30") != NULL);
    cr_assert(strstr(result->data, "20") == NULL);  // Deleted row

    free_command_result(result);
    free_command(get_all_cmd, 0);
}

Test(execute_get_all, get_all_only_key_field) {
    Command* create_cmd = parse_command("CREATE mydb (int age, string name)");
    CommandResult* create_result = execute_command(create_cmd);
    free(create_result);
    free_command(create_cmd, 0);

    Command* add1 = parse_command("ADD mydb * (25, \"Alice\")");
    CommandResult* add_result1 = execute_command(add1);
    free(add_result1);
    free_command(add1, 1);

    Command* add2 = parse_command("ADD mydb * (30, \"Bob\")");
    CommandResult* add_result2 = execute_command(add2);
    free(add_result2);
    free_command(add2, 1);

    Command* get_all_cmd = parse_command("GET_ALL mydb (key)");
    CommandResult* result = execute_command(get_all_cmd);
    cr_assert_not_null(result);
    cr_assert_eq(result->code, 2);
    cr_assert_not_null(result->data);

    // Only key column should be present
    cr_assert(strstr(result->data, "key") != NULL);
    cr_assert(strstr(result->data, "age") == NULL);
    cr_assert(strstr(result->data, "name") == NULL);

    free_command_result(result);
    free_command(get_all_cmd, 0);
}

// ============================================================================
// execute_count tests
// ============================================================================

TestSuite(execute_count, .init = setup, .fini = teardown);

Test(execute_count, count_empty_database) {
    Command* create_cmd = parse_command("CREATE mydb (int value)");
    CommandResult* create_result = execute_command(create_cmd);
    cr_assert_eq(create_result->code, 1);
    free(create_result);
    free_command(create_cmd, 0);

    Command* count_cmd = parse_command("COUNT mydb");
    cr_assert_not_null(count_cmd);
    cr_assert_eq(count_cmd->op, OP_COUNT);

    CommandResult* result = execute_command(count_cmd);
    cr_assert_not_null(result);
    cr_assert_eq(result->code, 0);  // 0 rows
    cr_assert_null(result->message);

    free_command_result(result);
    free_command(count_cmd, 0);
}

Test(execute_count, count_single_row) {
    Command* create_cmd = parse_command("CREATE mydb (int value)");
    CommandResult* create_result = execute_command(create_cmd);
    free(create_result);
    free_command(create_cmd, 0);

    Command* add_cmd = parse_command("ADD mydb * (42)");
    CommandResult* add_result = execute_command(add_cmd);
    free(add_result);
    free_command(add_cmd, 1);

    Command* count_cmd = parse_command("COUNT mydb");
    CommandResult* result = execute_command(count_cmd);
    cr_assert_not_null(result);
    cr_assert_eq(result->code, 1);  // 1 row
    cr_assert_null(result->message);

    free_command_result(result);
    free_command(count_cmd, 0);
}

Test(execute_count, count_multiple_rows) {
    Command* create_cmd = parse_command("CREATE mydb (int value)");
    CommandResult* create_result = execute_command(create_cmd);
    free(create_result);
    free_command(create_cmd, 0);

    for(int i = 0; i < 10; i++) {
        Command* add_cmd = parse_command("ADD mydb * (42)");
        CommandResult* add_result = execute_command(add_cmd);
        free(add_result);
        free_command(add_cmd, 1);
    }

    Command* count_cmd = parse_command("COUNT mydb");
    CommandResult* result = execute_command(count_cmd);
    cr_assert_not_null(result);
    cr_assert_eq(result->code, 10);  // 10 rows
    cr_assert_null(result->message);

    free_command_result(result);
    free_command(count_cmd, 0);
}

Test(execute_count, count_after_delete) {
    Command* create_cmd = parse_command("CREATE mydb (int value)");
    CommandResult* create_result = execute_command(create_cmd);
    free(create_result);
    free_command(create_cmd, 0);

    Command* add1 = parse_command("ADD mydb * (10)");
    CommandResult* add_result1 = execute_command(add1);
    free(add_result1);
    free_command(add1, 1);

    Command* add2 = parse_command("ADD mydb * (20)");
    CommandResult* add_result2 = execute_command(add2);
    free(add_result2);
    free_command(add2, 1);

    Command* add3 = parse_command("ADD mydb * (30)");
    CommandResult* add_result3 = execute_command(add3);
    free(add_result3);
    free_command(add3, 1);

    // Count should be 3
    Command* count1 = parse_command("COUNT mydb");
    CommandResult* result1 = execute_command(count1);
    cr_assert_eq(result1->code, 3);
    free_command_result(result1);
    free_command(count1, 0);

    // Delete one row
    Command* del_cmd = parse_command("DEL mydb 2");
    CommandResult* del_result = execute_command(del_cmd);
    free(del_result);
    free_command(del_cmd, 0);

    // Count should be 2
    Command* count2 = parse_command("COUNT mydb");
    CommandResult* result2 = execute_command(count2);
    cr_assert_eq(result2->code, 2);
    free_command_result(result2);
    free_command(count2, 0);
}

Test(execute_count, count_database_not_found) {
    Command* count_cmd = parse_command("COUNT nonexistent");
    cr_assert_not_null(count_cmd);

    CommandResult* result = execute_command(count_cmd);
    cr_assert_not_null(result);
    cr_assert(result->code < 0);
    cr_assert_not_null(result->message);

    free_command_result(result);
    free_command(count_cmd, 0);
}

Test(execute_count, count_different_databases) {
    Command* create1 = parse_command("CREATE db1 (int value)");
    CommandResult* create_result1 = execute_command(create1);
    free(create_result1);
    free_command(create1, 0);

    Command* create2 = parse_command("CREATE db2 (int value)");
    CommandResult* create_result2 = execute_command(create2);
    free(create_result2);
    free_command(create2, 0);

    // Add 3 rows to db1
    for(int i = 0; i < 3; i++) {
        Command* add_cmd = parse_command("ADD db1 * (42)");
        CommandResult* add_result = execute_command(add_cmd);
        free(add_result);
        free_command(add_cmd, 1);
    }

    // Add 5 rows to db2
    for(int i = 0; i < 5; i++) {
        Command* add_cmd = parse_command("ADD db2 * (99)");
        CommandResult* add_result = execute_command(add_cmd);
        free(add_result);
        free_command(add_cmd, 1);
    }

    // Count db1
    Command* count1 = parse_command("COUNT db1");
    CommandResult* result1 = execute_command(count1);
    cr_assert_eq(result1->code, 3);
    free_command_result(result1);
    free_command(count1, 0);

    // Count db2
    Command* count2 = parse_command("COUNT db2");
    CommandResult* result2 = execute_command(count2);
    cr_assert_eq(result2->code, 5);
    free_command_result(result2);
    free_command(count2, 0);
}

Test(execute_count, count_after_update) {
    Command* create_cmd = parse_command("CREATE mydb (int value)");
    CommandResult* create_result = execute_command(create_cmd);
    free(create_result);
    free_command(create_cmd, 0);

    Command* add1 = parse_command("ADD mydb * (10)");
    CommandResult* add_result1 = execute_command(add1);
    free(add_result1);
    free_command(add1, 1);

    Command* add2 = parse_command("ADD mydb * (20)");
    CommandResult* add_result2 = execute_command(add2);
    free(add_result2);
    free_command(add2, 1);

    // Update doesn't change count
    Command* up_cmd = parse_command("UP mydb 1 (99)");
    CommandResult* up_result = execute_command(up_cmd);
    free(up_result);
    free_command(up_cmd, 1);

    Command* count_cmd = parse_command("COUNT mydb");
    CommandResult* result = execute_command(count_cmd);
    cr_assert_eq(result->code, 2);  // Still 2 rows
    free_command_result(result);
    free_command(count_cmd, 0);
}

Test(execute_count, count_all_deleted) {
    Command* create_cmd = parse_command("CREATE mydb (int value)");
    CommandResult* create_result = execute_command(create_cmd);
    free(create_result);
    free_command(create_cmd, 0);

    Command* add1 = parse_command("ADD mydb * (10)");
    CommandResult* add_result1 = execute_command(add1);
    free(add_result1);
    free_command(add1, 1);

    Command* add2 = parse_command("ADD mydb * (20)");
    CommandResult* add_result2 = execute_command(add2);
    free(add_result2);
    free_command(add2, 1);

    // Delete all rows
    Command* del1 = parse_command("DEL mydb 1");
    CommandResult* del_result1 = execute_command(del1);
    free(del_result1);
    free_command(del1, 0);

    Command* del2 = parse_command("DEL mydb 2");
    CommandResult* del_result2 = execute_command(del2);
    free(del_result2);
    free_command(del2, 0);

    // Count should be 0
    Command* count_cmd = parse_command("COUNT mydb");
    CommandResult* result = execute_command(count_cmd);
    cr_assert_eq(result->code, 0);
    free_command_result(result);
    free_command(count_cmd, 0);
}

// ============================================================================
// execute_search tests
// ============================================================================

TestSuite(execute_search, .init = setup, .fini = teardown);

Test(execute_search, search_int_field_single_match) {
    Command* create_cmd = parse_command("CREATE mydb (int age, string name)");
    CommandResult* create_result = execute_command(create_cmd);
    free(create_result);
    free_command(create_cmd, 0);

    Command* add1 = parse_command("ADD mydb * (25, \"Alice\")");
    CommandResult* add_result1 = execute_command(add1);
    free(add_result1);
    free_command(add1, 1);

    Command* add2 = parse_command("ADD mydb * (30, \"Bob\")");
    CommandResult* add_result2 = execute_command(add2);
    free(add_result2);
    free_command(add2, 1);

    Command* add3 = parse_command("ADD mydb * (25, \"Charlie\")");
    CommandResult* add_result3 = execute_command(add3);
    free(add_result3);
    free_command(add3, 1);

    // Search for age = 30
    Command* search_cmd = parse_command("SEARCH mydb age 30");
    CommandResult* result = execute_command(search_cmd);
    cr_assert_not_null(result);
    cr_assert_eq(result->code, 1);  // 1 match
    cr_assert_null(result->message);
    cr_assert_not_null(result->data);
    cr_assert(strstr(result->data, "Bob") != NULL);
    cr_assert(strstr(result->data, "30") != NULL);
    free_command_result(result);
    free_command(search_cmd, 0);
}

Test(execute_search, search_int_field_multiple_matches) {
    Command* create_cmd = parse_command("CREATE mydb (int age, string name)");
    CommandResult* create_result = execute_command(create_cmd);
    free(create_result);
    free_command(create_cmd, 0);

    Command* add1 = parse_command("ADD mydb * (25, \"Alice\")");
    CommandResult* add_result1 = execute_command(add1);
    free(add_result1);
    free_command(add1, 1);

    Command* add2 = parse_command("ADD mydb * (30, \"Bob\")");
    CommandResult* add_result2 = execute_command(add2);
    free(add_result2);
    free_command(add2, 1);

    Command* add3 = parse_command("ADD mydb * (25, \"Charlie\")");
    CommandResult* add_result3 = execute_command(add3);
    free(add_result3);
    free_command(add3, 1);

    // Search for age = 25
    Command* search_cmd = parse_command("SEARCH mydb age 25");
    CommandResult* result = execute_command(search_cmd);
    cr_assert_not_null(result);
    cr_assert_eq(result->code, 2);  // 2 matches
    cr_assert_null(result->message);
    cr_assert_not_null(result->data);
    cr_assert(strstr(result->data, "Alice") != NULL);
    cr_assert(strstr(result->data, "Charlie") != NULL);
    free_command_result(result);
    free_command(search_cmd, 0);
}

Test(execute_search, search_string_field) {
    Command* create_cmd = parse_command("CREATE mydb (string city, int population)");
    CommandResult* create_result = execute_command(create_cmd);
    free(create_result);
    free_command(create_cmd, 0);

    Command* add1 = parse_command("ADD mydb * (\"Paris\", 2000000)");
    CommandResult* add_result1 = execute_command(add1);
    free(add_result1);
    free_command(add1, 1);

    Command* add2 = parse_command("ADD mydb * (\"London\", 9000000)");
    CommandResult* add_result2 = execute_command(add2);
    free(add_result2);
    free_command(add2, 1);

    Command* add3 = parse_command("ADD mydb * (\"Paris\", 500000)");
    CommandResult* add_result3 = execute_command(add3);
    free(add_result3);
    free_command(add3, 1);

    // Search for city = "Paris"
    Command* search_cmd = parse_command("SEARCH mydb city \"Paris\"");
    CommandResult* result = execute_command(search_cmd);
    cr_assert_not_null(result);
    cr_assert_eq(result->code, 2);  // 2 matches
    cr_assert_null(result->message);
    cr_assert_not_null(result->data);
    cr_assert(strstr(result->data, "2000000") != NULL);
    cr_assert(strstr(result->data, "500000") != NULL);
    free_command_result(result);
    free_command(search_cmd, 0);
}

Test(execute_search, search_bool_field) {
    Command* create_cmd = parse_command("CREATE mydb (string name, bool active)");
    CommandResult* create_result = execute_command(create_cmd);
    free(create_result);
    free_command(create_cmd, 0);

    Command* add1 = parse_command("ADD mydb * (\"Alice\", true)");
    CommandResult* add_result1 = execute_command(add1);
    free(add_result1);
    free_command(add1, 1);

    Command* add2 = parse_command("ADD mydb * (\"Bob\", false)");
    CommandResult* add_result2 = execute_command(add2);
    free(add_result2);
    free_command(add2, 1);

    Command* add3 = parse_command("ADD mydb * (\"Charlie\", true)");
    CommandResult* add_result3 = execute_command(add3);
    free(add_result3);
    free_command(add3, 1);

    // Search for active = true
    Command* search_cmd = parse_command("SEARCH mydb active true");
    CommandResult* result = execute_command(search_cmd);
    cr_assert_not_null(result);
    cr_assert_eq(result->code, 2);  // 2 matches
    cr_assert_null(result->message);
    cr_assert_not_null(result->data);
    cr_assert(strstr(result->data, "Alice") != NULL);
    cr_assert(strstr(result->data, "Charlie") != NULL);
    free_command_result(result);
    free_command(search_cmd, 0);
}

Test(execute_search, search_double_field) {
    Command* create_cmd = parse_command("CREATE mydb (string item, double price)");
    CommandResult* create_result = execute_command(create_cmd);
    free(create_result);
    free_command(create_cmd, 0);

    Command* add1 = parse_command("ADD mydb * (\"Apple\", 1.50)");
    CommandResult* add_result1 = execute_command(add1);
    free(add_result1);
    free_command(add1, 1);

    Command* add2 = parse_command("ADD mydb * (\"Banana\", 0.75)");
    CommandResult* add_result2 = execute_command(add2);
    free(add_result2);
    free_command(add2, 1);

    Command* add3 = parse_command("ADD mydb * (\"Orange\", 1.50)");
    CommandResult* add_result3 = execute_command(add3);
    free(add_result3);
    free_command(add3, 1);

    // Search for price = 1.50
    Command* search_cmd = parse_command("SEARCH mydb price 1.50");
    CommandResult* result = execute_command(search_cmd);
    cr_assert_not_null(result);
    cr_assert_eq(result->code, 2);  // 2 matches
    cr_assert_null(result->message);
    cr_assert_not_null(result->data);
    cr_assert(strstr(result->data, "Apple") != NULL);
    cr_assert(strstr(result->data, "Orange") != NULL);
    free_command_result(result);
    free_command(search_cmd, 0);
}

Test(execute_search, search_no_matches) {
    Command* create_cmd = parse_command("CREATE mydb (int value)");
    CommandResult* create_result = execute_command(create_cmd);
    free(create_result);
    free_command(create_cmd, 0);

    Command* add1 = parse_command("ADD mydb * (10)");
    CommandResult* add_result1 = execute_command(add1);
    free(add_result1);
    free_command(add1, 1);

    Command* add2 = parse_command("ADD mydb * (20)");
    CommandResult* add_result2 = execute_command(add2);
    free(add_result2);
    free_command(add2, 1);

    // Search for value = 999 (no match)
    Command* search_cmd = parse_command("SEARCH mydb value 999");
    CommandResult* result = execute_command(search_cmd);
    cr_assert_not_null(result);
    cr_assert_eq(result->code, 0);  // 0 matches
    cr_assert_null(result->message);
    cr_assert_not_null(result->data);
    cr_assert(strstr(result->data, "(No rows)") != NULL);
    free_command_result(result);
    free_command(search_cmd, 0);
}

Test(execute_search, search_with_specific_return_fields) {
    Command* create_cmd = parse_command("CREATE mydb (int age, string name, string city)");
    CommandResult* create_result = execute_command(create_cmd);
    free(create_result);
    free_command(create_cmd, 0);

    Command* add1 = parse_command("ADD mydb * (25, \"Alice\", \"Paris\")");
    CommandResult* add_result1 = execute_command(add1);
    free(add_result1);
    free_command(add1, 1);

    Command* add2 = parse_command("ADD mydb * (30, \"Bob\", \"London\")");
    CommandResult* add_result2 = execute_command(add2);
    free(add_result2);
    free_command(add2, 1);

    // Search for age = 25 and return only name and city
    Command* search_cmd = parse_command("SEARCH mydb age 25 (name, city)");
    CommandResult* result = execute_command(search_cmd);
    cr_assert_not_null(result);
    cr_assert_eq(result->code, 1);  // 1 match
    cr_assert_null(result->message);
    cr_assert_not_null(result->data);
    cr_assert(strstr(result->data, "Alice") != NULL);
    cr_assert(strstr(result->data, "Paris") != NULL);
    // Should not have key or age columns in output
    cr_assert(strstr(result->data, "| key") == NULL);
    free_command_result(result);
    free_command(search_cmd, 0);
}

Test(execute_search, search_database_not_found) {
    Command* search_cmd = parse_command("SEARCH nonexistent age 25");
    CommandResult* result = execute_command(search_cmd);
    cr_assert_not_null(result);
    cr_assert_eq(result->code, -1);
    cr_assert_not_null(result->message);
    free_command_result(result);
    free_command(search_cmd, 0);
}

Test(execute_search, search_invalid_field) {
    Command* create_cmd = parse_command("CREATE mydb (int value)");
    CommandResult* create_result = execute_command(create_cmd);
    free(create_result);
    free_command(create_cmd, 0);

    Command* search_cmd = parse_command("SEARCH mydb nonexistent 10");
    CommandResult* result = execute_command(search_cmd);
    cr_assert_not_null(result);
    cr_assert_eq(result->code, -1);
    cr_assert_not_null(result->message);
    free_command_result(result);
    free_command(search_cmd, 0);
}

Test(execute_search, search_empty_database) {
    Command* create_cmd = parse_command("CREATE mydb (int value)");
    CommandResult* create_result = execute_command(create_cmd);
    free(create_result);
    free_command(create_cmd, 0);

    Command* search_cmd = parse_command("SEARCH mydb value 10");
    CommandResult* result = execute_command(search_cmd);
    cr_assert_not_null(result);
    cr_assert_eq(result->code, 0);  // 0 matches
    cr_assert_null(result->message);
    cr_assert(strstr(result->data, "(No rows)") != NULL);
    free_command_result(result);
    free_command(search_cmd, 0);
}

Test(execute_search, search_by_key_field) {
    Command* create_cmd = parse_command("CREATE mydb (string name)");
    CommandResult* create_result = execute_command(create_cmd);
    free(create_result);
    free_command(create_cmd, 0);

    Command* add1 = parse_command("ADD mydb * (\"Alice\")");
    CommandResult* add_result1 = execute_command(add1);
    free(add_result1);
    free_command(add1, 1);

    Command* add2 = parse_command("ADD mydb * (\"Bob\")");
    CommandResult* add_result2 = execute_command(add2);
    free(add_result2);
    free_command(add2, 1);

    // Search by key field
    Command* search_cmd = parse_command("SEARCH mydb key 2");
    CommandResult* result = execute_command(search_cmd);
    cr_assert_not_null(result);
    cr_assert_eq(result->code, 1);  // 1 match
    cr_assert(strstr(result->data, "Bob") != NULL);
    free_command_result(result);
    free_command(search_cmd, 0);
}

Test(execute_search, search_invalid_return_field) {
    Command* create_cmd = parse_command("CREATE mydb (int value)");
    CommandResult* create_result = execute_command(create_cmd);
    free(create_result);
    free_command(create_cmd, 0);

    Command* add1 = parse_command("ADD mydb * (10)");
    CommandResult* add_result1 = execute_command(add1);
    free(add_result1);
    free_command(add1, 1);

    Command* search_cmd = parse_command("SEARCH mydb value 10 (nonexistent)");
    CommandResult* result = execute_command(search_cmd);
    cr_assert_not_null(result);
    cr_assert_eq(result->code, -1);  // Error
    cr_assert_not_null(result->message);
    free_command_result(result);
    free_command(search_cmd, 0);
}

// ============================================================================
// execute_create_index tests
// ============================================================================

TestSuite(execute_create_index, .init = setup, .fini = teardown);

Test(execute_create_index, create_index_success) {
    Command* create_cmd = parse_command("CREATE mydb (int age, string name)");
    CommandResult* create_result = execute_command(create_cmd);
    cr_assert_eq(create_result->code, 2);
    free(create_result);
    free_command(create_cmd, 0);

    Command* idx_cmd = parse_command("CREATE_INDEX mydb age");
    cr_assert_not_null(idx_cmd);
    cr_assert_eq(idx_cmd->op, OP_CREATE_INDEX);

    CommandResult* idx_result = execute_command(idx_cmd);
    cr_assert_not_null(idx_result);
    cr_assert_eq(idx_result->code, 1);
    cr_assert_null(idx_result->message);

    DB* db = find_db("mydb");
    cr_assert_not_null(db);
    cr_assert_eq(db_has_index(db, 1), 1);

    free_command_result(idx_result);
    free_command(idx_cmd, 0);
}

Test(execute_create_index, create_index_duplicate_fails) {
    Command* create_cmd = parse_command("CREATE mydb (int age, string name)");
    CommandResult* create_result = execute_command(create_cmd);
    free(create_result);
    free_command(create_cmd, 0);

    Command* idx_cmd1 = parse_command("CREATE_INDEX mydb age");
    CommandResult* idx_result1 = execute_command(idx_cmd1);
    cr_assert_eq(idx_result1->code, 1);
    free_command_result(idx_result1);
    free_command(idx_cmd1, 0);

    Command* idx_cmd2 = parse_command("CREATE_INDEX mydb age");
    CommandResult* idx_result2 = execute_command(idx_cmd2);
    cr_assert_not_null(idx_result2);
    cr_assert_eq(idx_result2->code, -2);
    cr_assert_not_null(idx_result2->message);

    free_command_result(idx_result2);
    free_command(idx_cmd2, 0);
}

Test(execute_create_index, create_index_database_not_found) {
    Command* idx_cmd = parse_command("CREATE_INDEX nonexistent age");
    CommandResult* idx_result = execute_command(idx_cmd);
    cr_assert_not_null(idx_result);
    cr_assert_eq(idx_result->code, -1);
    cr_assert_not_null(idx_result->message);

    free_command_result(idx_result);
    free_command(idx_cmd, 0);
}

Test(execute_create_index, create_index_invalid_field) {
    Command* create_cmd = parse_command("CREATE mydb (int value)");
    CommandResult* create_result = execute_command(create_cmd);
    free(create_result);
    free_command(create_cmd, 0);

    Command* idx_cmd = parse_command("CREATE_INDEX mydb nonexistent");
    CommandResult* idx_result = execute_command(idx_cmd);
    cr_assert_not_null(idx_result);
    cr_assert_eq(idx_result->code, -1);
    cr_assert_not_null(idx_result->message);

    free_command_result(idx_result);
    free_command(idx_cmd, 0);
}

Test(execute_create_index, indexed_search_consistent_after_add_update_delete) {
    Command* create_cmd = parse_command("CREATE mydb (int age, string name)");
    CommandResult* create_result = execute_command(create_cmd);
    free(create_result);
    free_command(create_cmd, 0);

    Command* add1 = parse_command("ADD mydb * (25, \"Alice\")");
    CommandResult* add_result1 = execute_command(add1);
    free(add_result1);
    free_command(add1, 1);

    Command* add2 = parse_command("ADD mydb * (30, \"Bob\")");
    CommandResult* add_result2 = execute_command(add2);
    free(add_result2);
    free_command(add2, 1);

    Command* idx_cmd = parse_command("CREATE_INDEX mydb age");
    CommandResult* idx_result = execute_command(idx_cmd);
    cr_assert_eq(idx_result->code, 1);
    free_command_result(idx_result);
    free_command(idx_cmd, 0);

    Command* search1 = parse_command("SEARCH mydb age 25");
    CommandResult* search_result1 = execute_command(search1);
    cr_assert_eq(search_result1->code, 1);
    cr_assert(strstr(search_result1->data, "Alice") != NULL);
    free_command_result(search_result1);
    free_command(search1, 0);

    Command* up_cmd = parse_command("UP mydb 2 (25, _)");
    CommandResult* up_result = execute_command(up_cmd);
    cr_assert_eq(up_result->code, 2);
    free_command_result(up_result);
    free_command(up_cmd, 1);

    Command* search2 = parse_command("SEARCH mydb age 25");
    CommandResult* search_result2 = execute_command(search2);
    cr_assert_eq(search_result2->code, 2);
    cr_assert(strstr(search_result2->data, "Alice") != NULL);
    cr_assert(strstr(search_result2->data, "Bob") != NULL);
    free_command_result(search_result2);
    free_command(search2, 0);

    Command* del_cmd = parse_command("DEL mydb 1");
    CommandResult* del_result = execute_command(del_cmd);
    cr_assert_eq(del_result->code, 1);
    free_command_result(del_result);
    free_command(del_cmd, 0);

    Command* search3 = parse_command("SEARCH mydb age 25");
    CommandResult* search_result3 = execute_command(search3);
    cr_assert_eq(search_result3->code, 1);
    cr_assert(strstr(search_result3->data, "Bob") != NULL);
    cr_assert(strstr(search_result3->data, "Alice") == NULL);
    free_command_result(search_result3);
    free_command(search3, 0);
}

// ============================================================================
// Limit enforcement tests
// ============================================================================

TestSuite(execute_limits, .init = setup, .fini = teardown);

Test(execute_limits, create_too_many_fields_ops_safety_net) {
    /* Build a CreateData with MAX_FIELDS_PER_DB + 1 user fields directly,
       bypassing the parser, to exercise the operations safety net. */
    int    n      = MAX_FIELDS_PER_DB + 1;
    Field* fields = (Field*)malloc(sizeof(Field) * n);
    cr_assert_not_null(fields);
    for(int i = 0; i < n; i++) {
        snprintf(fields[i].name, MAX_FIELD_NAME_LENGTH, "f%d", i);
        fields[i].type = TYPE_INT;
    }

    Command cmd;
    cmd.op                        = OP_CREATE;
    cmd.data.create.fieldCount    = n;
    cmd.data.create.fields        = fields;
    strncpy(cmd.data.create.dbName, "overload", MAX_DB_NAME_LENGTH - 1);
    cmd.data.create.dbName[MAX_DB_NAME_LENGTH - 1] = '\0';

    CommandResult* result = execute_command(&cmd);
    cr_assert_not_null(result);
    cr_assert(result->code < 0);
    cr_assert_not_null(result->message);
    free(result);
    free(fields);
}

Test(execute_limits, max_db_count_reached) {
    /* Create MAX_DB_COUNT databases and verify the next one fails. */
    char dbname[MAX_DB_NAME_LENGTH];
    for(int i = 0; i < MAX_DB_COUNT; i++) {
        snprintf(dbname, sizeof(dbname), "db%d", i);
        char cmdstr[128];
        snprintf(cmdstr, sizeof(cmdstr), "CREATE %s (int x)", dbname);
        Command* cmd = parse_command(cmdstr);
        cr_assert_not_null(cmd);
        CommandResult* res = execute_command(cmd);
        cr_assert_not_null(res);
        cr_assert(res->code >= 0);
        free(res);
        free_command(cmd, 0);
    }

    /* The (MAX_DB_COUNT + 1)-th database must fail */
    Command* cmd = parse_command("CREATE onemore (int x)");
    cr_assert_not_null(cmd);
    CommandResult* res = execute_command(cmd);
    cr_assert_not_null(res);
    cr_assert(res->code < 0);
    cr_assert_not_null(res->message);
    free(res);
    free_command(cmd, 0);
}

Test(execute_limits, add_string_too_long_ops_safety_net) {
    /* Create a DB with a string field, then try to add a row whose string
       value exceeds MAX_STRING_VALUE_LENGTH via the operations layer. */
    Command* create_cmd = parse_command("CREATE strdb (string val)");
    cr_assert_not_null(create_cmd);
    CommandResult* cr_res = execute_command(create_cmd);
    cr_assert_not_null(cr_res);
    cr_assert(cr_res->code >= 0);
    free(cr_res);
    free_command(create_cmd, 0);

    /* Build an AddData manually with an oversized string */
    int    slen = MAX_STRING_VALUE_LENGTH + 1;
    char*  str  = (char*)malloc(slen + 1);
    cr_assert_not_null(str);
    memset(str, 'x', slen);
    str[slen] = '\0';

    Data val;
    val.size    = slen;
    val.value.s = str;

    Command cmd;
    cmd.op                    = OP_ADD;
    cmd.data.add.autoKey      = 1;
    cmd.data.add.key          = 0;
    cmd.data.add.valueCount   = 1;
    cmd.data.add.values       = &val;
    strncpy(cmd.data.add.dbName, "strdb", MAX_DB_NAME_LENGTH - 1);
    cmd.data.add.dbName[MAX_DB_NAME_LENGTH - 1] = '\0';

    CommandResult* result = execute_command(&cmd);
    cr_assert_not_null(result);
    cr_assert(result->code < 0);
    cr_assert_not_null(result->message);
    free(result);
    free(str);
}
