# Changelog

## 0.2.0

- Add token-based authentication support.
- New `ClientOptions.key` field: if set, `connect()` automatically sends `AUTH <key>` and throws `ServerMessageError` on failure.
- New `PoudbClient` methods: `auth(token)`, `addKey(name, role)`, `delKey(name)`, `listKeys()`.
- New command builders: `buildAuth`, `buildAddKey`, `buildDelKey`, `buildListKeys`.
- New `KeyRole` type exported from package root (`"admin" | "readonly"`).

## 0.1.1

- Fix ESM output compatibility for strict Node resolution by using explicit `.js` internal imports.

## 0.1.0

- Initial scaffold for TypeScript poudb client
- TCP connection core with timeout and request queue
- Typed command builders and table parser
- Unit and integration test skeleton
