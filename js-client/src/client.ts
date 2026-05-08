import { ReconnectExhaustedError, ServerMessageError } from "./errors.js";
import { PoudbConnection } from "./net/connection.js";
import { discriminateResponse } from "./net/response-discriminator.js";
import {
    buildAdd,
    buildAddKey,
    buildAuth,
    buildCount,
    buildCreate,
    buildCreateIndex,
    buildDel,
    buildDelKey,
    buildDelTable,
    buildGet,
    buildGetAll,
    buildListKeys,
    buildSearch,
    buildUp,
    buildWhoami,
} from "./protocol/commands.js";
import { assertCode, assertTypedTable } from "./operations.js";
import {
    ClientOptions,
    CommandResponse,
    CommandValue,
    KeyRole,
    SchemaField,
    SchemaToRow,
    TypedTable,
    UpdateValue,
} from "./types.js";

const DEFAULT_HOST = "127.0.0.1";
const DEFAULT_PORT = 3005;
const DEFAULT_TIMEOUT_MS = 4000;
const DEFAULT_SETTLE_TIME_MS = 20;
const DEFAULT_MAX_RETRIES = 2;
const DEFAULT_RETRY_DELAY_MS = 100;

export class PoudbClient {
    private readonly host: string;
    private readonly port: number;
    private readonly reconnectEnabled: boolean;
    private readonly maxRetries: number;
    private readonly retryDelayMs: number;
    private readonly connection: PoudbConnection;

    public constructor(private readonly options: ClientOptions = {}) {
        this.host = options.host ?? DEFAULT_HOST;
        this.port = options.port ?? DEFAULT_PORT;
        this.reconnectEnabled = options.reconnect?.enabled ?? false;
        this.maxRetries = options.reconnect?.maxRetries ?? DEFAULT_MAX_RETRIES;
        this.retryDelayMs = options.reconnect?.retryDelayMs ?? DEFAULT_RETRY_DELAY_MS;
        this.connection = new PoudbConnection(
            options.timeoutMs ?? DEFAULT_TIMEOUT_MS,
            options.settleTimeMs ?? DEFAULT_SETTLE_TIME_MS,
        );
    }

    public async connect(): Promise<void> {
        await this.connection.connect(this.host, this.port);
        if (this.options.key != null) {
            const response = await this.sendRaw(buildAuth(this.options.key));
            if (response.kind === "message") {
                await this.connection.disconnect();
                throw new ServerMessageError(response.message);
            }
        }
    }

    public disconnect(): Promise<void> {
        return this.connection.disconnect();
    }

    public isConnected(): boolean {
        return this.connection.isConnected();
    }

    public async sendRaw(command: string): Promise<CommandResponse> {
        const raw = await this.sendWithReconnect(command);
        return discriminateResponse(raw);
    }

    public async create(db: string, schema: SchemaField[]): Promise<number> {
        const response = await this.sendRaw(buildCreate(db, schema));
        return assertCode(response);
    }

    public async add(db: string, key: "*" | number, values: CommandValue[]): Promise<number> {
        const response = await this.sendRaw(buildAdd(db, key, values));
        return assertCode(response);
    }

    public async up(db: string, key: number, values: UpdateValue[]): Promise<number> {
        const response = await this.sendRaw(buildUp(db, key, values));
        return assertCode(response);
    }

    public async get(
        db: string,
        key: number,
        opts?: { fields?: string[] },
    ): Promise<TypedTable<Record<string, string>>>;
    public async get<S extends readonly SchemaField[]>(
        db: string,
        key: number,
        opts: { fields?: string[]; schema: S },
    ): Promise<TypedTable<SchemaToRow<S>>>;
    public async get<S extends readonly SchemaField[]>(
        db: string,
        key: number,
        opts?: { fields?: string[]; schema?: S },
    ): Promise<TypedTable<SchemaToRow<S>> | TypedTable<Record<string, string>>> {
        const response = await this.sendRaw(buildGet(db, key, opts?.fields));
        return opts?.schema !== undefined
            ? assertTypedTable(response, opts.schema)
            : assertTypedTable(response);
    }

    public async del(db: string, key: number): Promise<number> {
        const response = await this.sendRaw(buildDel(db, key));
        return assertCode(response);
    }

