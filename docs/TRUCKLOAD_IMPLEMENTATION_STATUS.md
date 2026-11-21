# Truckload implementation status

The Truck 2 and Truck 3 roadmap is being delivered as small, tested milestones.

## Truck 2: language/runtime

- [x] List literals in `brew`: strings, numbers, booleans, negative numbers, and empty lists.
- [x] List output in both `shaii` and `gshai`-generated executables.
- [x] Zero-based list indexing, append, replacement, removal, and bounds diagnostics.
- [ ] List iteration syntax.
- [ ] Maps and sets.
- [ ] Optional static typing.
- [ ] Structured error values and propagation.
- [ ] File and process APIs.
- [ ] JSON parsing and serialization.
- [ ] HTTP/HTTPS APIs.
- [ ] Concurrency and synchronization.

## Truck 3: .shui

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

Only checked items above are implemented and merged into `main`.