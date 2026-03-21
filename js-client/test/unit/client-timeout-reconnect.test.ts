import { describe, expect, it } from "vitest";
import { PoudbClient } from "../../src/client";

describe("client options", () => {
    it("creates client with reconnect options", () => {
        const client = new PoudbClient({
            reconnect: {
                enabled: true,
                maxRetries: 1,
                retryDelayMs: 10,
            },
        });

        expect(client.isConnected()).toBe(false);
    });
});
