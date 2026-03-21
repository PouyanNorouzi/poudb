import { CommandValue, SchemaField, UpdateValue } from "../types";
import { formatValue, joinValues } from "./escaping";

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

export function buildUp(db: string, key: number, values: UpdateValue[]): string {
    const valuePart = values
        .map((value) => (value === "_" ? "_" : formatValue(value)))
        .join(", ");
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
    value: CommandValue,
    returnFields?: string[],
): string {
    return `SEARCH ${db} ${field} ${formatValue(value)}${fieldsSegment(returnFields)}`;
}

export function buildCount(db: string): string {
    return `COUNT ${db}`;
}

export function buildCreateIndex(db: string, field: string): string {
    return `CREATE_INDEX ${db} ${field}`;
}
