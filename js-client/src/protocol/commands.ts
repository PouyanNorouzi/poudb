import { ArrayAppend, CommandValue, KeyRole, SchemaField, UpdateValue } from "../types.js";
import { formatArray, formatArrayAppend, formatScalar, joinValues } from "./escaping.js";

function fieldsSegment(fields?: string[]): string {
    if (!fields || fields.length === 0) {
        return "";
    }

    return ` (${fields.join(", ")})`;
}

export function buildCreate(db: string, schema: SchemaField[]): string {
    const fieldDefs = schema.map((field) => `${field.type} ${field.name}`).join(", ");
    return `CREATE ${db} (${fieldDefs})`;
}

export function buildAdd(db: string, key: "*" | number, values: CommandValue[]): string {
    const keyPart = key === "*" ? "*" : String(key);
    return `ADD ${db} ${keyPart} (${joinValues(values)})`;
}

function formatUpdateValue(value: UpdateValue): string {
    if (value === "_") return "_";
    if (Array.isArray(value)) return formatArray(value);
    if (typeof value === "object" && "arrayAppend" in (value as object)) {
        return formatArrayAppend((value as ArrayAppend).arrayAppend);
    }
    return formatScalar(value as string | number | boolean);
}

export function buildUp(db: string, key: number, values: UpdateValue[]): string {
    const valuePart = values.map(formatUpdateValue).join(", ");
    return `UP ${db} ${key} (${valuePart})`;
}

export function buildGet(db: string, key: number, fields?: string[]): string {
    return `GET ${db} ${key}${fieldsSegment(fields)}`;
}

export function buildDel(db: string, key: number): string {
    return `DEL ${db} ${key}`;
}

export function buildGetAll(db: string, fields?: string[]): string {
    return `GET_ALL ${db}${fieldsSegment(fields)}`;
}

export function buildSearch(
    db: string,
    field: string,
    value: string | number | boolean,
    returnFields?: string[],
): string {
    return `SEARCH ${db} ${field} ${formatScalar(value)}${fieldsSegment(returnFields)}`;
}

export function buildCount(db: string): string {
    return `COUNT ${db}`;
}

export function buildCreateIndex(db: string, field: string): string {
    return `CREATE_INDEX ${db} ${field}`;
}

export function buildAuth(token: string): string {
    return `AUTH ${token}`;
}

export function buildAddKey(name: string, role: KeyRole): string {
    return `ADD_KEY ${name} ${role}`;
}

export function buildDelKey(name: string): string {
    return `DEL_KEY ${name}`;
}

export function buildDelTable(db: string): string {
    return `DEL_TABLE ${db}`;
}

export function buildListKeys(): string {
    return `LIST_KEYS`;
}

export function buildWhoami(): string {
    return `WHOAMI`;
}
