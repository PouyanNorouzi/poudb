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

    // ── Array tests ──────────────────────────────────────────────────────────

    it("creates a db with all array field types", async () => {
        const client = await createConnectedClient();
        const db = newDbName();

        try {
            const code = await client.create(db, [
                { type: "int[]", name: "scores" },
                { type: "double[]", name: "prices" },
                { type: "bool[]", name: "flags" },
                { type: "string[]", name: "tags" },
            ]);
            expect(code).toBeGreaterThanOrEqual(0);
        } finally {
            await client.disconnect();
        }
    });

    it("round-trips an int[] field", async () => {
        const client = await createConnectedClient();
        const db = newDbName();

        try {
            await client.create(db, [{ type: "int[]", name: "scores" }]);

            const key = await client.add(db, "*", [[10, 20, 30]]);
            expect(key).toBeGreaterThan(0);

            const result = await client.get(db, key);
            expect(result.data.rows).toHaveLength(1);
            expect(result.data.rows[0]?.scores).toBe("[10, 20, 30]");
        } finally {
            await client.disconnect();
        }
    });

    it("round-trips a double[] field", async () => {
        const client = await createConnectedClient();
        const db = newDbName();

        try {
            await client.create(db, [{ type: "double[]", name: "prices" }]);

            const key = await client.add(db, "*", [[1.5, 2.5, 3.5]]);
            const result = await client.get(db, key);
            expect(result.data.rows[0]?.prices).toBe("[1.5, 2.5, 3.5]");
        } finally {
            await client.disconnect();
        }
    });

    it("round-trips a bool[] field", async () => {
        const client = await createConnectedClient();
        const db = newDbName();

        try {
            await client.create(db, [{ type: "bool[]", name: "flags" }]);

            const key = await client.add(db, "*", [[true, false, true]]);
            const result = await client.get(db, key);
            expect(result.data.rows[0]?.flags).toBe("[true, false, true]");
        } finally {
            await client.disconnect();
        }
    });

    it("round-trips a string[] field", async () => {
        const client = await createConnectedClient();
        const db = newDbName();

        try {
            await client.create(db, [{ type: "string[]", name: "tags" }]);

            const key = await client.add(db, "*", [["alpha", "beta", "gamma"]]);
            const result = await client.get(db, key);
            expect(result.data.rows[0]?.tags).toBe("[alpha, beta, gamma]");
        } finally {
            await client.disconnect();
        }
    });

    it("stores and retrieves an empty array", async () => {
        const client = await createConnectedClient();
        const db = newDbName();

        try {
            await client.create(db, [{ type: "int[]", name: "scores" }]);

            const key = await client.add(db, "*", [[]]);
            const result = await client.get(db, key);
            expect(result.data.rows[0]?.scores).toBe("[]");
        } finally {
            await client.disconnect();
        }
    });

    it("replaces an array field with UP", async () => {
        const client = await createConnectedClient();
        const db = newDbName();

        try {
            await client.create(db, [{ type: "int[]", name: "scores" }]);

            const key = await client.add(db, "*", [[1, 2, 3]]);
            await client.up(db, key, [[10, 20]]);

            const result = await client.get(db, key);
            expect(result.data.rows[0]?.scores).toBe("[10, 20]");
        } finally {
            await client.disconnect();
        }
    });

    it("appends to an int[] field with UP arrayAppend", async () => {
        const client = await createConnectedClient();
        const db = newDbName();

        try {
            await client.create(db, [{ type: "int[]", name: "scores" }]);

            const key = await client.add(db, "*", [[1, 2]]);
            await client.up(db, key, [{ arrayAppend: 3 }]);

            const result = await client.get(db, key);
            expect(result.data.rows[0]?.scores).toBe("[1, 2, 3]");
        } finally {
            await client.disconnect();
        }
    });

    it("appends to a string[] field with UP arrayAppend", async () => {
        const client = await createConnectedClient();
        const db = newDbName();

        try {
            await client.create(db, [{ type: "string[]", name: "tags" }]);

            const key = await client.add(db, "*", [["foo", "bar"]]);
            await client.up(db, key, [{ arrayAppend: "baz" }]);

            const result = await client.get(db, key);
            expect(result.data.rows[0]?.tags).toBe("[foo, bar, baz]");
        } finally {
            await client.disconnect();
        }
    });

    it("ignores array field in UP with underscore sentinel", async () => {
        const client = await createConnectedClient();
        const db = newDbName();

        try {
            await client.create(db, [
                { type: "string", name: "label" },
                { type: "int[]", name: "scores" },
            ]);

            const key = await client.add(db, "*", ["original", [5, 6, 7]]);
            await client.up(db, key, ["updated", "_"]);

            const result = await client.get(db, key);
            expect(result.data.rows[0]?.label).toBe("updated");
            expect(result.data.rows[0]?.scores).toBe("[5, 6, 7]");
        } finally {
            await client.disconnect();
        }
    });

    it("getAll returns rows with array fields", async () => {
        const client = await createConnectedClient();
        const db = newDbName();

        try {
            await client.create(db, [
                { type: "string", name: "name" },
                { type: "int[]", name: "scores" },
            ]);

            await client.add(db, "*", ["Alice", [1, 2]]);
            await client.add(db, "*", ["Bob", [3, 4, 5]]);

            const all = await client.getAll(db);
            expect(all.data.rows).toHaveLength(2);

            const scoreValues = all.data.rows.map((r) => r.scores).sort();
            expect(scoreValues).toEqual(["[1, 2]", "[3, 4, 5]"].sort());
        } finally {
            await client.disconnect();
        }
    });

    it("searches a scalar field in a db that also has array fields", async () => {
        const client = await createConnectedClient();
        const db = newDbName();

        try {
            await client.create(db, [
                { type: "string", name: "name" },
                { type: "int[]", name: "scores" },
            ]);

            await client.add(db, "*", ["Alice", [10, 20]]);
            await client.add(db, "*", ["Bob", [30]]);

            const result = await client.search(db, "name", "Alice");
            expect(result.data.rows).toHaveLength(1);
            expect(result.data.rows[0]?.name).toBe("Alice");
        } finally {
            await client.disconnect();
        }
    });

    it("searches a string[] field by contained value", async () => {
        const client = await createConnectedClient();
        const db = newDbName();

        try {
            await client.create(db, [
                { type: "string", name: "name" },
                { type: "string[]", name: "tags" },
            ]);

            await client.add(db, "*", ["Alice", ["sale", "featured"]]);
            await client.add(db, "*", ["Bob", ["new"]]);

            const result = await client.search(db, "tags", "sale");
            expect(result.data.rows).toHaveLength(1);
            expect(result.data.rows[0]?.name).toBe("Alice");
        } finally {
            await client.disconnect();
        }
    });

    it("searches an int[] field by contained value", async () => {
        const client = await createConnectedClient();
        const db = newDbName();

        try {
            await client.create(db, [
                { type: "string", name: "name" },
                { type: "int[]", name: "scores" },
            ]);

            await client.add(db, "*", ["Alice", [10, 42, 99]]);
            await client.add(db, "*", ["Bob", [1, 2, 3]]);

            const result = await client.search(db, "scores", 42);
            expect(result.data.rows).toHaveLength(1);
            expect(result.data.rows[0]?.name).toBe("Alice");
        } finally {
            await client.disconnect();
        }
    });

    it("handles mixed scalar and array fields together", async () => {
        const client = await createConnectedClient();
        const db = newDbName();

        try {
            await client.create(db, [
                { type: "string", name: "name" },
                { type: "int", name: "age" },
                { type: "bool[]", name: "flags" },
                { type: "double[]", name: "prices" },
            ]);

            const key = await client.add(db, "*", ["Alice", 30, [true, false], [1.1, 2.2]]);

            const result = await client.get(db, key);
            const row = result.data.rows[0];
            expect(row?.name).toBe("Alice");
            expect(row?.age).toBe("30");
            expect(row?.flags).toBe("[true, false]");
            expect(row?.prices).toBe("[1.1, 2.2]");
        } finally {
            await client.disconnect();
        }
    });

    it("replaces array field with empty array via UP", async () => {
        const client = await createConnectedClient();
        const db = newDbName();

        try {
            await client.create(db, [{ type: "string[]", name: "tags" }]);

            const key = await client.add(db, "*", [["a", "b", "c"]]);
            await client.up(db, key, [[]]);

            const result = await client.get(db, key);
            expect(result.data.rows[0]?.tags).toBe("[]");
        } finally {
            await client.disconnect();
        }
    });
});
