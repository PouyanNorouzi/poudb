# poudb

A lightweight, standalone database written in C. Runs as its own process and communicates with clients over sockets using a simple custom protocol.

## Features

* Custom in-memory and persistent storage engine
* Text-based client protocol over TCP sockets
* Basic operations: `SET`, `GET`, `DELETE`
* Minimal dependencies and built-in memory management
* Scalable build system using GNU Make
* Clean modular architecture for future extensibility

## Project Structure

```
custom-db/
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

More options (e.g., port, config file) will be added in later versions.
