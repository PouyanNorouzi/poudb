#include <criterion/criterion.h>
#include <criterion/new/assert.h>

#include "db/parser.h"

/**
 * Test suite for parse_create function
 */

// Helper function to free command memory
static void free_command(Command* cmd) {
    if(cmd == NULL) return;
    if(cmd->op == OP_CREATE && cmd->data.create.fields != NULL) {
        free(cmd->data.create.fields);
    }
    free(cmd);
}

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

    free_command(cmd);
}

Test(parse_create, single_string_field) {
    Command* cmd = parse_command("CREATE users (string name)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_CREATE);
    cr_assert_str_eq(cmd->data.create.dbName, "users");
    cr_assert_eq(cmd->data.create.fieldCount, 1);
    cr_assert_str_eq(cmd->data.create.fields[0].name, "name");
    cr_assert_eq(cmd->data.create.fields[0].type, TYPE_STRING);

    free_command(cmd);
}

Test(parse_create, single_double_field) {
    Command* cmd = parse_command("CREATE prices (double amount)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_CREATE);
    cr_assert_str_eq(cmd->data.create.dbName, "prices");
    cr_assert_eq(cmd->data.create.fieldCount, 1);
    cr_assert_str_eq(cmd->data.create.fields[0].name, "amount");
    cr_assert_eq(cmd->data.create.fields[0].type, TYPE_DOUBLE);

    free_command(cmd);
}

Test(parse_create, single_bool_field) {
    Command* cmd = parse_command("CREATE flags (bool active)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_CREATE);
    cr_assert_str_eq(cmd->data.create.dbName, "flags");
    cr_assert_eq(cmd->data.create.fieldCount, 1);
    cr_assert_str_eq(cmd->data.create.fields[0].name, "active");
    cr_assert_eq(cmd->data.create.fields[0].type, TYPE_BOOL);

    free_command(cmd);
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

    free_command(cmd);
}

Test(parse_create, case_insensitive_command) {
    Command* cmd = parse_command("create mydb (int id)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_CREATE);
    cr_assert_str_eq(cmd->data.create.dbName, "mydb");

    free_command(cmd);
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

    free_command(cmd);
}

Test(parse_create, extra_whitespace) {
    Command* cmd =
        parse_command("  CREATE   mydb   (  int   id  ,  string   name  )  ");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_CREATE);
    cr_assert_str_eq(cmd->data.create.dbName, "mydb");
    cr_assert_eq(cmd->data.create.fieldCount, 2);

    free_command(cmd);
}

Test(parse_create, underscore_in_names) {
    Command* cmd =
        parse_command("CREATE my_database (int user_id, string first_name)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_CREATE);
    cr_assert_str_eq(cmd->data.create.dbName, "my_database");
    cr_assert_str_eq(cmd->data.create.fields[0].name, "user_id");
    cr_assert_str_eq(cmd->data.create.fields[1].name, "first_name");

    free_command(cmd);
}

Test(parse_create, db_name_starts_with_underscore) {
    Command* cmd = parse_command("CREATE _private (int id)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_CREATE);
    cr_assert_str_eq(cmd->data.create.dbName, "_private");

    free_command(cmd);
}

// ============================================================================
// Invalid CREATE command tests - Error cases
// ============================================================================

Test(parse_create, missing_db_name) {
    Command* cmd = parse_command("CREATE (int id)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd);
}

Test(parse_create, missing_parentheses) {
    Command* cmd = parse_command("CREATE mydb int id");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd);
}

Test(parse_create, empty_field_list) {
    Command* cmd = parse_command("CREATE mydb ()");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd);
}

Test(parse_create, missing_field_name) {
    Command* cmd = parse_command("CREATE mydb (int)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd);
}

Test(parse_create, missing_field_type) {
    Command* cmd = parse_command("CREATE mydb (id)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd);
}

Test(parse_create, invalid_field_type) {
    Command* cmd = parse_command("CREATE mydb (varchar name)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd);
}

Test(parse_create, invalid_db_name_starts_with_number) {
    Command* cmd = parse_command("CREATE 123db (int id)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd);
}

Test(parse_create, invalid_field_name_starts_with_number) {
    Command* cmd = parse_command("CREATE mydb (int 123field)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd);
}

Test(parse_create, unmatched_opening_paren) {
    Command* cmd = parse_command("CREATE mydb (int id");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd);
}

Test(parse_create, invalid_db_name_with_special_chars) {
    Command* cmd = parse_command("CREATE my-db (int id)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd);
}

Test(parse_create, invalid_field_name_with_special_chars) {
    Command* cmd = parse_command("CREATE mydb (int user-id)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd);
}

Test(parse_create, null_input) {
    Command* cmd = parse_command(NULL);

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd);
}

Test(parse_create, empty_input) {
    Command* cmd = parse_command("");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd);
}

Test(parse_create, only_whitespace) {
    Command* cmd = parse_command("   ");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd);
}

Test(parse_create, invalid_command) {
    Command* cmd = parse_command("INVALID mydb (int id)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd);
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

    free_command(cmd);
}

Test(parse_add, missing_key) {
    Command* cmd = parse_command("ADD mydb (42)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd);
}

Test(parse_add, missing_values) {
    Command* cmd = parse_command("ADD mydb 1");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd);
}

Test(parse_add, empty_values) {
    Command* cmd = parse_command("ADD mydb 1 ()");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd);
}

Test(parse_add, invalid_db_name) {
    Command* cmd = parse_command("ADD 123db 1 (42)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd);
}

Test(parse_add, invalid_key_not_integer) {
    Command* cmd = parse_command("ADD mydb abc (42)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd);
}

Test(parse_add, unmatched_parenthesis) {
    Command* cmd = parse_command("ADD mydb 1 (42, \"test\"");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd);
}

Test(parse_add, unterminated_string) {
    Command* cmd = parse_command("ADD mydb 1 (\"unterminated)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd);
}

Test(parse_add, invalid_value) {
    Command* cmd = parse_command("ADD mydb 1 (invalid_identifier)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd);
}

Test(parse_add, missing_comma_between_values) {
    Command* cmd = parse_command("ADD mydb 1 (42 \"test\")");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd);
}

Test(parse_add, invalid_db_name_special_chars) {
    Command* cmd = parse_command("ADD my-db 1 (42)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd);
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

    free_command(cmd);
}

Test(parse_up, missing_key) {
    Command* cmd = parse_command("UP mydb (42)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd);
}

Test(parse_up, missing_values) {
    Command* cmd = parse_command("UP mydb 1");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd);
}

Test(parse_up, empty_values) {
    Command* cmd = parse_command("UP mydb 1 ()");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd);
}

Test(parse_up, invalid_db_name) {
    Command* cmd = parse_command("UP 123db 1 (42)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd);
}

Test(parse_up, invalid_key_not_integer) {
    Command* cmd = parse_command("UP mydb abc (42)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd);
}

Test(parse_up, auto_key_not_allowed) {
    Command* cmd = parse_command("UP mydb * (42)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd);
}

Test(parse_up, unmatched_parenthesis) {
    Command* cmd = parse_command("UP mydb 1 (42, \"test\"");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd);
}

Test(parse_up, unterminated_string) {
    Command* cmd = parse_command("UP mydb 1 (\"unterminated)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd);
}

Test(parse_up, invalid_value) {
    Command* cmd = parse_command("UP mydb 1 (invalid_identifier)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd);
}

Test(parse_up, missing_comma_between_values) {
    Command* cmd = parse_command("UP mydb 1 (42 \"test\")");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd);
}

Test(parse_up, invalid_db_name_special_chars) {
    Command* cmd = parse_command("UP my-db 1 (42)");

    cr_assert_not_null(cmd);
    cr_assert_eq(cmd->op, OP_ERROR);

    free_command(cmd);
}
