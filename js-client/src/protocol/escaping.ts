import { CommandValue } from "../types.js";

export function escapeString(value: string): string {
    return value
        .replace(/\\/g, "\\\\")
        .replace(/\n/g, "\\n")
        .replace(/\r/g, "\\r")
        .replace(/\t/g, "\\t")
        .replace(/\"/g, "\\\"");
}

export function formatValue(value: CommandValue): string {
    if (typeof value === "string") {
        return `\"${escapeString(value)}\"`;
    }

    if (typeof value === "boolean") {
        return value ? "true" : "false";
    }

    return Number.isFinite(value) ? String(value) : "0";
}

export function joinValues(values: CommandValue[]): string {
    return values.map((value) => formatValue(value)).join(", ");
}
