import { PoudbClient } from "../src";

async function main(): Promise<void> {
    const client = new PoudbClient();
    await client.connect();

    const db = "example_users";
    await client.create(db, [
        { type: "int", name: "age" },
        { type: "string", name: "name" },
    ]);

    const key = await client.add(db, "*", [27, "Ava"]);
    const all = await client.getAll(db);

    console.log("Inserted key:", key);
    console.log("Rows:", all.data.rows);

    await client.disconnect();
}

main().catch((error) => {
    console.error(error);
    process.exit(1);
});
