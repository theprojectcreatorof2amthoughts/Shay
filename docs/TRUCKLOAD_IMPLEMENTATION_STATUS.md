# Truckload implementation status

The Truck 2 and Truck 3 roadmap is being delivered as small, tested milestones.

## Truck 2: language/runtime

- [x] List literals, zero-based indexing, append, replacement, removal, and bounds diagnostics.
- [x] Map literals with string keys, key lookup, update, and removal.
- [x] Set literals with duplicate elimination, add, remove, and membership checks.
- [x] Optional scalar type annotations for `number`, `string`, and `boolean` with initializer validation.
- [x] Structured `throwit/>` errors and nested `tryit` / `catchit` handling.
- [x] File read, write, append, delete, existence checks, and directory create/existence APIs.
- [ ] List/map/set iteration syntax and typed collection elements.
- [ ] Typed function parameters, assignment checking, and richer inference.
- [x] Safe process spawning with explicit argument arrays and exit-code reporting.
- [ ] JSON parsing and serialization.
- [ ] HTTP/HTTPS APIs.
- [ ] Concurrency and synchronization.

## Truck 3: .shui / .shuil

- [x] Initial `.shuil` platform-configuration parser and `shui validate-library` command (configuration validation only; translator execution is not implemented).

- [x] Hybrid, backend-independent UI-tree specification.
- [x] Parser, typed UI tree, and validation.
- [x] Browser HTML/CSS renderer with action/input event hooks.
- [x] Native Windows renderer using Win32 controls.
- [ ] Execute the `logic` block.
- [ ] Connect browser and Windows events to shared Shay logic and state.
- [ ] Cross-renderer conformance tests for interactive behavior.
- [ ] Packaging and project-build integration.

## Current implementation boundary

The browser renderer emits semantic HTML/CSS and dispatches `shay:action` / `shay:input` events. The Windows renderer creates native controls and currently reports button actions to the debugger. Neither renderer executes the `.shui` `logic` block or synchronizes bound state yet.

File APIs use literal paths and content and never invoke a shell. Filesystem failures are categorized as `io` errors and can be handled by `tryit` / `catchit`. `runproc/>` launches a program directly with a separate argv list (no shell concatenation) and reports its exit code.

Only checked items above are implemented and merged into `main`.
