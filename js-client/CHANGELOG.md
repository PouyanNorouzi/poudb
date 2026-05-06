# Changelog

## 0.4.0

- Add array field type support: `"int[]"`, `"double[]"`, `"bool[]"`, `"string[]"` in `SchemaType`.
- New types: `ArraySchemaType`, `ScalarSchemaType`, `ScalarValue`, `ArrayValue`, `ArrayAppend`.
- `CommandValue` now includes `ArrayValue` (array of scalars) for ADD commands.
- `UpdateValue` now supports `ArrayAppend` (`{ arrayAppend: ScalarValue }`) for UP append syntax.
- `buildAdd` / `buildUp` serialize arrays as `[el1, el2, ...]` and appends as `[...+val]`.
- New escaping helpers: `formatArray`, `formatArrayAppend`, `formatScalar`.
- `buildSearch` search value restricted to scalar (`string | number | boolean`) — arrays use contains semantics server-side.

## 0.3.1

- New `PoudbClient.whoami()` method: returns the authenticated key name for the current connection.
- New command builder: `buildWhoami`.

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
