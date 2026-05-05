#include <criterion/criterion.h>
#include <string.h>

#include "auth.h"

/* Re-init store before each test */
static AuthStore store;

static void setup(void) {
    auth_store_init(&store);
}

/* ── auth_store_init ─────────────────────────────────────────────────── */

Test(auth, init_empty, .init = setup) {
    cr_assert_eq(store.count, 0);
}

/* ── auth_generate_token ─────────────────────────────────────────────── */

Test(auth, generate_token_length, .init = setup) {
    char token[AUTH_TOKEN_BUF_SIZE];
    cr_assert_eq(auth_generate_token(token), 0);
    cr_assert_eq(strlen(token), AUTH_TOKEN_HEX_LEN);
}

Test(auth, generate_token_hex_chars, .init = setup) {
    char token[AUTH_TOKEN_BUF_SIZE];
    auth_generate_token(token);
    for(size_t i = 0; i < AUTH_TOKEN_HEX_LEN; i++) {
        char c = token[i];
        cr_assert((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'),
                  "Non-hex character '%c' at index %zu", c, i);
    }
}

Test(auth, generate_token_unique, .init = setup) {
    char t1[AUTH_TOKEN_BUF_SIZE], t2[AUTH_TOKEN_BUF_SIZE];
    auth_generate_token(t1);
    auth_generate_token(t2);
    cr_assert_neq(strcmp(t1, t2), 0, "Two generated tokens should differ");
}

/* ── auth_add_key + auth_verify ──────────────────────────────────────── */

Test(auth, add_and_verify_admin, .init = setup) {
    char token[AUTH_TOKEN_BUF_SIZE];
    auth_generate_token(token);

    cr_assert_eq(auth_add_key(&store, "root", token, ROLE_ADMIN), 0);
    cr_assert_eq(store.count, 1);
    cr_assert_eq(auth_verify(&store, token), AUTH_ADMIN);
}

Test(auth, add_and_verify_readonly, .init = setup) {
    char token[AUTH_TOKEN_BUF_SIZE];
    auth_generate_token(token);

    cr_assert_eq(auth_add_key(&store, "reader", token, ROLE_READONLY), 0);
    cr_assert_eq(auth_verify(&store, token), AUTH_READONLY);
}

Test(auth, verify_wrong_token_returns_none, .init = setup) {
    char token[AUTH_TOKEN_BUF_SIZE];
    auth_generate_token(token);
    auth_add_key(&store, "key1", token, ROLE_ADMIN);

    cr_assert_eq(auth_verify(&store, "deadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeef"),
                 AUTH_NONE);
}

Test(auth, verify_empty_store_returns_none, .init = setup) {
    cr_assert_eq(auth_verify(&store, "anytoken"), AUTH_NONE);
}

/* ── auth_del_key ────────────────────────────────────────────────────── */

Test(auth, del_key_removes_key, .init = setup) {
    char token[AUTH_TOKEN_BUF_SIZE];
    auth_generate_token(token);

    auth_add_key(&store, "temp", token, ROLE_ADMIN);
    cr_assert_eq(store.count, 1);

    cr_assert_eq(auth_del_key(&store, "temp"), 0);
    cr_assert_eq(store.count, 0);
    cr_assert_eq(auth_verify(&store, token), AUTH_NONE);
}

Test(auth, del_key_not_found_returns_minus1, .init = setup) {
    cr_assert_eq(auth_del_key(&store, "ghost"), -1);
}

Test(auth, del_key_middle_preserves_others, .init = setup) {
    char t1[AUTH_TOKEN_BUF_SIZE], t2[AUTH_TOKEN_BUF_SIZE],
        t3[AUTH_TOKEN_BUF_SIZE];
    auth_generate_token(t1);
    auth_generate_token(t2);
    auth_generate_token(t3);

    auth_add_key(&store, "a", t1, ROLE_ADMIN);
    auth_add_key(&store, "b", t2, ROLE_READONLY);
    auth_add_key(&store, "c", t3, ROLE_ADMIN);

    cr_assert_eq(auth_del_key(&store, "b"), 0);
    cr_assert_eq(store.count, 2);
    cr_assert_eq(auth_verify(&store, t1), AUTH_ADMIN);
    cr_assert_eq(auth_verify(&store, t2), AUTH_NONE);
    cr_assert_eq(auth_verify(&store, t3), AUTH_ADMIN);
}

/* ── duplicate name rejection ────────────────────────────────────────── */

Test(auth, add_duplicate_name_rejected, .init = setup) {
    char t1[AUTH_TOKEN_BUF_SIZE], t2[AUTH_TOKEN_BUF_SIZE];
    auth_generate_token(t1);
    auth_generate_token(t2);

    cr_assert_eq(auth_add_key(&store, "dup", t1, ROLE_ADMIN), 0);
    cr_assert_eq(auth_add_key(&store, "dup", t2, ROLE_READONLY), -1);
    cr_assert_eq(store.count, 1);
}

/* ── capacity ────────────────────────────────────────────────────────── */

Test(auth, add_at_capacity_rejected, .init = setup) {
    char token[AUTH_TOKEN_BUF_SIZE];
    char name[AUTH_KEY_NAME_MAX];

    for(int i = 0; i < AUTH_MAX_KEYS; i++) {
        auth_generate_token(token);
        snprintf(name, sizeof(name), "key%d", i);
        cr_assert_eq(auth_add_key(&store, name, token, ROLE_READONLY), 0,
                     "Should succeed for key %d", i);
    }
    cr_assert_eq(store.count, AUTH_MAX_KEYS);

    auth_generate_token(token);
    cr_assert_eq(auth_add_key(&store, "overflow", token, ROLE_ADMIN), -1,
                 "Should fail when store is full");
}
