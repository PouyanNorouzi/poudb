export type ScalarSchemaType = "int" | "double" | "bool" | "string";
export type ArraySchemaType = "int[]" | "double[]" | "bool[]" | "string[]";
export type SchemaType = ScalarSchemaType | ArraySchemaType;
export type KeyRole = "admin" | "readonly";

export interface SchemaField {
    name: string;
    type: SchemaType;
}

/** Scalar primitive usable as a field value or array element. */
export type ScalarValue = string | number | boolean;
/** A full array value for an array-typed field. */
export type ArrayValue = ScalarValue[];
/** Union of all values that can be sent in ADD commands. */
export type CommandValue = ScalarValue | ArrayValue;
/** Append a single element to a stored array field. */
export interface ArrayAppend {
    arrayAppend: ScalarValue;
}
/** Values allowed in UP commands: scalar, array, ignore sentinel, or append. */
export type UpdateValue = CommandValue | "_" | ArrayAppend;

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
