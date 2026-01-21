#include <criterion/criterion.h>
#include <criterion/new/assert.h>

#include "db/parser.h"

/**
 * Test suite for parse_create function
 */

// ============================================================================
// Valid CREATE command tests
// ============================================================================

Test(parse_create, single_int_field) {
    Command* cmd = parse_command("CREATE mydb (int id)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_CREATE);
    cr_assert_str_eq(cmd->data.create.dbName, "mydb");
    cr_assert_eq(cmd->data.create.fieldCount, 1);
    cr_assert_str_eq(cmd->data.create.fields[0].name, "id");
    cr_assert_eq(cmd->data.create.fields[0].type, TYPE_INT);

    free_command(cmd, 0);
}

Test(parse_create, single_string_field) {
    Command* cmd = parse_command("CREATE users (string name)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_CREATE);
    cr_assert_str_eq(cmd->data.create.dbName, "users");
    cr_assert_eq(cmd->data.create.fieldCount, 1);
    cr_assert_str_eq(cmd->data.create.fields[0].name, "name");
    cr_assert_eq(cmd->data.create.fields[0].type, TYPE_STRING);

    free_command(cmd, 0);
}

Test(parse_create, single_double_field) {
    Command* cmd = parse_command("CREATE prices (double amount)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_CREATE);
    cr_assert_str_eq(cmd->data.create.dbName, "prices");
    cr_assert_eq(cmd->data.create.fieldCount, 1);
    cr_assert_str_eq(cmd->data.create.fields[0].name, "amount");
    cr_assert_eq(cmd->data.create.fields[0].type, TYPE_DOUBLE);

    free_command(cmd, 0);
}

Test(parse_create, single_bool_field) {
    Command* cmd = parse_command("CREATE flags (bool active)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_CREATE);
    cr_assert_str_eq(cmd->data.create.dbName, "flags");
    cr_assert_eq(cmd->data.create.fieldCount, 1);
    cr_assert_str_eq(cmd->data.create.fields[0].name, "active");
    cr_assert_eq(cmd->data.create.fields[0].type, TYPE_BOOL);

    free_command(cmd, 0);
}

Test(parse_create, multiple_fields) {
    Command* cmd = parse_command(
        "CREATE users (int id, string name, double salary, bool active)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_CREATE);
    cr_assert_str_eq(cmd->data.create.dbName, "users");
    cr_assert_eq(cmd->data.create.fieldCount, 4);

    cr_assert_str_eq(cmd->data.create.fields[0].name, "id");
    cr_assert_eq(cmd->data.create.fields[0].type, TYPE_INT);

    cr_assert_str_eq(cmd->data.create.fields[1].name, "name");
    cr_assert_eq(cmd->data.create.fields[1].type, TYPE_STRING);

    cr_assert_str_eq(cmd->data.create.fields[2].name, "salary");
    cr_assert_eq(cmd->data.create.fields[2].type, TYPE_DOUBLE);

    cr_assert_str_eq(cmd->data.create.fields[3].name, "active");
    cr_assert_eq(cmd->data.create.fields[3].type, TYPE_BOOL);

    free_command(cmd, 0);
}

Test(parse_create, case_insensitive_command) {
    Command* cmd = parse_command("create mydb (int id)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_CREATE);
    cr_assert_str_eq(cmd->data.create.dbName, "mydb");

    free_command(cmd, 0);
}

Test(parse_create, case_insensitive_types) {
    Command* cmd = parse_command(
        "CREATE mydb (INT id, STRING name, DOUBLE price, BOOL flag)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_CREATE);
    cr_assert_eq(cmd->data.create.fieldCount, 4);
    cr_assert_eq(cmd->data.create.fields[0].type, TYPE_INT);
    cr_assert_eq(cmd->data.create.fields[1].type, TYPE_STRING);
    cr_assert_eq(cmd->data.create.fields[2].type, TYPE_DOUBLE);
    cr_assert_eq(cmd->data.create.fields[3].type, TYPE_BOOL);

    free_command(cmd, 0);
}

Test(parse_create, extra_whitespace) {
    Command* cmd =
        parse_command("  CREATE   mydb   (  int   id  ,  string   name  )  ");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_CREATE);
    cr_assert_str_eq(cmd->data.create.dbName, "mydb");
    cr_assert_eq(cmd->data.create.fieldCount, 2);

    free_command(cmd, 0);
}

Test(parse_create, underscore_in_names) {
    Command* cmd =
        parse_command("CREATE my_database (int user_id, string first_name)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_CREATE);
    cr_assert_str_eq(cmd->data.create.dbName, "my_database");
    cr_assert_str_eq(cmd->data.create.fields[0].name, "user_id");
    cr_assert_str_eq(cmd->data.create.fields[1].name, "first_name");

    free_command(cmd, 0);
}

Test(parse_create, db_name_starts_with_underscore) {
    Command* cmd = parse_command("CREATE _private (int id)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_CREATE);
    cr_assert_str_eq(cmd->data.create.dbName, "_private");

    free_command(cmd, 0);
}

// ============================================================================
// Invalid CREATE command tests - Error cases
// ============================================================================

Test(parse_create, missing_db_name) {
    Command* cmd = parse_command("CREATE (int id)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_create, missing_parentheses) {
    Command* cmd = parse_command("CREATE mydb int id");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_create, empty_field_list) {
    Command* cmd = parse_command("CREATE mydb ()");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_create, missing_field_name) {
    Command* cmd = parse_command("CREATE mydb (int)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_create, missing_field_type) {
    Command* cmd = parse_command("CREATE mydb (id)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_create, invalid_field_type) {
    Command* cmd = parse_command("CREATE mydb (varchar name)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_create, invalid_db_name_starts_with_number) {
    Command* cmd = parse_command("CREATE 123db (int id)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_create, invalid_field_name_starts_with_number) {
    Command* cmd = parse_command("CREATE mydb (int 123field)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_create, unmatched_opening_paren) {
    Command* cmd = parse_command("CREATE mydb (int id");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_create, invalid_db_name_with_special_chars) {
    Command* cmd = parse_command("CREATE my-db (int id)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_create, invalid_field_name_with_special_chars) {
    Command* cmd = parse_command("CREATE mydb (int user-id)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_create, null_input) {
    Command* cmd = parse_command(NULL);

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_create, empty_input) {
    Command* cmd = parse_command("");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_create, only_whitespace) {
    Command* cmd = parse_command("   ");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_create, invalid_command) {
    Command* cmd = parse_command("INVALID mydb (int id)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

// ============================================================================
// Test suite for parse_add function
// ============================================================================

// Helper function to free add command memory
static void free_add_command(Command* cmd) {
    if (cmd == NULL) return;
    if (cmd->op == OP_ADD && cmd->data.add.values != NULL) {
        for (int i = 0; i < cmd->data.add.valueCount; i++) {
            // Free strings (size > 0 indicates string)
            if (cmd->data.add.values[i].size > 0 && cmd->data.add.values[i].value.s != NULL) {
                free((void*)cmd->data.add.values[i].value.s);
            }
        }
        free(cmd->data.add.values);
    }
    free(cmd);
}

// ============================================================================
// Valid ADD command tests
// ============================================================================

Test(parse_add, single_int_value) {
    Command* cmd = parse_command("ADD mydb 1 (42)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ADD);
    cr_assert_str_eq(cmd->data.add.dbName, "mydb");
    cr_assert_eq(cmd->data.add.key, 1);
    cr_assert_eq(cmd->data.add.autoKey, 0);
    cr_assert_eq(cmd->data.add.valueCount, 1);
    cr_assert_eq(cmd->data.add.values[0].value.i, 42);

    free_add_command(cmd);
}

Test(parse_add, single_string_value) {
    Command* cmd = parse_command("ADD users 5 (\"John Doe\")");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ADD);
    cr_assert_str_eq(cmd->data.add.dbName, "users");
    cr_assert_eq(cmd->data.add.key, 5);
    cr_assert_eq(cmd->data.add.valueCount, 1);
    cr_assert_str_eq(cmd->data.add.values[0].value.s, "John Doe");

    free_add_command(cmd);
}

Test(parse_add, single_double_value) {
    Command* cmd = parse_command("ADD prices 1 (3.14)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ADD);
    cr_assert_str_eq(cmd->data.add.dbName, "prices");
    cr_assert_eq(cmd->data.add.valueCount, 1);
    cr_assert_float_eq(cmd->data.add.values[0].value.d, 3.14, 0.001);

    free_add_command(cmd);
}

Test(parse_add, single_bool_true) {
    Command* cmd = parse_command("ADD flags 1 (true)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ADD);
    cr_assert_str_eq(cmd->data.add.dbName, "flags");
    cr_assert_eq(cmd->data.add.valueCount, 1);
    cr_assert_eq(cmd->data.add.values[0].value.b, 1);

    free_add_command(cmd);
}

Test(parse_add, single_bool_false) {
    Command* cmd = parse_command("ADD flags 1 (false)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ADD);
    cr_assert_str_eq(cmd->data.add.dbName, "flags");
    cr_assert_eq(cmd->data.add.valueCount, 1);
    cr_assert_eq(cmd->data.add.values[0].value.b, 0);

    free_add_command(cmd);
}

Test(parse_add, auto_key) {
    Command* cmd = parse_command("ADD mydb * (100)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ADD);
    cr_assert_str_eq(cmd->data.add.dbName, "mydb");
    cr_assert_eq(cmd->data.add.autoKey, 1);
    cr_assert_eq(cmd->data.add.valueCount, 1);

    free_add_command(cmd);
}

Test(parse_add, multiple_values_mixed_types) {
    Command* cmd = parse_command("ADD users 10 (42, \"Alice\", 99.5, true)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ADD);
    cr_assert_str_eq(cmd->data.add.dbName, "users");
    cr_assert_eq(cmd->data.add.key, 10);
    cr_assert_eq(cmd->data.add.valueCount, 4);
    cr_assert_eq(cmd->data.add.values[0].value.i, 42);
    cr_assert_str_eq(cmd->data.add.values[1].value.s, "Alice");
    cr_assert_float_eq(cmd->data.add.values[2].value.d, 99.5, 0.001);
    cr_assert_eq(cmd->data.add.values[3].value.b, 1);

    free_add_command(cmd);
}

Test(parse_add, negative_int_value) {
    Command* cmd = parse_command("ADD temps 1 (-25)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ADD);
    cr_assert_eq(cmd->data.add.valueCount, 1);
    cr_assert_eq(cmd->data.add.values[0].value.i, -25);

    free_add_command(cmd);
}

Test(parse_add, negative_double_value) {
    Command* cmd = parse_command("ADD temps 1 (-3.14)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ADD);
    cr_assert_eq(cmd->data.add.valueCount, 1);
    cr_assert_float_eq(cmd->data.add.values[0].value.d, -3.14, 0.001);

    free_add_command(cmd);
}

Test(parse_add, string_with_escape_sequences) {
    Command* cmd = parse_command("ADD logs 1 (\"Hello\\nWorld\\t!\")");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ADD);
    cr_assert_eq(cmd->data.add.valueCount, 1);
    cr_assert_str_eq(cmd->data.add.values[0].value.s, "Hello\nWorld\t!");

    free_add_command(cmd);
}

Test(parse_add, string_with_escaped_quote) {
    Command* cmd = parse_command("ADD quotes 1 (\"He said \\\"Hi\\\"\")");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ADD);
    cr_assert_eq(cmd->data.add.valueCount, 1);
    cr_assert_str_eq(cmd->data.add.values[0].value.s, "He said \"Hi\"");

    free_add_command(cmd);
}

Test(parse_add, case_insensitive_command) {
    Command* cmd = parse_command("add mydb 1 (42)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ADD);
    cr_assert_str_eq(cmd->data.add.dbName, "mydb");

    free_add_command(cmd);
}

Test(parse_add, case_insensitive_bool) {
    Command* cmd = parse_command("ADD flags 1 (TRUE, FALSE)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ADD);
    cr_assert_eq(cmd->data.add.valueCount, 2);
    cr_assert_eq(cmd->data.add.values[0].value.b, 1);
    cr_assert_eq(cmd->data.add.values[1].value.b, 0);

    free_add_command(cmd);
}

Test(parse_add, extra_whitespace) {
    Command* cmd = parse_command("  ADD   mydb   5   (  42  ,  \"test\"  )  ");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ADD);
    cr_assert_str_eq(cmd->data.add.dbName, "mydb");
    cr_assert_eq(cmd->data.add.key, 5);
    cr_assert_eq(cmd->data.add.valueCount, 2);

    free_add_command(cmd);
}

Test(parse_add, scientific_notation_double) {
    Command* cmd = parse_command("ADD science 1 (1.5e10)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ADD);
    cr_assert_eq(cmd->data.add.valueCount, 1);
    cr_assert_float_eq(cmd->data.add.values[0].value.d, 1.5e10, 1e5);

    free_add_command(cmd);
}

Test(parse_add, zero_key) {
    Command* cmd = parse_command("ADD mydb 0 (42)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ADD);
    cr_assert_eq(cmd->data.add.key, 0);
    cr_assert_eq(cmd->data.add.autoKey, 0);

    free_add_command(cmd);
}

// ============================================================================
// Invalid ADD command tests - Error cases
// ============================================================================

Test(parse_add, missing_db_name) {
    Command* cmd = parse_command("ADD (42)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_add, missing_key) {
    Command* cmd = parse_command("ADD mydb (42)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_add, missing_values) {
    Command* cmd = parse_command("ADD mydb 1");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_add, empty_values) {
    Command* cmd = parse_command("ADD mydb 1 ()");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_add, invalid_db_name) {
    Command* cmd = parse_command("ADD 123db 1 (42)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_add, invalid_key_not_integer) {
    Command* cmd = parse_command("ADD mydb abc (42)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_add, unmatched_parenthesis) {
    Command* cmd = parse_command("ADD mydb 1 (42, \"test\"");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_add, unterminated_string) {
    Command* cmd = parse_command("ADD mydb 1 (\"unterminated)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_add, invalid_value) {
    Command* cmd = parse_command("ADD mydb 1 (invalid_identifier)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_add, missing_comma_between_values) {
    Command* cmd = parse_command("ADD mydb 1 (42 \"test\")");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_add, invalid_db_name_special_chars) {
    Command* cmd = parse_command("ADD my-db 1 (42)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

// ============================================================================
// Test suite for parse_up function
// ============================================================================

// Helper function to free update command memory
static void free_up_command(Command* cmd) {
    if (cmd == NULL) return;
    if (cmd->op == OP_UP && cmd->data.update.values != NULL) {
        for (int i = 0; i < cmd->data.update.valueCount; i++) {
            // Free strings (size > 0 indicates string)
            if (cmd->data.update.values[i].size > 0 && cmd->data.update.values[i].value.s != NULL) {
                free((void*)cmd->data.update.values[i].value.s);
            }
        }
        free(cmd->data.update.values);
    }
    if (cmd->op == OP_UP && cmd->data.update.ignoreFlags != NULL) {
        free(cmd->data.update.ignoreFlags);
    }
    free(cmd);
}

// ============================================================================
// Valid UP command tests
// ============================================================================

Test(parse_up, single_int_value) {
    Command* cmd = parse_command("UP mydb 1 (42)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_UP);
    cr_assert_str_eq(cmd->data.update.dbName, "mydb");
    cr_assert_eq(cmd->data.update.key, 1);
    cr_assert_eq(cmd->data.update.valueCount, 1);
    cr_assert_eq(cmd->data.update.values[0].value.i, 42);
    cr_assert_eq(cmd->data.update.ignoreFlags[0], 0);

    free_up_command(cmd);
}

Test(parse_up, single_string_value) {
    Command* cmd = parse_command("UP users 5 (\"John Doe\")");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_UP);
    cr_assert_str_eq(cmd->data.update.dbName, "users");
    cr_assert_eq(cmd->data.update.key, 5);
    cr_assert_eq(cmd->data.update.valueCount, 1);
    cr_assert_str_eq(cmd->data.update.values[0].value.s, "John Doe");
    cr_assert_eq(cmd->data.update.ignoreFlags[0], 0);

    free_up_command(cmd);
}

Test(parse_up, single_double_value) {
    Command* cmd = parse_command("UP prices 1 (3.14)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_UP);
    cr_assert_str_eq(cmd->data.update.dbName, "prices");
    cr_assert_eq(cmd->data.update.valueCount, 1);
    cr_assert_float_eq(cmd->data.update.values[0].value.d, 3.14, 0.001);
    cr_assert_eq(cmd->data.update.ignoreFlags[0], 0);

    free_up_command(cmd);
}

Test(parse_up, single_bool_true) {
    Command* cmd = parse_command("UP flags 1 (true)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_UP);
    cr_assert_str_eq(cmd->data.update.dbName, "flags");
    cr_assert_eq(cmd->data.update.valueCount, 1);
    cr_assert_eq(cmd->data.update.values[0].value.b, 1);
    cr_assert_eq(cmd->data.update.ignoreFlags[0], 0);

    free_up_command(cmd);
}

Test(parse_up, single_bool_false) {
    Command* cmd = parse_command("UP flags 1 (false)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_UP);
    cr_assert_str_eq(cmd->data.update.dbName, "flags");
    cr_assert_eq(cmd->data.update.valueCount, 1);
    cr_assert_eq(cmd->data.update.values[0].value.b, 0);
    cr_assert_eq(cmd->data.update.ignoreFlags[0], 0);

    free_up_command(cmd);
}

Test(parse_up, single_ignore_marker) {
    Command* cmd = parse_command("UP mydb 1 (_)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_UP);
    cr_assert_str_eq(cmd->data.update.dbName, "mydb");
    cr_assert_eq(cmd->data.update.key, 1);
    cr_assert_eq(cmd->data.update.valueCount, 1);
    cr_assert_eq(cmd->data.update.ignoreFlags[0], 1);

    free_up_command(cmd);
}

Test(parse_up, multiple_values_mixed_types) {
    Command* cmd = parse_command("UP users 10 (42, \"Alice\", 99.5, true)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_UP);
    cr_assert_str_eq(cmd->data.update.dbName, "users");
    cr_assert_eq(cmd->data.update.key, 10);
    cr_assert_eq(cmd->data.update.valueCount, 4);
    cr_assert_eq(cmd->data.update.values[0].value.i, 42);
    cr_assert_str_eq(cmd->data.update.values[1].value.s, "Alice");
    cr_assert_float_eq(cmd->data.update.values[2].value.d, 99.5, 0.001);
    cr_assert_eq(cmd->data.update.values[3].value.b, 1);
    cr_assert_eq(cmd->data.update.ignoreFlags[0], 0);
    cr_assert_eq(cmd->data.update.ignoreFlags[1], 0);
    cr_assert_eq(cmd->data.update.ignoreFlags[2], 0);
    cr_assert_eq(cmd->data.update.ignoreFlags[3], 0);

    free_up_command(cmd);
}

Test(parse_up, multiple_values_with_ignore) {
    Command* cmd = parse_command("UP users 10 (_, \"NewName\", _, false)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_UP);
    cr_assert_str_eq(cmd->data.update.dbName, "users");
    cr_assert_eq(cmd->data.update.key, 10);
    cr_assert_eq(cmd->data.update.valueCount, 4);
    cr_assert_eq(cmd->data.update.ignoreFlags[0], 1);
    cr_assert_eq(cmd->data.update.ignoreFlags[1], 0);
    cr_assert_str_eq(cmd->data.update.values[1].value.s, "NewName");
    cr_assert_eq(cmd->data.update.ignoreFlags[2], 1);
    cr_assert_eq(cmd->data.update.ignoreFlags[3], 0);
    cr_assert_eq(cmd->data.update.values[3].value.b, 0);

    free_up_command(cmd);
}

Test(parse_up, all_ignored) {
    Command* cmd = parse_command("UP mydb 5 (_, _, _)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_UP);
    cr_assert_eq(cmd->data.update.valueCount, 3);
    cr_assert_eq(cmd->data.update.ignoreFlags[0], 1);
    cr_assert_eq(cmd->data.update.ignoreFlags[1], 1);
    cr_assert_eq(cmd->data.update.ignoreFlags[2], 1);

    free_up_command(cmd);
}

Test(parse_up, negative_int_value) {
    Command* cmd = parse_command("UP temps 1 (-25)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_UP);
    cr_assert_eq(cmd->data.update.valueCount, 1);
    cr_assert_eq(cmd->data.update.values[0].value.i, -25);

    free_up_command(cmd);
}

Test(parse_up, negative_double_value) {
    Command* cmd = parse_command("UP temps 1 (-3.14)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_UP);
    cr_assert_eq(cmd->data.update.valueCount, 1);
    cr_assert_float_eq(cmd->data.update.values[0].value.d, -3.14, 0.001);

    free_up_command(cmd);
}

Test(parse_up, negative_key) {
    Command* cmd = parse_command("UP mydb -5 (42)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_UP);
    cr_assert_eq(cmd->data.update.key, -5);
    cr_assert_eq(cmd->data.update.valueCount, 1);

    free_up_command(cmd);
}

Test(parse_up, zero_key) {
    Command* cmd = parse_command("UP mydb 0 (42)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_UP);
    cr_assert_eq(cmd->data.update.key, 0);

    free_up_command(cmd);
}

Test(parse_up, string_with_escape_sequences) {
    Command* cmd = parse_command("UP logs 1 (\"Hello\\nWorld\\t!\")");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_UP);
    cr_assert_eq(cmd->data.update.valueCount, 1);
    cr_assert_str_eq(cmd->data.update.values[0].value.s, "Hello\nWorld\t!");

    free_up_command(cmd);
}

Test(parse_up, case_insensitive_command) {
    Command* cmd = parse_command("up mydb 1 (42)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_UP);
    cr_assert_str_eq(cmd->data.update.dbName, "mydb");

    free_up_command(cmd);
}

Test(parse_up, extra_whitespace) {
    Command* cmd = parse_command("   UP   mydb   5   (  42 ,  \"test\"  )  ");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_UP);
    cr_assert_str_eq(cmd->data.update.dbName, "mydb");
    cr_assert_eq(cmd->data.update.key, 5);
    cr_assert_eq(cmd->data.update.valueCount, 2);
    cr_assert_eq(cmd->data.update.values[0].value.i, 42);
    cr_assert_str_eq(cmd->data.update.values[1].value.s, "test");

    free_up_command(cmd);
}

Test(parse_up, underscore_db_name) {
    Command* cmd = parse_command("UP _private 1 (42)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_UP);
    cr_assert_str_eq(cmd->data.update.dbName, "_private");

    free_up_command(cmd);
}

// ============================================================================
// Invalid UP command tests - Error cases
// ============================================================================

Test(parse_up, missing_db_name) {
    Command* cmd = parse_command("UP (42)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_up, missing_key) {
    Command* cmd = parse_command("UP mydb (42)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_up, missing_values) {
    Command* cmd = parse_command("UP mydb 1");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_up, empty_values) {
    Command* cmd = parse_command("UP mydb 1 ()");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_up, invalid_db_name) {
    Command* cmd = parse_command("UP 123db 1 (42)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_up, invalid_key_not_integer) {
    Command* cmd = parse_command("UP mydb abc (42)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_up, auto_key_not_allowed) {
    Command* cmd = parse_command("UP mydb * (42)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_up, unmatched_parenthesis) {
    Command* cmd = parse_command("UP mydb 1 (42, \"test\"");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_up, unterminated_string) {
    Command* cmd = parse_command("UP mydb 1 (\"unterminated)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_up, invalid_value) {
    Command* cmd = parse_command("UP mydb 1 (invalid_identifier)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_up, missing_comma_between_values) {
    Command* cmd = parse_command("UP mydb 1 (42 \"test\")");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_up, invalid_db_name_special_chars) {
    Command* cmd = parse_command("UP my-db 1 (42)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

// ============================================================================
// Test suite for parse_get function
// ============================================================================

// Helper function to free get command memory
static void free_get_command(Command* cmd) {
    if (cmd == NULL) return;
    if (cmd->op == OP_GET && cmd->data.get.fields != NULL) {
        for (int i = 0; i < cmd->data.get.fieldCount; i++) {
            free(cmd->data.get.fields[i]);
        }
        free(cmd->data.get.fields);
    }
    free(cmd);
}

// ============================================================================
// Valid GET command tests
// ============================================================================

Test(parse_get, all_fields_no_parentheses) {
    Command* cmd = parse_command("GET mydb 1");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_GET);
    cr_assert_str_eq(cmd->data.get.dbName, "mydb");
    cr_assert_eq(cmd->data.get.key, 1);
    cr_assert_eq(cmd->data.get.fieldCount, 0);
    cr_assert_null(cmd->data.get.fields);

    free_get_command(cmd);
}

Test(parse_get, all_fields_empty_parentheses) {
    Command* cmd = parse_command("GET mydb 5 ()");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_GET);
    cr_assert_str_eq(cmd->data.get.dbName, "mydb");
    cr_assert_eq(cmd->data.get.key, 5);
    cr_assert_eq(cmd->data.get.fieldCount, 0);
    cr_assert_null(cmd->data.get.fields);

    free_get_command(cmd);
}

Test(parse_get, single_field) {
    Command* cmd = parse_command("GET users 10 (name)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_GET);
    cr_assert_str_eq(cmd->data.get.dbName, "users");
    cr_assert_eq(cmd->data.get.key, 10);
    cr_assert_eq(cmd->data.get.fieldCount, 1);
    cr_assert_str_eq(cmd->data.get.fields[0], "name");

    free_get_command(cmd);
}

Test(parse_get, multiple_fields) {
    Command* cmd = parse_command("GET users 10 (id, name, email)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_GET);
    cr_assert_str_eq(cmd->data.get.dbName, "users");
    cr_assert_eq(cmd->data.get.key, 10);
    cr_assert_eq(cmd->data.get.fieldCount, 3);
    cr_assert_str_eq(cmd->data.get.fields[0], "id");
    cr_assert_str_eq(cmd->data.get.fields[1], "name");
    cr_assert_str_eq(cmd->data.get.fields[2], "email");

    free_get_command(cmd);
}

Test(parse_get, zero_key) {
    Command* cmd = parse_command("GET mydb 0 (field1)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_GET);
    cr_assert_eq(cmd->data.get.key, 0);
    cr_assert_eq(cmd->data.get.fieldCount, 1);

    free_get_command(cmd);
}

Test(parse_get, negative_key) {
    Command* cmd = parse_command("GET mydb -5 (field1)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_GET);
    cr_assert_eq(cmd->data.get.key, -5);
    cr_assert_eq(cmd->data.get.fieldCount, 1);

    free_get_command(cmd);
}

Test(parse_get, field_with_underscores) {
    Command* cmd = parse_command("GET mydb 1 (first_name, last_name, user_id)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_GET);
    cr_assert_eq(cmd->data.get.fieldCount, 3);
    cr_assert_str_eq(cmd->data.get.fields[0], "first_name");
    cr_assert_str_eq(cmd->data.get.fields[1], "last_name");
    cr_assert_str_eq(cmd->data.get.fields[2], "user_id");

    free_get_command(cmd);
}

Test(parse_get, field_starts_with_underscore) {
    Command* cmd = parse_command("GET mydb 1 (_private, _internal)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_GET);
    cr_assert_eq(cmd->data.get.fieldCount, 2);
    cr_assert_str_eq(cmd->data.get.fields[0], "_private");
    cr_assert_str_eq(cmd->data.get.fields[1], "_internal");

    free_get_command(cmd);
}

Test(parse_get, db_name_with_underscore) {
    Command* cmd = parse_command("GET my_database 1 (field1)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_GET);
    cr_assert_str_eq(cmd->data.get.dbName, "my_database");

    free_get_command(cmd);
}

Test(parse_get, db_name_starts_with_underscore) {
    Command* cmd = parse_command("GET _private 1 (field1)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_GET);
    cr_assert_str_eq(cmd->data.get.dbName, "_private");

    free_get_command(cmd);
}

Test(parse_get, case_insensitive_command) {
    Command* cmd = parse_command("get mydb 1 (name)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_GET);
    cr_assert_str_eq(cmd->data.get.dbName, "mydb");

    free_get_command(cmd);
}

Test(parse_get, extra_whitespace) {
    Command* cmd = parse_command("  GET   mydb   10   (  field1  ,  field2  )  ");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_GET);
    cr_assert_str_eq(cmd->data.get.dbName, "mydb");
    cr_assert_eq(cmd->data.get.key, 10);
    cr_assert_eq(cmd->data.get.fieldCount, 2);
    cr_assert_str_eq(cmd->data.get.fields[0], "field1");
    cr_assert_str_eq(cmd->data.get.fields[1], "field2");

    free_get_command(cmd);
}

Test(parse_get, large_key) {
    Command* cmd = parse_command("GET mydb 999999 (field1)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_GET);
    cr_assert_eq(cmd->data.get.key, 999999);

    free_get_command(cmd);
}

Test(parse_get, many_fields) {
    Command* cmd = parse_command("GET mydb 1 (f1, f2, f3, f4, f5, f6, f7, f8)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_GET);
    cr_assert_eq(cmd->data.get.fieldCount, 8);
    cr_assert_str_eq(cmd->data.get.fields[0], "f1");
    cr_assert_str_eq(cmd->data.get.fields[7], "f8");

    free_get_command(cmd);
}

// ============================================================================
// Invalid GET command tests - Error cases
// ============================================================================

Test(parse_get, missing_db_name) {
    Command* cmd = parse_command("GET");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_get, missing_key) {
    Command* cmd = parse_command("GET mydb");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_get, invalid_db_name_starts_with_number) {
    Command* cmd = parse_command("GET 123db 1 (field1)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_get, invalid_db_name_special_chars) {
    Command* cmd = parse_command("GET my-db 1 (field1)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_get, invalid_key_not_integer) {
    Command* cmd = parse_command("GET mydb abc (field1)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_get, invalid_key_with_auto_marker) {
    Command* cmd = parse_command("GET mydb * (field1)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_get, invalid_field_name_starts_with_number) {
    Command* cmd = parse_command("GET mydb 1 (123field)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_get, invalid_field_name_special_chars) {
    Command* cmd = parse_command("GET mydb 1 (field-name)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_get, unmatched_opening_paren) {
    Command* cmd = parse_command("GET mydb 1 (field1");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_get, unmatched_closing_paren) {
    Command* cmd = parse_command("GET mydb 1 field1)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_get, empty_field_name) {
    Command* cmd = parse_command("GET mydb 1 (field1, , field2)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_get, trailing_comma) {
    Command* cmd = parse_command("GET mydb 1 (field1, field2,)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

// ============================================================================
// Test suite for parse_del function
// ============================================================================

// ============================================================================
// Valid DEL command tests
// ============================================================================

Test(parse_del, basic_delete) {
    Command* cmd = parse_command("DEL mydb 1");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_DEL);
    cr_assert_str_eq(cmd->data.delete.dbName, "mydb");
    cr_assert_eq(cmd->data.delete.key, 1);

    free_command(cmd, 0);
}

Test(parse_del, zero_key) {
    Command* cmd = parse_command("DEL mydb 0");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_DEL);
    cr_assert_str_eq(cmd->data.delete.dbName, "mydb");
    cr_assert_eq(cmd->data.delete.key, 0);

    free_command(cmd, 0);
}

Test(parse_del, negative_key) {
    Command* cmd = parse_command("DEL mydb -5");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_DEL);
    cr_assert_str_eq(cmd->data.delete.dbName, "mydb");
    cr_assert_eq(cmd->data.delete.key, -5);

    free_command(cmd, 0);
}

Test(parse_del, large_key) {
    Command* cmd = parse_command("DEL mydb 999999");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_DEL);
    cr_assert_eq(cmd->data.delete.key, 999999);

    free_command(cmd, 0);
}

Test(parse_del, db_name_with_underscore) {
    Command* cmd = parse_command("DEL my_database 1");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_DEL);
    cr_assert_str_eq(cmd->data.delete.dbName, "my_database");

    free_command(cmd, 0);
}

Test(parse_del, db_name_starts_with_underscore) {
    Command* cmd = parse_command("DEL _private 1");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_DEL);
    cr_assert_str_eq(cmd->data.delete.dbName, "_private");

    free_command(cmd, 0);
}

Test(parse_del, case_insensitive_command) {
    Command* cmd = parse_command("del mydb 1");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_DEL);
    cr_assert_str_eq(cmd->data.delete.dbName, "mydb");

    free_command(cmd, 0);
}

Test(parse_del, extra_whitespace) {
    Command* cmd = parse_command("  DEL   mydb   10  ");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_DEL);
    cr_assert_str_eq(cmd->data.delete.dbName, "mydb");
    cr_assert_eq(cmd->data.delete.key, 10);

    free_command(cmd, 0);
}

// ============================================================================
// Invalid DEL command tests - Error cases
// ============================================================================

Test(parse_del, missing_db_name) {
    Command* cmd = parse_command("DEL");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_del, missing_key) {
    Command* cmd = parse_command("DEL mydb");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_del, invalid_db_name_starts_with_number) {
    Command* cmd = parse_command("DEL 123db 1");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_del, invalid_db_name_special_chars) {
    Command* cmd = parse_command("DEL my-db 1");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_del, invalid_key_not_integer) {
    Command* cmd = parse_command("DEL mydb abc");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_del, invalid_key_with_auto_marker) {
    Command* cmd = parse_command("DEL mydb *");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_del, extra_arguments) {
    Command* cmd = parse_command("DEL mydb 1 extra");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_del, unexpected_parenthesis) {
    Command* cmd = parse_command("DEL mydb 1 ()");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

// ============================================================================
// Test suite for parse_get_all function
// ============================================================================

// Helper function to free get_all command memory
static void free_get_all_command(Command* cmd) {
    if (cmd == NULL) return;
    if (cmd->op == OP_GET_ALL && cmd->data.get_all.fields != NULL) {
        for (int i = 0; i < cmd->data.get_all.fieldCount; i++) {
            free(cmd->data.get_all.fields[i]);
        }
        free(cmd->data.get_all.fields);
    }
    free(cmd);
}

// ============================================================================
// Valid GET_ALL command tests
// ============================================================================

Test(parse_get_all, all_fields_no_parentheses) {
    Command* cmd = parse_command("GET_ALL mydb");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_GET_ALL);
    cr_assert_str_eq(cmd->data.get_all.dbName, "mydb");
    cr_assert_eq(cmd->data.get_all.fieldCount, 0);
    cr_assert_null(cmd->data.get_all.fields);

    free_get_all_command(cmd);
}

Test(parse_get_all, all_fields_empty_parentheses) {
    Command* cmd = parse_command("GET_ALL mydb ()");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_GET_ALL);
    cr_assert_str_eq(cmd->data.get_all.dbName, "mydb");
    cr_assert_eq(cmd->data.get_all.fieldCount, 0);
    cr_assert_null(cmd->data.get_all.fields);

    free_get_all_command(cmd);
}

Test(parse_get_all, single_field) {
    Command* cmd = parse_command("GET_ALL users (name)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_GET_ALL);
    cr_assert_str_eq(cmd->data.get_all.dbName, "users");
    cr_assert_eq(cmd->data.get_all.fieldCount, 1);
    cr_assert_str_eq(cmd->data.get_all.fields[0], "name");

    free_get_all_command(cmd);
}

Test(parse_get_all, multiple_fields) {
    Command* cmd = parse_command("GET_ALL users (id, name, email)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_GET_ALL);
    cr_assert_str_eq(cmd->data.get_all.dbName, "users");
    cr_assert_eq(cmd->data.get_all.fieldCount, 3);
    cr_assert_str_eq(cmd->data.get_all.fields[0], "id");
    cr_assert_str_eq(cmd->data.get_all.fields[1], "name");
    cr_assert_str_eq(cmd->data.get_all.fields[2], "email");

    free_get_all_command(cmd);
}

Test(parse_get_all, field_with_underscores) {
    Command* cmd = parse_command("GET_ALL mydb (first_name, last_name, user_id)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_GET_ALL);
    cr_assert_eq(cmd->data.get_all.fieldCount, 3);
    cr_assert_str_eq(cmd->data.get_all.fields[0], "first_name");
    cr_assert_str_eq(cmd->data.get_all.fields[1], "last_name");
    cr_assert_str_eq(cmd->data.get_all.fields[2], "user_id");

    free_get_all_command(cmd);
}

Test(parse_get_all, field_starts_with_underscore) {
    Command* cmd = parse_command("GET_ALL mydb (_private, _internal)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_GET_ALL);
    cr_assert_eq(cmd->data.get_all.fieldCount, 2);
    cr_assert_str_eq(cmd->data.get_all.fields[0], "_private");
    cr_assert_str_eq(cmd->data.get_all.fields[1], "_internal");

    free_get_all_command(cmd);
}

Test(parse_get_all, db_name_with_underscore) {
    Command* cmd = parse_command("GET_ALL my_database (field1)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_GET_ALL);
    cr_assert_str_eq(cmd->data.get_all.dbName, "my_database");

    free_get_all_command(cmd);
}

Test(parse_get_all, db_name_starts_with_underscore) {
    Command* cmd = parse_command("GET_ALL _private (field1)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_GET_ALL);
    cr_assert_str_eq(cmd->data.get_all.dbName, "_private");

    free_get_all_command(cmd);
}

Test(parse_get_all, case_insensitive_command) {
    Command* cmd = parse_command("get_all mydb (name)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_GET_ALL);
    cr_assert_str_eq(cmd->data.get_all.dbName, "mydb");

    free_get_all_command(cmd);
}

Test(parse_get_all, extra_whitespace) {
    Command* cmd = parse_command("  GET_ALL   mydb   (  field1  ,  field2  )  ");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_GET_ALL);
    cr_assert_str_eq(cmd->data.get_all.dbName, "mydb");
    cr_assert_eq(cmd->data.get_all.fieldCount, 2);
    cr_assert_str_eq(cmd->data.get_all.fields[0], "field1");
    cr_assert_str_eq(cmd->data.get_all.fields[1], "field2");

    free_get_all_command(cmd);
}

Test(parse_get_all, many_fields) {
    Command* cmd = parse_command("GET_ALL mydb (f1, f2, f3, f4, f5, f6, f7, f8)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_GET_ALL);
    cr_assert_eq(cmd->data.get_all.fieldCount, 8);
    cr_assert_str_eq(cmd->data.get_all.fields[0], "f1");
    cr_assert_str_eq(cmd->data.get_all.fields[7], "f8");

    free_get_all_command(cmd);
}

// ============================================================================
// Invalid GET_ALL command tests - Error cases
// ============================================================================

Test(parse_get_all, missing_db_name) {
    Command* cmd = parse_command("GET_ALL");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_get_all, invalid_db_name_starts_with_number) {
    Command* cmd = parse_command("GET_ALL 123db (field1)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_get_all, invalid_db_name_special_chars) {
    Command* cmd = parse_command("GET_ALL my-db (field1)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_get_all, invalid_field_name_starts_with_number) {
    Command* cmd = parse_command("GET_ALL mydb (123field)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_get_all, invalid_field_name_special_chars) {
    Command* cmd = parse_command("GET_ALL mydb (field-name)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_get_all, unmatched_opening_paren) {
    Command* cmd = parse_command("GET_ALL mydb (field1");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_get_all, unmatched_closing_paren) {
    Command* cmd = parse_command("GET_ALL mydb field1)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_get_all, empty_field_name) {
    Command* cmd = parse_command("GET_ALL mydb (field1, , field2)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_get_all, trailing_comma) {
    Command* cmd = parse_command("GET_ALL mydb (field1, field2,)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_get_all, extra_arguments) {
    Command* cmd = parse_command("GET_ALL mydb extra");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

// ============================================================================
// Test suite for parse_search function
// ============================================================================

// Helper function to free search command memory
static void free_search_command(Command* cmd) {
    if (cmd == NULL) return;
    if (cmd->op == OP_SEARCH) {
        if (cmd->data.search.returnFields != NULL) {
            for (int i = 0; i < cmd->data.search.fieldCount; i++) {
                free(cmd->data.search.returnFields[i]);
            }
            free(cmd->data.search.returnFields);
        }
        // Free string value if present
        if (cmd->data.search.value.size > 0 && cmd->data.search.value.value.s != NULL) {
            free((void*)cmd->data.search.value.value.s);
        }
    }
    free(cmd);
}

// ============================================================================
// Valid SEARCH command tests
// ============================================================================

Test(parse_search, all_fields_int_value) {
    Command* cmd = parse_command("SEARCH mydb age 25");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_SEARCH);
    cr_assert_str_eq(cmd->data.search.dbName, "mydb");
    cr_assert_str_eq(cmd->data.search.fieldName, "age");
    cr_assert_eq(cmd->data.search.value.value.i, 25);
    cr_assert_eq(cmd->data.search.fieldCount, 0);
    cr_assert_null(cmd->data.search.returnFields);

    free_search_command(cmd);
}

Test(parse_search, all_fields_string_value) {
    Command* cmd = parse_command("SEARCH users name \"John\"");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_SEARCH);
    cr_assert_str_eq(cmd->data.search.dbName, "users");
    cr_assert_str_eq(cmd->data.search.fieldName, "name");
    cr_assert_str_eq(cmd->data.search.value.value.s, "John");
    cr_assert_eq(cmd->data.search.fieldCount, 0);

    free_search_command(cmd);
}

Test(parse_search, all_fields_double_value) {
    Command* cmd = parse_command("SEARCH products price 99.99");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_SEARCH);
    cr_assert_str_eq(cmd->data.search.dbName, "products");
    cr_assert_str_eq(cmd->data.search.fieldName, "price");
    cr_assert_float_eq(cmd->data.search.value.value.d, 99.99, 0.001);

    free_search_command(cmd);
}

Test(parse_search, all_fields_bool_true) {
    Command* cmd = parse_command("SEARCH users active true");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_SEARCH);
    cr_assert_str_eq(cmd->data.search.dbName, "users");
    cr_assert_str_eq(cmd->data.search.fieldName, "active");
    cr_assert_eq(cmd->data.search.value.value.b, 1);

    free_search_command(cmd);
}

Test(parse_search, all_fields_bool_false) {
    Command* cmd = parse_command("SEARCH users verified false");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_SEARCH);
    cr_assert_str_eq(cmd->data.search.fieldName, "verified");
    cr_assert_eq(cmd->data.search.value.value.b, 0);

    free_search_command(cmd);
}

Test(parse_search, all_fields_empty_parens) {
    Command* cmd = parse_command("SEARCH mydb age 25 ()");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_SEARCH);
    cr_assert_str_eq(cmd->data.search.dbName, "mydb");
    cr_assert_str_eq(cmd->data.search.fieldName, "age");
    cr_assert_eq(cmd->data.search.value.value.i, 25);
    cr_assert_eq(cmd->data.search.fieldCount, 0);

    free_search_command(cmd);
}

Test(parse_search, single_return_field) {
    Command* cmd = parse_command("SEARCH users age 25 (name)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_SEARCH);
    cr_assert_str_eq(cmd->data.search.dbName, "users");
    cr_assert_str_eq(cmd->data.search.fieldName, "age");
    cr_assert_eq(cmd->data.search.value.value.i, 25);
    cr_assert_eq(cmd->data.search.fieldCount, 1);
    cr_assert_str_eq(cmd->data.search.returnFields[0], "name");

    free_search_command(cmd);
}

Test(parse_search, multiple_return_fields) {
    Command* cmd = parse_command("SEARCH users age 25 (name, email, phone)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_SEARCH);
    cr_assert_str_eq(cmd->data.search.dbName, "users");
    cr_assert_eq(cmd->data.search.fieldCount, 3);
    cr_assert_str_eq(cmd->data.search.returnFields[0], "name");
    cr_assert_str_eq(cmd->data.search.returnFields[1], "email");
    cr_assert_str_eq(cmd->data.search.returnFields[2], "phone");

    free_search_command(cmd);
}

Test(parse_search, string_with_spaces) {
    Command* cmd = parse_command("SEARCH users name \"John Doe\" (email)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_SEARCH);
    cr_assert_str_eq(cmd->data.search.fieldName, "name");
    cr_assert_str_eq(cmd->data.search.value.value.s, "John Doe");
    cr_assert_eq(cmd->data.search.fieldCount, 1);

    free_search_command(cmd);
}

Test(parse_search, negative_int_value) {
    Command* cmd = parse_command("SEARCH temps temperature -10");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_SEARCH);
    cr_assert_eq(cmd->data.search.value.value.i, -10);

    free_search_command(cmd);
}

Test(parse_search, negative_double_value) {
    Command* cmd = parse_command("SEARCH coords latitude -45.5");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_SEARCH);
    cr_assert_float_eq(cmd->data.search.value.value.d, -45.5, 0.001);

    free_search_command(cmd);
}

Test(parse_search, field_with_underscores) {
    Command* cmd = parse_command("SEARCH users first_name \"Alice\" (user_id, last_name)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_SEARCH);
    cr_assert_str_eq(cmd->data.search.fieldName, "first_name");
    cr_assert_eq(cmd->data.search.fieldCount, 2);
    cr_assert_str_eq(cmd->data.search.returnFields[0], "user_id");
    cr_assert_str_eq(cmd->data.search.returnFields[1], "last_name");

    free_search_command(cmd);
}

Test(parse_search, case_insensitive_command) {
    Command* cmd = parse_command("search mydb age 25");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_SEARCH);
    cr_assert_str_eq(cmd->data.search.dbName, "mydb");

    free_search_command(cmd);
}

Test(parse_search, case_insensitive_bool) {
    Command* cmd = parse_command("SEARCH users active TRUE");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_SEARCH);
    cr_assert_eq(cmd->data.search.value.value.b, 1);

    free_search_command(cmd);
}

Test(parse_search, extra_whitespace) {
    Command* cmd = parse_command("  SEARCH   users   age   25   (  name  ,  email  )  ");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_SEARCH);
    cr_assert_str_eq(cmd->data.search.dbName, "users");
    cr_assert_str_eq(cmd->data.search.fieldName, "age");
    cr_assert_eq(cmd->data.search.value.value.i, 25);
    cr_assert_eq(cmd->data.search.fieldCount, 2);

    free_search_command(cmd);
}

Test(parse_search, db_with_underscore) {
    Command* cmd = parse_command("SEARCH my_database age 25");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_SEARCH);
    cr_assert_str_eq(cmd->data.search.dbName, "my_database");

    free_search_command(cmd);
}

// ============================================================================
// Invalid SEARCH command tests - Error cases
// ============================================================================

Test(parse_search, missing_db_name) {
    Command* cmd = parse_command("SEARCH");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_search, missing_field_name) {
    Command* cmd = parse_command("SEARCH mydb");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_search, missing_value) {
    Command* cmd = parse_command("SEARCH mydb age");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_search, invalid_db_name) {
    Command* cmd = parse_command("SEARCH 123db age 25");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_search, invalid_field_name) {
    Command* cmd = parse_command("SEARCH mydb 123age 25");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_search, invalid_db_name_special_chars) {
    Command* cmd = parse_command("SEARCH my-db age 25");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_search, invalid_field_name_special_chars) {
    Command* cmd = parse_command("SEARCH mydb user-age 25");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_search, unterminated_string) {
    Command* cmd = parse_command("SEARCH users name \"John");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_search, invalid_return_field) {
    Command* cmd = parse_command("SEARCH users age 25 (123name)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_search, unmatched_opening_paren) {
    Command* cmd = parse_command("SEARCH users age 25 (name");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_search, unmatched_closing_paren) {
    Command* cmd = parse_command("SEARCH users age 25 name)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_search, empty_return_field_name) {
    Command* cmd = parse_command("SEARCH users age 25 (name, , email)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_search, trailing_comma) {
    Command* cmd = parse_command("SEARCH users age 25 (name, email,)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

// ============================================================================
// Test suite for parse_count function
// ============================================================================

// ============================================================================
// Valid COUNT command tests
// ============================================================================

Test(parse_count, basic_count) {
    Command* cmd = parse_command("COUNT mydb");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_COUNT);
    cr_assert_str_eq(cmd->data.count.dbName, "mydb");

    free_command(cmd, 0);
}

Test(parse_count, db_name_with_underscore) {
    Command* cmd = parse_command("COUNT my_database");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_COUNT);
    cr_assert_str_eq(cmd->data.count.dbName, "my_database");

    free_command(cmd, 0);
}

Test(parse_count, db_name_starts_with_underscore) {
    Command* cmd = parse_command("COUNT _private");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_COUNT);
    cr_assert_str_eq(cmd->data.count.dbName, "_private");

    free_command(cmd, 0);
}

Test(parse_count, case_insensitive_command) {
    Command* cmd = parse_command("count mydb");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_COUNT);
    cr_assert_str_eq(cmd->data.count.dbName, "mydb");

    free_command(cmd, 0);
}

Test(parse_count, extra_whitespace) {
    Command* cmd = parse_command("  COUNT   mydb  ");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_COUNT);
    cr_assert_str_eq(cmd->data.count.dbName, "mydb");

    free_command(cmd, 0);
}

Test(parse_count, long_db_name) {
    Command* cmd = parse_command("COUNT my_very_long_database_name");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_COUNT);
    cr_assert_str_eq(cmd->data.count.dbName, "my_very_long_database_name");

    free_command(cmd, 0);
}

// ============================================================================
// Invalid COUNT command tests - Error cases
// ============================================================================

Test(parse_count, missing_db_name) {
    Command* cmd = parse_command("COUNT");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_count, invalid_db_name_starts_with_number) {
    Command* cmd = parse_command("COUNT 123db");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_count, invalid_db_name_special_chars) {
    Command* cmd = parse_command("COUNT my-db");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_count, extra_arguments) {
    Command* cmd = parse_command("COUNT mydb extra");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_count, unexpected_parenthesis) {
    Command* cmd = parse_command("COUNT mydb ()");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

// ============================================================================
// Valid CREATE_INDEX command tests
// ============================================================================

Test(parse_create_index, basic) {
    Command* cmd = parse_command("CREATE_INDEX mydb myfield");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_CREATE_INDEX);
    cr_assert_str_eq(cmd->data.create_index.dbName, "mydb");
    cr_assert_str_eq(cmd->data.create_index.fieldName, "myfield");

    free_command(cmd, 0);
}

Test(parse_create_index, with_underscores) {
    Command* cmd = parse_command("CREATE_INDEX my_database my_field_name");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_CREATE_INDEX);
    cr_assert_str_eq(cmd->data.create_index.dbName, "my_database");
    cr_assert_str_eq(cmd->data.create_index.fieldName, "my_field_name");

    free_command(cmd, 0);
}

Test(parse_create_index, case_insensitive) {
    Command* cmd = parse_command("create_index MyDb MyField");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_CREATE_INDEX);
    cr_assert_str_eq(cmd->data.create_index.dbName, "MyDb");
    cr_assert_str_eq(cmd->data.create_index.fieldName, "MyField");

    free_command(cmd, 0);
}

Test(parse_create_index, extra_whitespace) {
    Command* cmd = parse_command("  CREATE_INDEX   mydb   myfield  ");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_CREATE_INDEX);
    cr_assert_str_eq(cmd->data.create_index.dbName, "mydb");
    cr_assert_str_eq(cmd->data.create_index.fieldName, "myfield");

    free_command(cmd, 0);
}

Test(parse_create_index, long_names) {
    Command* cmd = parse_command("CREATE_INDEX very_long_database_name very_long_field_name_here");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_CREATE_INDEX);
    cr_assert_str_eq(cmd->data.create_index.dbName, "very_long_database_name");
    cr_assert_str_eq(cmd->data.create_index.fieldName, "very_long_field_name_here");

    free_command(cmd, 0);
}

// ============================================================================
// Invalid CREATE_INDEX command tests - Error cases
// ============================================================================

Test(parse_create_index, missing_db_name) {
    Command* cmd = parse_command("CREATE_INDEX");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_create_index, missing_field_name) {
    Command* cmd = parse_command("CREATE_INDEX mydb");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_create_index, invalid_db_name_starts_with_number) {
    Command* cmd = parse_command("CREATE_INDEX 123invalid myfield");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_create_index, invalid_field_name_special_chars) {
    Command* cmd = parse_command("CREATE_INDEX mydb field-name");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}

Test(parse_create_index, extra_arguments) {
    Command* cmd = parse_command("CREATE_INDEX mydb myfield extra");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd, 0);
}
