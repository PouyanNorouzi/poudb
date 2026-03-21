import { describe, expect, it } from "vitest";
import { parseTable } from "../../src/parsers/table";

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
