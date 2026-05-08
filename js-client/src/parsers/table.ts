import { ParsedTable, SchemaField, SchemaToRow, SchemaType, TypedTable } from "../types.js";

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

function parseArrayString(raw: string): string[] {
    const inner = raw.replace(/^\[/, "").replace(/\]$/, "");
    if (inner.length === 0) return [];
    return inner.split(", ").map((v) => {
        // Strip surrounding double-quotes if present (string[] wire format)
        if (v.startsWith('"') && v.endsWith('"')) {
            return v.slice(1, -1);
        }
        return v;
    });
}

export function coerceValue(value: string, type: SchemaType): unknown {
    switch (type) {
        case "int":
        case "double":
            return Number(value);
        case "bool":
            return value === "true";
        case "string":
            return value;
        case "int[]":
        case "double[]":
            return parseArrayString(value).map(Number);
        case "bool[]":
            return parseArrayString(value).map((v) => v === "true");
        case "string[]":
            return parseArrayString(value);
    }
}

export function coerceTable<S extends readonly SchemaField[]>(
    parsed: ParsedTable,
    schema: S,
): TypedTable<SchemaToRow<S>> {
    const schemaMap = new Map<string, SchemaType>(schema.map((f) => [f.name, f.type]));

    const rows = parsed.rows.map((raw) => {
        const row: Record<string, unknown> = {};
        for (const [key, value] of Object.entries(raw)) {
            const type = schemaMap.get(key);
            if (type !== undefined) {
                row[key] = coerceValue(value, type);
            } else if (key === "key" || key === "time_created" || key === "time_updated") {
                row[key] = Number(value);
            } else {
                row[key] = value;
            }
        }
        return row as SchemaToRow<S>;
    });

    return { headers: parsed.headers, rows, raw: parsed.raw };
}
