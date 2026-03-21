import { ParsedTable } from "../types.js";

function splitCells(line: string): string[] {
    const trimmed = line.trim();
    if (!trimmed.startsWith("|") || !trimmed.endsWith("|")) {
        return [];
    }

    return trimmed
        .split("|")
        .slice(1, -1)
        .map((cell) => cell.trim());
}

export function parseTable(raw: string): ParsedTable {
    const trimmed = raw.trim();
    if (trimmed === "(No rows)") {
        return { headers: [], rows: [], raw };
    }

    const lines = raw
        .split(/\r?\n/)
        .map((line) => line.trim())
        .filter((line) => line.length > 0);

    if (lines.length < 2) {
        throw new Error("Invalid table response: missing header or separator");
    }

    const headers = splitCells(lines[0]);
    if (headers.length === 0) {
        throw new Error("Invalid table response: malformed header");
    }

    const rows: Array<Record<string, string>> = [];
    for (let i = 2; i < lines.length; i += 1) {
        const cells = splitCells(lines[i]);
        if (cells.length === 0) {
            continue;
        }

        const row: Record<string, string> = {};
        for (let h = 0; h < headers.length; h += 1) {
            row[headers[h]] = cells[h] ?? "";
        }
        rows.push(row);
    }

    return { headers, rows, raw };
}
