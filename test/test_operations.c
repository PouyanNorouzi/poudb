#include <criterion/criterion.h>
#include <criterion/new/assert.h>

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
