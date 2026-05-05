#ifndef AUTH_H
#define AUTH_H

#include <sodium.h>

#define AUTH_MAX_KEYS       64
#define AUTH_KEY_NAME_MAX   64
/* 32 random bytes → 64 hex chars + NUL */
#define AUTH_TOKEN_HEX_LEN  64
#define AUTH_TOKEN_BUF_SIZE (AUTH_TOKEN_HEX_LEN + 1)

typedef enum {
    ROLE_READONLY = 0,
    ROLE_ADMIN    = 1
} KeyRole;

typedef enum {
    AUTH_NONE     = 0,
    AUTH_READONLY = 1,
    AUTH_ADMIN    = 2
} AuthLevel;

typedef struct {
    char    name[AUTH_KEY_NAME_MAX];
    char    hash[crypto_pwhash_STRBYTES];
    KeyRole role;
} AuthKey;

typedef struct {
    AuthKey keys[AUTH_MAX_KEYS];
    int     count;
} AuthStore;

/* Global auth store shared by all modules */
extern AuthStore g_auth_store;

/**
 * Initialize the auth store to an empty state.
 */
void auth_store_init(AuthStore* store);

/**
 * Generate a cryptographically random 32-byte token encoded as 64 hex chars.
 * Writes AUTH_TOKEN_BUF_SIZE bytes (including NUL) into out.
 * Returns 0 on success, -1 on error.
 */
int auth_generate_token(char out[AUTH_TOKEN_BUF_SIZE]);

/**
 * Add a named key to the store. Hashes the token with Argon2id.
 * Returns 0 on success, -1 if the store is full, the name already exists,
 * or hashing fails.
 */
int auth_add_key(AuthStore* store,
                 const char* name,
                 const char* token,
                 KeyRole role);

/**
 * Remove a key by name.
 * Returns 0 on success, -1 if not found.
 */
int auth_del_key(AuthStore* store, const char* name);

/**
 * Verify a token against all stored keys.
 * Returns AUTH_ADMIN or AUTH_READONLY on a match, AUTH_NONE otherwise.
 */
AuthLevel auth_verify(const AuthStore* store, const char* token);

#endif /* AUTH_H */
