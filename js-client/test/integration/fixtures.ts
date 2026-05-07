import { randomUUID } from "node:crypto";
import { PoudbClient } from "../../src/client";

export function integrationEnabled(): boolean {
    return process.env.POUDB_INTEGRATION === "1";
}

export function newDbName(): string {
    return `jslib_${randomUUID().replace(/-/g, "")}`;
}

export async function createConnectedClient(): Promise<PoudbClient> {
    const key = process.env.POUDB_AUTH_TOKEN;
    const client = new PoudbClient({ key });
    await client.connect();
    return client;
}
