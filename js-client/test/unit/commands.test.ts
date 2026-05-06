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
    buildWhoami,
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

    it("builds create command with array fields", () => {
        expect(
            buildCreate("users", [
                { type: "int[]", name: "scores" },
                { type: "string[]", name: "tags" },
            ]),
        ).toBe("CREATE users (int[] scores, string[] tags)");
    });

    it("builds add and up commands", () => {
        expect(buildAdd("users", "*", [10, "Alice", true])).toBe(
            'ADD users * (10, "Alice", true)',
        );
        expect(buildUp("users", 1, [11, "_", false])).toBe('UP users 1 (11, _, false)');
    });

    it("builds add with array value", () => {
        expect(buildAdd("db", 1, [[1, 2, 3]])).toBe("ADD db 1 ([1, 2, 3])");
        expect(buildAdd("db", "*", [[true, false], "hello"])).toBe(
            'ADD db * ([true, false], "hello")',
        );
    });

    it("builds up with array replacement and append", () => {
        expect(buildUp("db", 1, [[10, 20]])).toBe("UP db 1 ([10, 20])");
        expect(buildUp("db", 1, [{ arrayAppend: 42 }])).toBe("UP db 1 ([...+42])");
        expect(buildUp("db", 1, [{ arrayAppend: "tag" }])).toBe('UP db 1 ([...+"tag"])');
        expect(buildUp("db", 1, ["_", [1, 2]])).toBe("UP db 1 (_, [1, 2])");
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

    it("builds whoami command", () => {
        expect(buildWhoami()).toBe("WHOAMI");
    });
});
