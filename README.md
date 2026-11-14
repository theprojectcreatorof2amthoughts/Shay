# Shay

Shay (شاي, Arabic for tea) is an independent hybrid programming language implemented with a C11 runtime and a C++20 toolchain. It has two separate command-line programs:

- **`gshai`** — the Shay interpreter. It runs source files and provides syntax-check mode.
- **`gshaic`** — the Shay compiler. It compiles source into native relocatable object files.

Shay has its own original grammar and vocabulary, including `brew` for variable declarations and `instxt-"..."` for text output.

## File formats

| Extension | Meaning |
|---|---|
| `.shay` | Shay program |
| `.shaii` | Shay library |
| `.shaz` | Zeccurs task file |
| `.sho` | Compiled Shay object file (native relocatable object) |

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

The bootstrap currently supports string output, literal variable declarations, variable output, import checks, duplicate-import protection, circular-import detection, and source-located diagnostics. This is the first implementation stage; the full language feature set is still being developed.

## Build

Requirements: a C11 compiler, a C++20 compiler, GNU Make or CMake 3.20+.

```sh
make
```

This builds both command-line tools:

```sh
./build/gshai --version
./build/gshaic --version
```

### Interpret Shay source

```sh
./build/gshai examples/hello.shay
./build/gshai --check examples/hello.shay
```

### Compile Shay source to an object file

`-ob` asks the compiler to produce a native relocatable object file. The output extension is `.sho`, not `.o`:

```sh
./build/gshaic -ob examples/hello.shay
# Produces examples/hello.sho
```

The object exports `shay_program_entry` and uses the Shay runtime. It can be linked with the runtime and a small host entry point:

```cpp
extern "C" int shay_program_entry();
int main() { return shay_program_entry(); }
```

For example, compile the host entry point and link it with `examples/hello.sho` and the runtime library/object.

### CMake

```sh
cmake -S . -B build-cmake
cmake --build build-cmake
ctest --test-dir build-cmake --output-on-failure
```

## Make targets

- `make` — build both `gshai` and `gshaic`.
- `make run PROGRAM=examples/hello.shay` — interpret a source file.
- `make check PROGRAM=examples/hello.shay` — syntax-check a source file.
- `make compile PROGRAM=examples/hello.shay` — emit a `.sho` object.
- `make test` — run interpreter and compiler regression tests.

## Architecture

- `src/gshai.cpp`: interpreter and source execution.
- `src/gshaic.cpp`: separate compiler and `.sho` object generation.
- `src/lexer.cpp`, `src/parser.cpp`: shared frontend.
- `src/runtime.c` / `src/runtime.h`: C11 runtime primitives.
- `examples/`: runnable Shay examples.

## Development roadmap

The long-term goal is to grow Shay into a complete, practical, general-purpose programming language while preserving its own syntax and file formats:

- Full lexer, parser, AST, and structured diagnostics.
- Interactive REPL and persistent execution environment.
- Functions, recursion, lexical scopes, closures, and control flow.
- Numeric, string, boolean, collection, and memory values.
- Checked memory/buffer operations and expanded math/string/geometry built-ins.
- Embedded and external library loading, plus Zeccurs task execution.
- Native executable generation, object linking, optimization levels, warning controls, and check-only mode.
- C/C++ host-function bridge and embedding API.
- Make/CMake builds and automated regression/diagnostic tests.

These are target capabilities, not claims that the bootstrap currently implements them.
