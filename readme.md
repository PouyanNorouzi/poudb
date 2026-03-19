# poudb

A lightweight, standalone database written in C. Runs as its own process and communicates with clients over sockets using a simple custom protocol.

## Features

* Custom in-memory and persistent storage engine
* Text-based client protocol over TCP sockets
* Basic operations: `SET`, `GET`, `DELETE`
* Minimal dependencies and built-in memory management
* Scalable build system using GNU Make
* Clean modular architecture for future extensibility

## Command Examples

### CREATE
Create a new database with a defined schema. The key is automatically managed.
```
CREATE users (string name, int age, bool active)
```

### ADD
Add a new record to a database. Use `*` for auto-generated key.
```
ADD users * ("Alice", 30, true)
ADD users 42 ("Bob", 25, false)
```

### UP
Update an existing record. Use `_` to skip updating a field.
```
UP users 42 (_, 26, true)
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

## Project Structure

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
snapshot_path=poudb.snapshot

[autosave]
enabled=true
interval_ms=30000
```
