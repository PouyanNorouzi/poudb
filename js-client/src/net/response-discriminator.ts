import { ProtocolParseError } from "../errors.js";
import { parseTable } from "../parsers/table.js";
import { CodeResponse, CommandResponse, MessageResponse, TableResponse } from "../types.js";

function isNumericCode(text: string): boolean {
    return /^-?\d+$/.test(text);
}

function isTableLike(text: string): boolean {
    return text.startsWith("|") || text === "(No rows)";
}

export function discriminateResponse(raw: string): CommandResponse {
    const trimmed = raw.trim();
    if (trimmed.length === 0) {
        throw new ProtocolParseError("Empty response from server");
    }

    if (isNumericCode(trimmed)) {
        const response: CodeResponse = {
            kind: "code",
            raw,
            code: Number.parseInt(trimmed, 10),
        };
        return response;
    }

    if (isTableLike(trimmed)) {
        const response: TableResponse = {
            kind: "table",
            raw,
            parsed: parseTable(raw),
        };
        return response;
    }

    const response: MessageResponse = {
        kind: "message",
        raw,
        message: trimmed,
    };

    return response;
}
