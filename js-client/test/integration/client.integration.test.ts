import { describe, expect, it } from "vitest";
import { createConnectedClient, integrationEnabled, newDbName } from "./fixtures";

describe.runIf(integrationEnabled())("poudb integration", () => {
    it("round-trips create/add/get/count", async () => {
        const client = await createConnectedClient();
        const db = newDbName();

        try {
            await client.create(db, [
                { type: "int", name: "age" },
                { type: "string", name: "name" },
            ]);

            const key = await client.add(db, "*", [30, "Alice"]);
            expect(key).toBeGreaterThan(0);

            const table = await client.get(db, key);
            expect(table.data.rows).toHaveLength(1);
            expect(table.data.rows[0]?.name).toBe("Alice");

            const count = await client.count(db);
            expect(count).toBe(1);
        } finally {
            await client.disconnect();
        }
    });

    it("supports up/search/getAll/del semantics", async () => {
        const client = await createConnectedClient();
        const db = newDbName();

        try {
            await client.create(db, [
                { type: "int", name: "age" },
                { type: "string", name: "name" },
                { type: "bool", name: "active" },
            ]);

            const k1 = await client.add(db, "*", [20, "Mina", true]);
            const k2 = await client.add(db, "*", [34, "Arman", false]);

            const upCode = await client.up(db, k2, [35, "_", true]);
            expect(upCode).toBe(k2);

            const search = await client.search(db, "active", true, ["name", "active"]);
            const names = search.data.rows.map((row) => row.name).sort();
            expect(names).toEqual(["Arman", "Mina"]);

            const all = await client.getAll(db, ["key", "name"]);
            const keys = all.data.rows.map((row) => Number.parseInt(row.key, 10)).sort((a, b) => a - b);
            expect(keys).toEqual([k1, k2].sort((a, b) => a - b));

            const delCode = await client.del(db, k1);
            expect(delCode).toBe(k1);

            const count = await client.count(db);
            expect(count).toBe(1);
        } finally {
            await client.disconnect();
        }
    });
});
