import { describe, expect, it } from "vitest";
import { discriminateResponse } from "../../src/net/response-discriminator";

describe("response discriminator", () => {
    it("detects code response", () => {
        const res = discriminateResponse("42\n");
        expect(res.kind).toBe("code");
        if (res.kind === "code") {
            expect(res.code).toBe(42);
        }
    });

    it("detects table response", () => {
        const raw = "| key | name |\n|-----|------|\n| 1 | Alice |\n";
        const res = discriminateResponse(raw);
        expect(res.kind).toBe("table");
        if (res.kind === "table") {
            expect(res.parsed.rows[0]?.name).toBe("Alice");
        }
    });

    it("detects message response", () => {
        const res = discriminateResponse("Database not found\n");
        expect(res.kind).toBe("message");
        if (res.kind === "message") {
            expect(res.message).toContain("not found");
        }
    });

    it("detects LIST_KEYS response as table", () => {
        const raw = "| name | role |\n| - | - |\n| mykey | admin |\n";
        const res = discriminateResponse(raw);
        expect(res.kind).toBe("table");
        if (res.kind === "table") {
            expect(res.parsed.headers).toEqual(["name", "role"]);
            expect(res.parsed.rows[0]?.name).toBe("mykey");
            expect(res.parsed.rows[0]?.role).toBe("admin");
        }
    });

    it("detects LIST_KEYS header-only response as table", () => {
        const raw = "| name | role |\n| - | - |\n";
        const res = discriminateResponse(raw);
        expect(res.kind).toBe("table");
        if (res.kind === "table") {
            expect(res.parsed.headers).toEqual(["name", "role"]);
            expect(res.parsed.rows).toHaveLength(0);
        }
    });
});
