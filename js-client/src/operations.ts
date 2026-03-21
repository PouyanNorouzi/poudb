import { ServerMessageError } from "./errors";
import { CommandResponse, ParsedTable, QueryResult } from "./types";

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
