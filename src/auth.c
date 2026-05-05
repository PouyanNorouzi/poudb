#include "auth.h"

#include <string.h>

AuthStore g_auth_store;

void auth_store_init(AuthStore* store) {
    memset(store, 0, sizeof(AuthStore));
}

int auth_generate_token(char out[AUTH_TOKEN_BUF_SIZE]) {
    unsigned char raw[32];
    randombytes_buf(raw, sizeof(raw));
    sodium_bin2hex(out, AUTH_TOKEN_BUF_SIZE, raw, sizeof(raw));
    return 0;
}

int auth_add_key(AuthStore* store,
                 const char* name,
                 const char* token,
                 KeyRole role) {
    if(store->count >= AUTH_MAX_KEYS) {
        return -1;
    }

    /* Reject duplicate names */
    for(int i = 0; i < store->count; i++) {
        if(strcmp(store->keys[i].name, name) == 0) {
            return -1;
        }
    }

    AuthKey* key = &store->keys[store->count];
    strncpy(key->name, name, AUTH_KEY_NAME_MAX - 1);
    key->name[AUTH_KEY_NAME_MAX - 1] = '\0';
    key->role                        = role;

    if(crypto_pwhash_str(key->hash,
                         token,
                         strlen(token),
                         crypto_pwhash_OPSLIMIT_INTERACTIVE,
                         crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0) {
        /* Hash failed (out of memory) — scrub partial entry */
        memset(key, 0, sizeof(AuthKey));
        return -1;
    }

    store->count++;
    return 0;
}

int auth_del_key(AuthStore* store, const char* name) {
    for(int i = 0; i < store->count; i++) {
        if(strcmp(store->keys[i].name, name) == 0) {
            /* Shift remaining entries left and scrub the vacated slot */
            for(int j = i; j < store->count - 1; j++) {
                store->keys[j] = store->keys[j + 1];
            }
            memset(&store->keys[--store->count], 0, sizeof(AuthKey));
            return 0;
        }
    }
    return -1;
}

AuthLevel auth_verify(const AuthStore* store, const char* token) {
    for(int i = 0; i < store->count; i++) {
        if(crypto_pwhash_str_verify(store->keys[i].hash,
                                    token,
                                    strlen(token)) == 0) {
            return (store->keys[i].role == ROLE_ADMIN) ? AUTH_ADMIN
                                                       : AUTH_READONLY;
        }
    }
    return AUTH_NONE;
}
