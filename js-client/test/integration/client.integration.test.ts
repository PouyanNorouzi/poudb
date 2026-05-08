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
            expect(table.rows).toHaveLength(1);
            expect(table.rows[0]?.name).toBe("Alice");

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

            const search = await client.search(db, "active", true, { fields: ["name", "active"] });
            const names = search.rows.map((row) => row.name).sort();
            expect(names).toEqual(["Arman", "Mina"]);

            const all = await client.getAll(db, { fields: ["key", "name"] });
            const keys = all.rows.map((row) => Number.parseInt(row.key, 10)).sort((a, b) => a - b);
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
            expect(result.rows).toHaveLength(1);
            expect(result.rows[0]?.scores).toBe("[10, 20, 30]");
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
            expect(result.rows[0]?.prices).toBe("[1.5, 2.5, 3.5]");
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
            expect(result.rows[0]?.flags).toBe("[true, false, true]");
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
            expect(result.rows[0]?.tags).toBe('["alpha", "beta", "gamma"]');
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
            expect(result.rows[0]?.scores).toBe("[]");
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
            expect(result.rows[0]?.scores).toBe("[10, 20]");
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
            expect(result.rows[0]?.scores).toBe("[1, 2, 3]");
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
            expect(result.rows[0]?.tags).toBe('["foo", "bar", "baz"]');
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
            expect(result.rows[0]?.label).toBe("updated");
            expect(result.rows[0]?.scores).toBe("[5, 6, 7]");
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
            expect(all.rows).toHaveLength(2);

            const scoreValues = all.rows.map((r) => r.scores).sort();
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
            expect(result.rows).toHaveLength(1);
            expect(result.rows[0]?.name).toBe("Alice");
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
            expect(result.rows).toHaveLength(1);
            expect(result.rows[0]?.name).toBe("Alice");
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
            expect(result.rows).toHaveLength(1);
            expect(result.rows[0]?.name).toBe("Alice");
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
            const row = result.rows[0];
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
            expect(result.rows[0]?.tags).toBe("[]");
        } finally {
            await client.disconnect();
        }
    });

    // ── Timestamp tests ───────────────────────────────────────────────────────

    it("get includes time_created and time_updated columns", async () => {
        const client = await createConnectedClient();
        const db = newDbName();

        try {
            await client.create(db, [{ type: "int", name: "val" }]);
            const key = await client.add(db, "*", [99]);

            const result = await client.get(db, key);
            expect(result.headers).toContain("time_created");
            expect(result.headers).toContain("time_updated");

            const row = result.rows[0];
            expect(row).toBeDefined();
            const tc = Number.parseInt(row!.time_created, 10);
            const tu = Number.parseInt(row!.time_updated, 10);
            expect(Number.isNaN(tc)).toBe(false);
            expect(Number.isNaN(tu)).toBe(false);
            expect(tc).toBeGreaterThan(0);
            expect(tc).toBe(tu);
        } finally {
            await client.disconnect();
        }
    });

    it("getAll includes time_created and time_updated columns", async () => {
        const client = await createConnectedClient();
        const db = newDbName();

        try {
            await client.create(db, [{ type: "string", name: "label" }]);
            await client.add(db, "*", ["hello"]);

            const result = await client.getAll(db);
            expect(result.headers).toContain("time_created");
            expect(result.headers).toContain("time_updated");

            const row = result.rows[0];
            expect(row).toBeDefined();
            expect(Number.parseInt(row!.time_created, 10)).toBeGreaterThan(0);
            expect(Number.parseInt(row!.time_updated, 10)).toBeGreaterThan(0);
        } finally {
            await client.disconnect();
        }
    });

    it("search includes time_created and time_updated columns", async () => {
        const client = await createConnectedClient();
        const db = newDbName();

        try {
            await client.create(db, [{ type: "string", name: "name" }]);
            await client.add(db, "*", ["Bob"]);

            const result = await client.search(db, "name", "Bob");
            expect(result.headers).toContain("time_created");
            expect(result.headers).toContain("time_updated");
        } finally {
            await client.disconnect();
        }
    });

    it("time_updated advances after UP", async () => {
        const client = await createConnectedClient();
        const db = newDbName();

        try {
            await client.create(db, [{ type: "int", name: "val" }]);
            const key = await client.add(db, "*", [1]);

            const before = await client.get(db, key);
            const tcBefore = Number.parseInt(before.rows[0]!.time_created, 10);

            // wait 1s to ensure time advances
            await new Promise((resolve) => setTimeout(resolve, 1100));

            await client.up(db, key, [2]);

            const after = await client.get(db, key);
            const tcAfter = Number.parseInt(after.rows[0]!.time_created, 10);
            const tuAfter = Number.parseInt(after.rows[0]!.time_updated, 10);

            // created_at must not change after UP
            expect(tcAfter).toBe(tcBefore);
            // updated_at must be >= created_at
            expect(tuAfter).toBeGreaterThanOrEqual(tcAfter);
        } finally {
            await client.disconnect();
        }
    });

    // ── Typed schema tests ────────────────────────────────────────────────────

    it("get with schema returns coerced types", async () => {
        const client = await createConnectedClient();
        const db = newDbName();

        const schema = [
            { type: "int", name: "age" },
            { type: "bool", name: "active" },
            { type: "string", name: "name" },
        ] as const;

        try {
            await client.create(db, [...schema]);
            const key = await client.add(db, "*", [25, true, "Leila"]);

            const result = await client.get(db, key, { schema });
            const row = result.rows[0]!;
            expect(row.age).toBe(25);
            expect(row.active).toBe(true);
            expect(row.name).toBe("Leila");
            expect(row.key).toBe(key);
        } finally {
            await client.disconnect();
        }
    });

    it("getAll with schema returns coerced types", async () => {
        const client = await createConnectedClient();
        const db = newDbName();

        const schema = [
            { type: "double", name: "score" },
            { type: "string", name: "label" },
        ] as const;

        try {
            await client.create(db, [...schema]);
            await client.add(db, "*", [1.5, "alpha"]);
            await client.add(db, "*", [2.5, "beta"]);

            const result = await client.getAll(db, { schema });
            expect(result.rows).toHaveLength(2);
            for (const row of result.rows) {
                expect(typeof row.score).toBe("number");
                expect(typeof row.label).toBe("string");
                expect(typeof row.key).toBe("number");
            }
        } finally {
            await client.disconnect();
        }
    });

    it("search with schema returns coerced types", async () => {
        const client = await createConnectedClient();
        const db = newDbName();

        const schema = [
            { type: "int", name: "age" },
            { type: "string", name: "name" },
        ] as const;

        try {
            await client.create(db, [...schema]);
            await client.add(db, "*", [30, "Alice"]);
            await client.add(db, "*", [25, "Bob"]);

            const result = await client.search(db, "name", "Alice", { schema });
            expect(result.rows).toHaveLength(1);
            expect(result.rows[0]!.age).toBe(30);
            expect(result.rows[0]!.name).toBe("Alice");
        } finally {
            await client.disconnect();
        }
    });

    it("get with schema and fields projection returns only requested fields coerced", async () => {
        const client = await createConnectedClient();
        const db = newDbName();

        const schema = [
            { type: "int", name: "age" },
            { type: "string", name: "name" },
            { type: "bool", name: "active" },
        ] as const;

        try {
            await client.create(db, [...schema]);
            const key = await client.add(db, "*", [40, "Dara", true]);

            const result = await client.get(db, key, { fields: ["age", "active"], schema });
            expect(result.headers).toContain("age");
            expect(result.headers).toContain("active");
            expect(result.headers).not.toContain("name");
            expect(result.rows[0]!.age).toBe(40);
            expect(result.rows[0]!.active).toBe(true);
        } finally {
            await client.disconnect();
        }
    });

    it("getAll with schema and fields projection returns only requested fields coerced", async () => {
        const client = await createConnectedClient();
        const db = newDbName();

        const schema = [
            { type: "double", name: "score" },
            { type: "string", name: "label" },
        ] as const;

        try {
            await client.create(db, [...schema]);
            await client.add(db, "*", [9.9, "top"]);

            const result = await client.getAll(db, { fields: ["score"], schema });
            expect(result.headers).toContain("score");
            expect(result.headers).not.toContain("label");
            expect(typeof result.rows[0]!.score).toBe("number");
        } finally {
            await client.disconnect();
        }
    });

    it("typed get returns empty rows when no match exists", async () => {
        const client = await createConnectedClient();
        const db = newDbName();

        const schema = [{ type: "int", name: "val" }] as const;

        try {
            await client.create(db, [...schema]);
            // Search returns no rows; coerceTable should produce an empty array
            const result = await client.search(db, "val", 999, { schema });
            expect(result.rows).toEqual([]);
        } finally {
            await client.disconnect();
        }
    });

    it("get with schema coerces array field types end-to-end", async () => {
        const client = await createConnectedClient();
        const db = newDbName();

        const schema = [
            { type: "int[]", name: "scores" },
            { type: "bool[]", name: "flags" },
            { type: "string[]", name: "tags" },
            { type: "double[]", name: "prices" },
        ] as const;

        try {
            await client.create(db, [...schema]);
            const key = await client.add(db, "*", [[1, 2], [true, false], ["x", "y"], [1.5, 2.5]]);

            const result = await client.get(db, key, { schema });
            const row = result.rows[0]!;
            expect(row.scores).toEqual([1, 2]);
            expect(row.flags).toEqual([true, false]);
            expect(row.tags).toEqual(["x", "y"]);
            expect(row.prices).toEqual([1.5, 2.5]);
        } finally {
            await client.disconnect();
        }
    });

    it("listKeys returns flat TypedTable shape with expected columns", async () => {
        const client = await createConnectedClient();

        try {
            const result = await client.listKeys();
            expect(result).toHaveProperty("headers");
            expect(result).toHaveProperty("rows");
            expect(result).toHaveProperty("raw");
            expect(result.headers).toContain("name");
            expect(result.headers).toContain("role");
        } finally {
            await client.disconnect();
        }
    });
});
