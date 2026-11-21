# Truckload implementation status

This branch is the implementation track for Truck 2 (language/runtime) and Truck 3 (.shui).

## Truck 2: collections

- [x] Parse list literals in `brew`: strings, numbers, booleans, negative numbers, and empty lists.
- [x] Store and print lists in both `shaii` and `gshai`-generated executables.
- [x] Replace list values when a variable is redeclared as a scalar.
- [ ] Add list indexing, mutation, iteration, and bounds diagnostics.
- [ ] Add maps and sets.
- [ ] Add optional static typing and structured errors.

## Truck 3: .shui

- [x] Record the hybrid, backend-independent design in `docs/SHUI_SPEC.md`.
- [ ] Add a parser and validated UI tree.
- [ ] Add browser rendering.
- [ ] Add Windows desktop rendering.
- [ ] Connect UI events and state to Shay logic.

The checked items above describe repository implementation or design documentation only; unchecked items are not implemented yet.
