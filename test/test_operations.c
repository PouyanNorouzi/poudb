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
    cr_assert_eq(db->rowsCapacity, INITIAL_ROW_CAPACITY);
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
    cr_assert_eq(db->rows[0].values[0].value.i, 1);  // key
    cr_assert_eq(db->rows[0].values[1].value.i, 42); // value

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
    cr_assert_str_eq(db->rows[0].values[1].value.s, "hello");

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
    cr_assert_eq(db->rows[0].values[0].value.i, 100);
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
    cr_assert_eq(db->rows[0].values[0].value.i, 1);       // key
    cr_assert_eq(db->rows[0].values[1].value.i, 25);      // age
    cr_assert_str_eq(db->rows[0].values[2].value.s, "John Doe"); // name
    cr_assert_float_eq(db->rows[0].values[3].value.d, 50000.50, 0.01); // salary
    cr_assert_eq(db->rows[0].values[4].value.b, true);    // active

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
    cr_assert_eq(db->rows[0].values[1].value.i, -42);
    cr_assert_float_eq(db->rows[0].values[2].value.d, -3.14, 0.01);

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
    cr_assert_eq(db->rows[0].values[1].value.b, true);
    cr_assert_eq(db->rows[0].values[2].value.b, false);

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
    cr_assert_str_eq(db->rows[0].values[1].value.s, "");

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
    cr_assert_str_eq(db->rows[0].values[1].value.s, "hello world with spaces");

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
    cr_assert_eq(db->rows[0].values[0].value.i, 0);
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
    cr_assert_float_eq(db->rows[0].values[1].value.d, 1500.0, 0.01);

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
    cr_assert_eq(db->rows[0].values[1].value.i, 100);

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
    cr_assert_str_eq(db->rows[0].values[1].value.s, "world");

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
    cr_assert_eq(db->rows[0].values[1].value.i, 30);
    cr_assert_str_eq(db->rows[0].values[2].value.s, "Jane");
    cr_assert_float_eq(db->rows[0].values[3].value.d, 60000.0, 0.01);
    cr_assert_eq(db->rows[0].values[4].value.b, false);

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
    cr_assert_eq(db->rows[0].values[1].value.i, 20);  // Updated
    cr_assert_str_eq(db->rows[0].values[2].value.s, "original");  // Unchanged
    cr_assert_eq(db->rows[0].values[3].value.i, 40);  // Updated

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
    cr_assert_eq(db->rows[0].values[1].value.i, 10);
    cr_assert_eq(db->rows[0].values[2].value.i, 20);

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
    cr_assert_eq(db->rows[0].values[1].value.i, -42);
    cr_assert_float_eq(db->rows[0].values[2].value.d, -3.14, 0.01);

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
    cr_assert_eq(db->rows[0].values[1].value.b, false);
    cr_assert_eq(db->rows[0].values[2].value.b, true);

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
    cr_assert_str_eq(db->rows[0].values[1].value.s, "");

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
    cr_assert_eq(db->rows[0].values[1].value.i, 999);

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
    cr_assert_eq(db->rows[0].values[1].value.i, 10);   // Unchanged
    cr_assert_eq(db->rows[1].values[1].value.i, 200); // Updated
    cr_assert_eq(db->rows[2].values[1].value.i, 30);  // Unchanged
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
    cr_assert_eq(db->rows[0].values[0].value.i, 1);
    cr_assert_eq(db->rows[0].values[1].value.i, 10);
    cr_assert_eq(db->rows[1].values[0].value.i, 3);
    cr_assert_eq(db->rows[1].values[1].value.i, 30);
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
    cr_assert_eq(db->rows[0].values[0].value.i, 2);
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
    cr_assert_eq(db->rows[0].values[0].value.i, 1);
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
    cr_assert_eq(db->rows[0].values[0].value.i, 2);
    cr_assert_eq(db->rows[0].values[1].value.i, 99);
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
