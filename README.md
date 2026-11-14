# Shay

Shay (شاي, Arabic for tea) is an independent hybrid programming language implemented with a C11 runtime and a C++20 compiler/toolchain. Its compiler and command-line driver are named **gshai**.

Shay is its own language. It does not reuse Y_ or Y++ syntax. Its source vocabulary is being designed around original constructs such as `brew` for variable declarations and `instxt-"..."` for text output.

## File formats

| Extension | Meaning |
|---|---|
| `.shay` | Shay program |
| `.shaii` | Shay library |
| `.shaz` | Zeccurs task file |

## Imports

```shay
$shaiiport "math.shaii"
$shazport "startup.shaz"
```

| Importing file | `$shaiiport` | `$shazport` |
|---|---:|---:|
| `.shay` | Yes | Yes |
| `.shaz` | Yes | Yes |
| `.shaii` | No | Yes |

Imports are relative to the importing file. Duplicate files are loaded once and circular imports are diagnosed. Zeccurs is task-oriented and variable-free.

## Initial syntax

```shay
instxt-"Hello, world!"
brew username = "Alex"
insval-username
```

The bootstrap currently supports string output, literal variable declarations, variable output, import checks, duplicate-import protection, circular-import detection, and source-located diagnostics. This is the first implementation stage, not yet feature-equivalent to gyy or gy++.

## Build

Requirements: a C11 compiler, a C++20 compiler, GNU Make or CMake 3.20+.

```sh
make
./build/gshai --version
./build/gshai --check examples/hello.shay
./build/gshai examples/hello.shay

cmake -S . -B build-cmake
cmake --build build-cmake
```

## Architecture

- `src/gshai.cpp`: C++20 driver, source validation, import graph, initial evaluator.
- `src/runtime.c` / `src/runtime.h`: C11 runtime primitives.
- `examples/`: runnable Shay examples.

## Roadmap toward gyy / gy++ capability coverage

The target is to bring the useful compiler/runtime capabilities of both sibling projects into Shay while retaining Shay's own syntax and file formats:

- Full lexer, parser, AST, and structured diagnostics.
- Interactive REPL and persistent execution environment.
- Functions, recursion, lexical scopes, closures, and control flow.
- Numeric, string, boolean, collection, and memory values.
- Checked memory/buffer operations and expanded math/string/geometry built-ins.
- Embedded and external library loading, plus Zeccurs task execution.
- Native executable generation, optimization levels, warning controls, and check-only mode.
- C/C++ host-function bridge and embedding API.
- Make/CMake builds and automated regression/diagnostic tests.

These are target capabilities, not claims that the bootstrap currently implements them.
