# Project Guidelines

## Code Style
- Follow existing C style in this repository: 4-space indentation, K&R-style braces, and minimal but meaningful comments.
- Keep public declarations in `include/` and implementations in matching paths under `src/`.
- Prefer explicit error checks and early returns for system calls and allocation failures.
- Preserve ownership conventions for heap data (for example, command/result lifecycle through `free_command`, `free_command_result`, and DB cleanup APIs).

## Architecture
- Runtime flow is: socket input in `src/net.c` -> parse in `src/db/parser.c` -> execute in `src/db/operations.c` -> send response in `src/net.c`.
- Event loop and lifecycle live in `src/main.c` with epoll via `src/connection_manager.c`.
- DB storage internals are in `src/db/data_model.c`; row storage is hashmap-backed and iteration order is intentionally unordered.
- Persistence is binary snapshot-based in `src/db/persistence.c`, with atomic replace (`*.tmp` then rename) and default file `poudb.snapshot`.

## Build And Test
- Build: `make` (or `make all`).
- Run server: `make run_server`.
- Build and run test client: `make run_client` (or `make test_client` then `./bin/client`).
- Run tests: `make test` (or `make test_parser`, `make test_operations`).
- Memory checks: `make valgrind`.
- Tests require Criterion (`-lcriterion` in `Makefile`).

## Conventions
- Keep Linux-specific networking assumptions (epoll, `sys/epoll.h`) unless the task explicitly asks for portability changes.
- Respect protocol/operation contracts across parser and executor. If parser output changes, update operation handling and tests together.
- For ADD/UP paths, preserve current string ownership transfer behavior between parsed command data and DB storage.
- Do not assume stable row iteration order in output or tests; use key-based assertions where possible.
- Keep snapshot compatibility in mind when changing persistence encoding or DB schema serialization.

## Pitfalls
- Server capacity is limited by `MAX_CONNECTIONS` in `include/net.h`; avoid silent behavior changes around admission/rejection.
- Default port is defined in `include/main.h`.
- Snapshot path is relative to current working directory unless explicitly overridden in persistence calls.
- Avoid editing generated artifacts under `build/` and `bin/` unless the user asks.

## References
- Command syntax and high-level usage: `readme.md`.
- Canonical build/test/run targets and linker requirements: `Makefile`.
- Parser behavior examples: `test/test_parser.c`.
- Operation semantics and formatting expectations: `test/test_operations.c`.