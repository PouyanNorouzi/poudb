export type SchemaType = "int" | "double" | "bool" | "string";
export type KeyRole = "admin" | "readonly";

export interface SchemaField {
    name: string;
    type: SchemaType;
}

export type CommandValue = string | number | boolean;
export type UpdateValue = CommandValue | "_";

export interface ParsedTable {
    headers: string[];
    rows: Array<Record<string, string>>;
    raw: string;
}

export interface CodeResponse {
    kind: "code";
    raw: string;
    code: number;
}

export interface TableResponse {
    kind: "table";
    raw: string;
    parsed: ParsedTable;
}

export interface MessageResponse {
    kind: "message";
    raw: string;
    message: string;
}

export type CommandResponse = CodeResponse | TableResponse | MessageResponse;

export interface ReconnectOptions {
    enabled?: boolean;
    maxRetries?: number;
    retryDelayMs?: number;
}

export interface ClientOptions {
    host?: string;
    port?: number;
    timeoutMs?: number;
    settleTimeMs?: number;
    reconnect?: ReconnectOptions;
    /** If set, automatically authenticates after connecting. */
    key?: string;
}

export interface QueryResult<T = unknown> {
    raw: string;
    data: T;
}
