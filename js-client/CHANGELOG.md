# Changelog

## 0.5.0

### Breaking changes

- `get()`, `getAll()`, `search()`, and `listKeys()` now return `TypedTable<T>` (`{ headers, rows, raw }`) instead of `QueryResult<ParsedTable>` (`{ raw, data: { headers, rows } }`). Update all access from `.data.rows` / `.data.headers` to `.rows` / `.headers`.
- The `fields` positional parameter of `get()`, `getAll()`, and `search()` has been replaced by an options object. Migrate: `get(db, key, ["f1"])` → `get(db, key, { fields: ["f1"] })`, `getAll(db, ["f1"])` → `getAll(db, { fields: ["f1"] })`, `search(db, f, v, ["f1"])` → `search(db, f, v, { fields: ["f1"] })`.

### New features

- Schema-aware typed queries: pass `schema` in the options object of `get()`, `getAll()`, or `search()` to receive rows with coerced JavaScript types instead of raw strings. Use an `as const` schema for full TypeScript inference.
  ```ts
  const schema = [
      { type: "int", name: "age" },
      { type: "bool", name: "active" },
  ] as const;
  const result = await client.get(db, key, { schema });
  // result.rows[0].age is number, result.rows[0].active is boolean
  ```
- New exported types: `TypedTable<T>`, `SchemaTypeToJS<T>`, `SchemaToRow<T>`.
- New exported functions: `coerceValue(value, type)`, `coerceTable(parsed, schema)` for lower-level use.
- `key`, `time_created`, and `time_updated` are always coerced to `number` when a schema is provided, regardless of whether they appear in the schema definition.

## 0.4.1

- All table responses (GET, GET_ALL, SEARCH) now include `time_created` and `time_updated` columns automatically.
- Both columns contain Unix epoch integer strings (seconds since 1970).
- `time_created` is set once when a row is inserted; `time_updated` is refreshed on every UP.
- Server snapshot format bumped to version 4; snapshots from earlier versions are discarded on load.

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
