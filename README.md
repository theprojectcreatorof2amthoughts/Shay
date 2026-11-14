# Shay

**Shay** is an original hybrid programming language implemented with **C11** and **C++20**.

- `.shay` — main Shay source files.
- `.shaii` — Shay libraries.
- `.shaz` — Zeccurs task files, designed to run tasks without variables.

## Syntax preview

```shay
instxt-"Hello World!"

brew greeting = "Hello from Shay"
insval-greeting
```

## Imports

```shay
$shaiiport "math.shaii"
$shazport "startup.shaz"
```

Only `.shay` and `.shaz` files may import `.shaii` files. A `.shaii` file may import `.shaz` files, but not other `.shaii` files. Zeccurs files may import other Zeccurs files.

## Build

Requirements: a C11 compiler, a C++20 compiler, and GNU Make (or CMake).

```sh
make
make test
./shay examples/hello.shay
```

Or use CMake:

```sh
cmake -S . -B build-cmake
cmake --build build-cmake
ctest --test-dir build-cmake --output-on-failure
```

See [the initial syntax notes](docs/SYNTAX.md).

> This is the first executable foundation, not the complete language. The current interpreter implements text output, basic string-valued variables, and the agreed import rules. Functions, control flow, typed values, library exports, and native ahead-of-time compilation remain future compiler milestones.
