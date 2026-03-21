# poudb JS Client

TypeScript-first client library for poudb.

## Install

```bash
npm install @pouyan/poudb-client
```

## Quick Start

```ts
import { PoudbClient } from "@pouyan/poudb-client";

const client = new PoudbClient();
await client.connect();

await client.create("users", [
  { type: "int", name: "age" },
  { type: "string", name: "name" },
]);

const key = await client.add("users", "*", [30, "Alice"]);
const result = await client.get("users", key);
console.log(result.data.rows);

await client.disconnect();
```

## Raw Commands

```ts
const response = await client.sendRaw('COUNT users');
```

## Notes

- Commands are newline-terminated automatically.
- Timeout and optional reconnect can be configured via constructor options.
- Integration tests are disabled by default and require `POUDB_INTEGRATION=1`.
