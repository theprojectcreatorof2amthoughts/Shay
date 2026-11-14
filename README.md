# Shay

Shay is an independent hybrid programming language with a C11 runtime and a C++20 compiler toolchain. It provides two separate command-line programs:

- **`gshai`** — the Shay compiler. It creates native executables or relocatable `.sho` object files.
- **`shaii`** — the Shay interpreter. It runs Shay source files and provides syntax-check mode.

Shay has its own original grammar and vocabulary, including `brew` for variable declarations and `instxt-"..."` for text output.

## File formats

| Extension | Meaning |
|---|---|
| `.shay` | Shay program |
| `.shaii` | Shay library |
| `.shaz` | Zeccur task file |
| `.sho` | Compiled Shay object file (native relocatable object) |

## Vocabulary
| Vocabulary | Meaning |
|---|---|
| `Zeccur` | A non variable code file |
| `Shay` | Arabic word for tea (شاي) |

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

## Siil standard library

The repository includes a standard-library directory named `siil/`. `$shaiiport` checks beside the importing file first, then checks `SHAY_STDLIB` if set, followed by `siil/` in the current working directory and ancestor directories. Set `SHAY_STDLIB` to a library directory (for example, `/usr/local/share/shay/siil`) when using an installed copy. `make install` installs the bundled libraries under `/usr/local/share/shay/siil` by default.

Bundled modules include math, units, ASCII codes, text fragments, symbols, geometry, physics, time, booleans, data delimiters, messages, and numeric constants. They currently expose prefixed `brew` constants; callable library functions are not part of the implemented grammar yet. See [`siil/README.md`](siil/README.md) for the complete list.

```shay
$shaiiport "math.shaii"
$shaiiport "text.shaii"
insval-math_pi
insval-text_space
```

## Initial syntax

```shay
instxt-"Hello, world!"
brew username = "Alex"
insval-username
```

The current implementation supports string output, literal variable declarations, variable output, imports, duplicate-import protection, circular-import detection, and source-located diagnostics. This is an early implementation stage; the full language feature set is still being developed.

## Build

Requirements: a C11 compiler, a C++20 compiler, GNU Make or CMake 3.20+.

```sh
make
```

This builds both tools:

```sh
./build/gshai --version
./build/shaii --version
```

## Interpret Shay source

```sh
./build/shaii examples/hello.shay
./build/shaii --check examples/hello.shay
```

## Compile Shay source

### Create a standalone executable with `-o`

```sh
./build/gshai -o examples/hello.shay
# Produces examples/hello (a runnable executable)
./examples/hello
```

You can choose the executable path:

```sh
./build/gshai -o bin/my-program examples/hello.shay
```

### Create a native object file with `-ob`

```sh
./build/gshai -ob examples/hello.shay
# Produces examples/hello.sho
```

You can also choose an explicit `.sho` output path:

```sh
./build/gshai -ob build/hello-object.sho examples/hello.shay
```

The `.sho` file is a native relocatable object. It exports `shay_program_entry` and includes the minimal runtime write function used by the currently supported language subset, so it can be linked with a host entry point.

## Make targets

- `make` — build compiler `gshai` and interpreter `shaii`.
- `make run PROGRAM=examples/hello.shay` — interpret a source file.
- `make check PROGRAM=examples/hello.shay` — syntax-check a source file.
- `make compile PROGRAM=examples/hello.shay` — emit a `.sho` object.
- `make executable PROGRAM=examples/hello.shay` — emit a standalone executable.
- `make test` — run interpreter and compiler regression tests.

## Architecture

- `src/gshai.cpp`: compiler driver and native code generation.
- `src/shaii.cpp`: interpreter and source execution.
- `src/lexer.cpp`, `src/parser.cpp`: shared frontend.
- `src/runtime.c` / `src/runtime.h`: interpreter runtime primitives.
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
