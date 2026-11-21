# Truck 2 — Language and Runtime Expansion

This document defines an implementation plan for expanding Shay's language and runtime. It does not claim that the features below are already implemented.

## Compatibility rules

- Existing valid `.shay`, `.shaii`, and `.shaz` programs must keep their current behavior.
- The compiler (`gshai`) and interpreter (`shaii`) must share parsing, validation, and runtime semantics wherever practical.
- New syntax must have diagnostics with source file, line, and column.
- Every feature needs interpreter tests, compiler tests, and regression coverage before it is marked complete.
- No feature should require users to install a heavyweight runtime just to run a basic Shay program.

## Feature groups

### 1. Collections

Introduce a small, predictable set of built-in collection types:

- Lists: ordered, resizable sequences.
- Maps: key/value collections with deterministic key/value semantics.
- Sets: unique values (later milestone if list/map implementation is stable).

Required operations: creation, indexing, insertion, removal, length, iteration, and safe bounds/key errors. Mutation semantics must be identical in `shaii` and `gshai`.

Avoid implicit conversion between unrelated collection types.

### 2. Optional static typing

Typing is opt-in. Existing dynamic declarations remain valid.

Goals:
- Type annotations for variables and function return values.
- Primitive types first: `number`, `string`, `boolean`.
- Collection types after generic type syntax is designed.
- Compile-time checks for annotated code.
- Clear runtime checks at dynamic/typed boundaries.

Do not make all Shay programs require declarations or annotations.

### 3. Structured error handling

Add structured error values and predictable propagation rather than relying only on process termination or function-specific failure markers.

Requirements:
- Errors carry a stable category, message, and source location.
- Functions can return or propagate errors.
- A caller can handle an error and continue safely.
- Unhandled errors produce concise diagnostics and a non-zero exit status.
- Existing `!#function` behavior remains compatible during migration.

Exact syntax is intentionally undecided until the current function/return model is reviewed.

### 4. File and process APIs

Provide a portable standard library for:
- Reading and writing text files.
- Checking existence and enumerating directories.
- Creating directories.
- Running a child process with explicit argument arrays.
- Capturing exit status, stdout, and stderr.

Security rules:
- Never build shell commands by concatenating untrusted strings.
- Keep shell execution explicit and separate from safe argument-based process launching.
- Return structured errors for permission, missing-path, and process failures.

### 5. JSON and networking

JSON comes before networking because it is useful for configuration, APIs, and project metadata.

JSON requirements:
- Parse and serialize null, booleans, numbers, strings, arrays, and objects.
- Preserve Unicode and escape strings correctly.
- Report malformed input with a useful offset/source location.
- Avoid silently converting invalid JSON values.

Networking requirements:
- HTTP client with explicit methods, headers, timeouts, and response status.
- HTTPS certificate validation enabled by default.
- No hidden network calls.
- Clear timeout, DNS, TLS, and HTTP error categories.
- Tests use local mock servers; CI must not depend on public internet availability.

### 6. Concurrency

Concurrency is a later milestone, after value ownership and error semantics are settled.

Design constraints:
- Explicit task/thread creation and joining.
- Defined ownership rules for values crossing task boundaries.
- No shared mutable state without synchronization.
- Deterministic cancellation and shutdown behavior.
- A small, documented synchronization API.

## Suggested delivery order

1. Collections and value semantics.
2. Optional primitive typing and static validation.
3. Structured errors.
4. File APIs and safe process execution.
5. JSON.
6. HTTP/HTTPS client.
7. Concurrency.

## Definition of done

A feature is complete only when:
- Its syntax and semantics are documented.
- Both interpreter and compiler support it, or the docs clearly label a temporary limitation.
- Tests cover valid use, invalid use, edge cases, and error behavior.
- Existing regression tests pass.
- The implementation has no platform-specific behavior leaking into shared language semantics.
