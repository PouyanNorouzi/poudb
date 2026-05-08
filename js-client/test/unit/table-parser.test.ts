import { describe, expect, it } from "vitest";
import { coerceTable, coerceValue, parseTable } from "../../src/parsers/table";
import type { SchemaField } from "../../src/types";

describe("table parser", () => {
    it("parses rows from table output", () => {
        const parsed = parseTable("| key | name |\n|-----|------|\n| 1 | Alice |\n| 2 | Bob |\n");
        expect(parsed.headers).toEqual(["key", "name"]);
        expect(parsed.rows).toHaveLength(2);
        expect(parsed.rows[1]?.name).toBe("Bob");
    });

    it("handles no-row sentinel", () => {
        const parsed = parseTable("(No rows)\n");
        expect(parsed.headers).toEqual([]);
        expect(parsed.rows).toEqual([]);
    });

    it("throws on malformed tables", () => {
        expect(() => parseTable("not a table")).toThrow();
    });
});

describe("coerceValue", () => {
    it("coerces int", () => expect(coerceValue("42", "int")).toBe(42));
    it("coerces double", () => expect(coerceValue("3.14", "double")).toBeCloseTo(3.14));
    it("coerces bool true", () => expect(coerceValue("true", "bool")).toBe(true));
    it("coerces bool false", () => expect(coerceValue("false", "bool")).toBe(false));
    it("passes string through", () => expect(coerceValue("hello", "string")).toBe("hello"));
    it("coerces int[]", () => expect(coerceValue("[1, 2, 3]", "int[]")).toEqual([1, 2, 3]));
    it("coerces double[]", () => expect(coerceValue("[1.1, 2.2]", "double[]")).toEqual([1.1, 2.2]));
    it("coerces bool[]", () =>
        expect(coerceValue("[true, false, true]", "bool[]")).toEqual([true, false, true]));
    it("coerces string[]", () =>
        expect(coerceValue('["alpha", "beta"]', "string[]")).toEqual(["alpha", "beta"]));
    it("coerces empty int[]", () => expect(coerceValue("[]", "int[]")).toEqual([]));
    it("coerces empty double[]", () => expect(coerceValue("[]", "double[]")).toEqual([]));
    it("coerces empty bool[]", () => expect(coerceValue("[]", "bool[]")).toEqual([]));
    it("coerces empty string[]", () => expect(coerceValue("[]", "string[]")).toEqual([]));
    it("coerces single-element int[]", () => expect(coerceValue("[7]", "int[]")).toEqual([7]));
    it("coerces negative int", () => expect(coerceValue("-5", "int")).toBe(-5));
    it("coerces zero int", () => expect(coerceValue("0", "int")).toBe(0));
    it("coerces negative double", () => expect(coerceValue("-1.5", "double")).toBeCloseTo(-1.5));
    it("coerces int[] with negative values", () =>
        expect(coerceValue("[-1, 0, 1]", "int[]")).toEqual([-1, 0, 1]));
});

describe("coerceTable — array schema fields", () => {
    const schema = [
        { name: "scores", type: "int[]" },
        { name: "prices", type: "double[]" },
        { name: "flags", type: "bool[]" },
        { name: "tags", type: "string[]" },
    ] as const satisfies readonly SchemaField[];

    it("coerces all array field types", () => {
        const raw =
            '| key | scores | prices | flags | tags | time_created | time_updated |\n' +
            '|-----|--------|--------|-------|------|--------------|--------------|\n' +
            '| 1 | [1, 2, 3] | [1.1, 2.2] | [true, false] | ["a", "b"] | 100 | 200 |\n';
        const parsed = parseTable(raw);
        const table = coerceTable(parsed, schema);
        const row = table.rows[0]!;
        expect(row.scores).toEqual([1, 2, 3]);
        expect(row.prices).toEqual([1.1, 2.2]);
        expect(row.flags).toEqual([true, false]);
        expect(row.tags).toEqual(["a", "b"]);
    });

    it("coerces empty array fields", () => {
        const raw =
            "| key | scores | prices | flags | tags | time_created | time_updated |\n" +
            "|-----|--------|--------|-------|------|--------------|--------------|\n" +
            "| 1 | [] | [] | [] | [] | 100 | 200 |\n";
        const parsed = parseTable(raw);
        const table = coerceTable(parsed, schema);
        const row = table.rows[0]!;
        expect(row.scores).toEqual([]);
        expect(row.prices).toEqual([]);
        expect(row.flags).toEqual([]);
        expect(row.tags).toEqual([]);
    });
});

describe("coerceTable — fallback for unknown columns", () => {
    it("passes through columns not in schema as strings", () => {
        const schema = [{ name: "age", type: "int" }] as const satisfies readonly SchemaField[];
        const raw =
            "| key | age | extra | time_created | time_updated |\n" +
            "|-----|-----|-------|--------------|--------------|\n" +
            "| 1 | 10 | hello | 100 | 200 |\n";
        const parsed = parseTable(raw);
        const table = coerceTable(parsed, schema);
        const row = table.rows[0]! as Record<string, unknown>;
        expect(row["age"]).toBe(10);
        expect(row["extra"]).toBe("hello");
        expect(row["key"]).toBe(1);
    });
});

describe("coerceTable", () => {
    const schema = [
        { name: "age", type: "int" },
        { name: "score", type: "double" },
        { name: "active", type: "bool" },
        { name: "name", type: "string" },
    ] as const satisfies readonly SchemaField[];

    const raw =
        "| key | age | score | active | name | time_created | time_updated |\n" +
        "|-----|-----|-------|--------|------|--------------|--------------|\n" +
        "| 1 | 30 | 9.5 | true | Alice | 1000 | 2000 |\n";

    it("coerces row values to their schema types", () => {
        const parsed = parseTable(raw);
        const table = coerceTable(parsed, schema);

        expect(table.rows).toHaveLength(1);
        const row = table.rows[0]!;
        expect(row.age).toBe(30);
        expect(row.score).toBeCloseTo(9.5);
        expect(row.active).toBe(true);
        expect(row.name).toBe("Alice");
    });

    it("coerces built-in key and timestamp fields to numbers", () => {
        const parsed = parseTable(raw);
        const table = coerceTable(parsed, schema);
        const row = table.rows[0]!;
        expect(row.key).toBe(1);
        expect(row.time_created).toBe(1000);
        expect(row.time_updated).toBe(2000);
    });

    it("preserves headers and raw", () => {
        const parsed = parseTable(raw);
        const table = coerceTable(parsed, schema);
        expect(table.headers).toEqual(parsed.headers);
        expect(table.raw).toBe(parsed.raw);
    });

    it("returns empty rows for no-rows response", () => {
        const parsed = parseTable("(No rows)\n");
        const table = coerceTable(parsed, schema);
        expect(table.rows).toEqual([]);
    });
});
