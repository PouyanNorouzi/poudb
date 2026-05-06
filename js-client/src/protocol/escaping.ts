import { ArrayValue, CommandValue, ScalarValue } from "../types.js";

export function escapeString(value: string): string {
    return value
        .replace(/\\/g, "\\\\")
        .replace(/\n/g, "\\n")
        .replace(/\r/g, "\\r")
        .replace(/\t/g, "\\t")
        .replace(/\"/g, "\\\"");
}

export function formatScalar(value: ScalarValue): string {
    if (typeof value === "string") {
        return `"${escapeString(value)}"`;
    }
    if (typeof value === "boolean") {
        return value ? "true" : "false";
    }
    return Number.isFinite(value) ? String(value) : "0";
}

/** @deprecated Use formatScalar for scalar values. */
export function formatValue(value: CommandValue): string {
    if (Array.isArray(value)) {
        return formatArray(value);
    }
    return formatScalar(value);
}

export function formatArray(values: ArrayValue): string {
    return `[${values.map(formatScalar).join(", ")}]`;
}

export function formatArrayAppend(value: ScalarValue): string {
    return `[...+${formatScalar(value)}]`;
}

export function joinValues(values: CommandValue[]): string {
    return values.map((value) => formatValue(value)).join(", ");
}
