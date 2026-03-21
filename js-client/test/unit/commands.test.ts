import { describe, expect, it } from "vitest";
import {
    buildAdd,
    buildCount,
    buildCreate,
    buildCreateIndex,
    buildDel,
    buildGet,
    buildGetAll,
    buildSearch,
    buildUp,
} from "../../src/protocol/commands";

describe("command builders", () => {
    it("builds create command", () => {
        expect(
            buildCreate("users", [
                { type: "int", name: "age" },
                { type: "string", name: "name" },
            ]),
        ).toBe("CREATE users (int age, string name)");
    });

    it("builds add and up commands", () => {
        expect(buildAdd("users", "*", [10, "Alice", true])).toBe(
            'ADD users * (10, "Alice", true)',
        );
        expect(buildUp("users", 1, [11, "_", false])).toBe('UP users 1 (11, _, false)');
    });

    it("builds getters and counters", () => {
        expect(buildGet("users", 1)).toBe("GET users 1");
        expect(buildGet("users", 1, ["name", "age"])).toBe("GET users 1 (name, age)");
        expect(buildGetAll("users")).toBe("GET_ALL users");
        expect(buildGetAll("users", ["name"])).toBe("GET_ALL users (name)");
        expect(buildSearch("users", "name", "Alice", ["name", "age"])).toBe(
            'SEARCH users name "Alice" (name, age)',
        );
        expect(buildDel("users", 7)).toBe("DEL users 7");
        expect(buildCount("users")).toBe("COUNT users");
        expect(buildCreateIndex("users", "name")).toBe("CREATE_INDEX users name");
    });
});
