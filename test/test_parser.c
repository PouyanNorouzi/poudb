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
