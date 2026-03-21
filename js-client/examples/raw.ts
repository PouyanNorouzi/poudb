import { PoudbClient } from "../src";

async function main(): Promise<void> {
    const client = new PoudbClient();
    await client.connect();

    const response = await client.sendRaw("COUNT example_users");
    console.log(response);

    await client.disconnect();
}

main().catch((error) => {
    console.error(error);
    process.exit(1);
});
