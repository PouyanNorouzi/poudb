# poudb

A lightweight, standalone database written in C. Runs as its own process and communicates with clients over sockets using a simple custom protocol.

## Features

* Custom in-memory and persistent storage engine
* Text-based client protocol over TCP sockets
* Operations: `CREATE`, `ADD`, `UP`, `GET`, `DEL`, `GET_ALL`, `SEARCH`, `COUNT`, `CREATE_INDEX`
* Typed homogeneous arrays: `int[]`, `double[]`, `bool[]`, `string[]` (up to 1024 elements)
* Token-based authentication with admin and readonly roles
* Minimal dependencies and built-in memory management
* Scalable build system using GNU Make
* Clean modular architecture for future extensibility

## Command Examples

### CREATE
Create a new database with a defined schema. The key is automatically managed.

Supported field types: `int`, `double`, `bool`, `string`, `int[]`, `double[]`, `bool[]`, `string[]`.
```
CREATE users (string name, int age, bool active)
CREATE products (string name, double[] prices, string[] tags, bool active)
```

### ADD
Add a new record to a database. Use `*` for auto-generated key.

Array values are written as comma-separated lists in square brackets. An empty array is `[]`.
```
ADD users * ("Alice", 30, true)
ADD users 42 ("Bob", 25, false)
ADD products * ("Widget", [9.99, 14.99], ["sale", "featured"], true)
ADD products * ("Plain", [], [], false)
```

### UP
Update an existing record. Use `_` to skip updating a field.

For array fields, provide a new literal to replace the array, or use `[...+value]` to append a single element to the existing array.
```
UP users 42 (_, 26, true)
UP products 1 (_, [5.99, 9.99], _, _)
UP products 1 (_, [...+19.99], [...+"clearance"], _)
```

### GET
Retrieve a specific record by key. Optionally specify fields to return.
```
GET users 42
GET users 42 (name, age)
```

### DEL
Delete a record by key.
```
DEL users 42
```

### GET_ALL
Retrieve all records from a database. Optionally specify fields to return.
```
GET_ALL users
GET_ALL users (name, age)
```

### SEARCH
Search for records where a field matches a specific value.
```
SEARCH users age 30
SEARCH users name "Alice" (name, age)
```

### COUNT
Get the total number of records in a database.
```
COUNT users
```

### CREATE_INDEX
Create an index on a field for faster queries.
```
CREATE_INDEX users name
```

## Authentication

All connections must authenticate before issuing any command. Authentication is token-based with two privilege levels:

| Role | Permitted operations |
|------|---------------------|
| `admin` | All operations including key management |
| `readonly` | `GET`, `GET_ALL`, `SEARCH`, `COUNT` |

### First run

On first startup, when no keys exist, the server automatically generates an admin token and prints it to stdout:

```
[AUTH] First-run admin key: 3f2a1b...
[AUTH] Store it securely - it will not be shown again.
```

Save this token — it is shown exactly once.

### AUTH
Authenticate the current connection. Must be sent before any other command.
Returns `1` for readonly or `2` for admin on success, or an error message on failure.
```
AUTH <token>
```

### ADD_KEY
Generate a new token and store it under a name (admin only).
The raw token is returned exactly once in the response — save it immediately.
```
ADD_KEY alice readonly
ADD_KEY deploy admin
```

### DEL_KEY
Revoke a key by name (admin only). Existing connections already authenticated with this token are not affected until they reconnect.
```
DEL_KEY alice
```

### LIST_KEYS
List all key names and their roles (admin only). Tokens are never shown.
```
LIST_KEYS
```

### Dependency

Authentication uses [libsodium](https://libsodium.org) for token generation (`randombytes_buf`) and Argon2id password hashing (`crypto_pwhash_str`).

```bash
# Debian/Ubuntu
sudo apt install libsodium-dev

# Arch
sudo pacman -S libsodium
```

Key hashes are stored in the snapshot file (format version 2). Snapshots from version 1 (pre-auth) load cleanly but will trigger a first-run key generation on next startup.

```
poudb/
├── src/        # Source files
├── include/    # Header files
├── build/      # Object files (ignored by git)
├── bin/        # Compiled binary (ignored by git)
├── Makefile    # Build instructions
├── .gitignore  # Git exclusions
└── README.md   # This file
```

## Build Instructions

Make sure you have `gcc` and `make` installed. Then run:

```bash
make
```

The compiled binary will be located in the `bin/` folder.

## Install

The project provides script-based installation with Make wrappers.

Install using defaults:

```bash
sudo make install
```

Install with a different prefix:

```bash
sudo make install PREFIX=/usr
```

Stage files for packaging:

```bash
make install DESTDIR=/tmp/poudb-stage
```

Direct script usage is also supported:

```bash
sudo PREFIX=/usr/local RUN_USER=poudb RUN_GROUP=poudb SERVICE_NAME=poudb.service ./scripts/install.sh
```

Uninstall:

```bash
sudo make uninstall
```

Notes:

* Binary path: `/usr/local/bin/poudb` by default
* Config path: `/etc/poudb/poudb.conf`
* Data path: `/var/lib/poudb`
* Systemd unit path: `/etc/systemd/system/poudb.service`
* `make install` enables the service on boot when run as root without `DESTDIR`
* Runtime config is still explicit; pass `--config /etc/poudb/poudb.conf` when starting the server.

Manage the service:

```bash
sudo systemctl status poudb
sudo systemctl start poudb
sudo systemctl restart poudb
sudo systemctl disable --now poudb
```

## Running the Server

```bash
./bin/poudb
```

### Runtime Options

`poudb` supports built-in defaults, optional INI config files, and command-line overrides.

Precedence is:

1. CLI options
2. Config file values
3. Built-in defaults

```bash
./bin/poudb --help
./bin/poudb --port 3010 --snapshot /tmp/poudb.snapshot
./bin/poudb --config ./poudb.conf --autosave off
```

Supported options:

* `-h, --help`
* `-f, --config PATH`
* `-p, --port PORT`
* `-s, --snapshot PATH`
* `-i, --autosave-interval MS`
* `-m, --max-connections N`
* `-a, --autosave on|off`

### Config File

Install dependency:

```bash
# Debian/Ubuntu
sudo apt install libinih-dev

# Arch
sudo pacman -S libinih
```

Example `poudb.conf`:

```ini
[server]
port=3005
max_connections=5

[storage]
snapshot_path=/var/lib/poudb/poudb.snapshot

[autosave]
enabled=true
interval_ms=30000
```
