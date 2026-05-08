import { coerceTable } from "./parsers/table.js";
import { ServerMessageError } from "./errors.js";
import { CommandResponse, ParsedTable, QueryResult, SchemaField, SchemaToRow, TypedTable } from "./types.js";

export function assertCode(response: CommandResponse): number {
    if (response.kind === "message") {
        throw new ServerMessageError(response.message);
    }

    if (response.kind !== "code") {
        throw new Error("Expected code response");
    }

    return response.code;
}

export function assertTable(response: CommandResponse): QueryResult<ParsedTable> {
    if (response.kind === "message") {
        throw new ServerMessageError(response.message);
    }

    if (response.kind !== "table") {
        throw new Error("Expected table response");
    }

    return {
        raw: response.raw,
        data: response.parsed,
    };
}

export function assertTypedTable<S extends readonly SchemaField[]>(
    response: CommandResponse,
    schema: S,
): TypedTable<SchemaToRow<S>>;
export function assertTypedTable(response: CommandResponse): TypedTable<Record<string, string>>;
export function assertTypedTable<S extends readonly SchemaField[]>(
    response: CommandResponse,
    schema?: S,
): TypedTable<SchemaToRow<S>> | TypedTable<Record<string, string>> {
    if (response.kind === "message") {
        throw new ServerMessageError(response.message);
    }

    if (response.kind !== "table") {
        throw new Error("Expected table response");
    }

    if (schema !== undefined) {
        return coerceTable(response.parsed, schema);
    }

    return {
        headers: response.parsed.headers,
        rows: response.parsed.rows,
        raw: response.raw,
    };
}