    public async getAll(
        db: string,
        opts?: { fields?: string[] },
    ): Promise<TypedTable<Record<string, string>>>;
    public async getAll<S extends readonly SchemaField[]>(
        db: string,
        opts: { fields?: string[]; schema: S },
    ): Promise<TypedTable<SchemaToRow<S>>>;
    public async getAll<S extends readonly SchemaField[]>(
        db: string,
        opts?: { fields?: string[]; schema?: S },
    ): Promise<TypedTable<SchemaToRow<S>> | TypedTable<Record<string, string>>> {
        const response = await this.sendRaw(buildGetAll(db, opts?.fields));
        return opts?.schema !== undefined
            ? assertTypedTable(response, opts.schema)
            : assertTypedTable(response);
    }

    public async search(
        db: string,
        field: string,
        value: string | number | boolean,
        opts?: { fields?: string[] },
    ): Promise<TypedTable<Record<string, string>>>;
    public async search<S extends readonly SchemaField[]>(
        db: string,
        field: string,
        value: string | number | boolean,
        opts: { fields?: string[]; schema: S },
    ): Promise<TypedTable<SchemaToRow<S>>>;
    public async search<S extends readonly SchemaField[]>(
        db: string,
        field: string,
        value: string | number | boolean,
        opts?: { fields?: string[]; schema?: S },
    ): Promise<TypedTable<SchemaToRow<S>> | TypedTable<Record<string, string>>> {
        const response = await this.sendRaw(buildSearch(db, field, value, opts?.fields));
        return opts?.schema !== undefined
            ? assertTypedTable(response, opts.schema)
            : assertTypedTable(response);
    }

    public async count(db: string): Promise<number> {
        const response = await this.sendRaw(buildCount(db));
        return assertCode(response);
    }

    public async createIndex(db: string, field: string): Promise<number> {
        const response = await this.sendRaw(buildCreateIndex(db, field));
        return assertCode(response);
    }

    /**
     * Authenticate this connection with a token.
     * Returns the privilege level: 1 = readonly, 2 = admin.
     */
    public async auth(token: string): Promise<number> {
        const response = await this.sendRaw(buildAuth(token));
        return assertCode(response);
    }

    /**
     * Generate a new auth key (admin only).
     * Returns the raw token — shown exactly once.
     */
    public async addKey(name: string, role: KeyRole): Promise<string> {
        const response = await this.sendRaw(buildAddKey(name, role));
        if (response.kind !== "message" && response.kind !== "table") {
            // data response comes back as a message (raw text)
            // fall through to assertCode which will throw
        }
        if (response.kind === "message" && !response.message.startsWith("ERR")) {
            return response.message;
        }
        // The server sends the token as plain data (no table/code format),
        // so it arrives as a raw message response.
        if (response.kind === "message") {
            throw new ServerMessageError(response.message);
        }
        // Numeric code means an error occurred
        assertCode(response);
        // unreachable
        return "";
    }

    /** Remove an auth key by name (admin only). */
    public async delKey(name: string): Promise<number> {
        const response = await this.sendRaw(buildDelKey(name));
        return assertCode(response);
    }

    /** Delete an entire table/database. */
    public async delTable(db: string): Promise<number> {
        const response = await this.sendRaw(buildDelTable(db));
        return assertCode(response);
    }

    /** List all auth keys as a table (admin only). */
    public async listKeys(): Promise<TypedTable<Record<string, string>>> {
        const response = await this.sendRaw(buildListKeys());
        return assertTypedTable(response);
    }

    /** Return the name of the authenticated key for this connection. */
    public async whoami(): Promise<string> {
        const response = await this.sendRaw(buildWhoami());
        if (response.kind === "message" && !response.message.startsWith("ERR")) {
            return response.message;
        }
        if (response.kind === "message") {
            throw new ServerMessageError(response.message);
        }
        assertCode(response); // throws
        return "";
    }

    private async sendWithReconnect(command: string): Promise<string> {
        let attempts = 0;

        while (true) {
            try {
                return await this.connection.send(command, this.options.timeoutMs);
            } catch (error) {
                if (!this.reconnectEnabled || attempts >= this.maxRetries) {
                    if (attempts > 0) {
                        throw new ReconnectExhaustedError();
                    }
                    throw error;
                }

                attempts += 1;
                await this.wait(this.retryDelayMs);
                await this.connection.connect(this.host, this.port);
            }
        }
    }

    private async wait(ms: number): Promise<void> {
        await new Promise((resolve) => setTimeout(resolve, ms));
    }
}
