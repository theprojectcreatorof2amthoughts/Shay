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
$shaiiport [math.shaii]          ## implemented library from siil/
$shaiiport "external/custom.shaii" ## external file path on disk
$shazport [startup.shaz]         ## Zeccur task
```

| Importing file | `$shaiiport` | `$shazport` |
|---|---:|---:|
| `.shay` | Yes | Yes |
| `.shaz` | Yes | Yes |
| `.shaii` | No | Yes |

Bracketed `$shaiiport [name.shaii]` imports an implemented library, searched beside the importer and then through `siil/`. Quoted `$shaiiport "path/to/file.shaii"` imports that external file relative to the importing file (or from an absolute path) and does not search `siil/`. `$shazport` continues to use bracket syntax for Zeccur tasks. Duplicate files are loaded once and circular imports are diagnosed. Zeccurs is task-oriented and variable-free.

## Siil standard library

The repository includes a standard-library directory named `siil/`. `$shaiiport` checks beside the importing file first, then checks `SHAY_STDLIB` if set, followed by `siil/` in the current working directory and ancestor directories. Set `SHAY_STDLIB` to a library directory (for example, `/usr/local/share/shay/siil`) when using an installed copy. `make install` installs the bundled libraries under `/usr/local/share/shay/siil` by default.

Bundled modules include math, units, ASCII codes, text fragments, symbols, geometry, physics, time, booleans, data delimiters, messages, and numeric constants. Libraries can declare reusable functions with `$declr`, return numeric expressions with `forw`, or execute statement bodies containing `ifit` blocks and output statements. `finish` closes both kinds of function. See [`siil/README.md`](siil/README.md) for the complete list.

```shay
$shaiiport [math.shaii]
$shaiiport [text.shaii]
insval-math_pi
insval-text_space
```

## Initial syntax

```shay
instxt-"Hello, world!"
brew username = "Alex"
insval-username

brew age = 20
ifit age >= 18 andit notit age == 21
    instxt-"Adult"
elseit
    instxt-"Not an eligible adult"
exec
```

The current implementation supports string output, literal variable declarations, line-based input with `inpvarforw-name`, variable output, imported functions, numeric-returning functions, void functions with statement bodies, numeric comparisons, and logical condition operators (`andit`, `orit`, and `notit`). Parentheses group logical conditions, with precedence `notit` then `andit` then `orit`. It also supports `ifit` blocks inside functions, function calls, imports, duplicate-import protection, circular-import detection, and source-located diagnostics. A function may use `forw expression` for a numeric return, or contain statements followed by `finish` for a void function.

### Read input into a variable

`inpvarforw-name` reads one complete line from standard input. It creates `name` if it has not been declared yet; if `name` already exists, it replaces its previous value. Input is stored as text, including spaces, and can be used with `insval-name` afterward.

```shay
instxt-"What's your name?"
inpvarforw-name
instxt-"Hello,"
insval-name
```

The interpreter and compiled executables both read from standard input. `shaii --check` does not wait for input. The first implementation supports `inpvarforw` at the top level of a program.

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

## Shay-native compiler flags

Compiler flags use Shay's own `--shay...` spelling and can appear before or after the output mode and source path:

```sh
./build/gshai --shaywall --shaywextra --shayo2 -o examples/hello examples/hello.shay
```

| Shay flag | Native compiler option | Meaning |
|---|---|---|
| `--shaywall` | `-Wall` | Enable common compiler warnings |
| `--shaywextra` | `-Wextra` | Enable additional compiler warnings |
| `--shayo0` | `-O0` | Disable optimization |
| `--shayo1` | `-O1` | Basic optimization |
| `--shayo2` | `-O2` | Standard optimization (default) |
| `--shayo3` | `-O3` | Aggressive optimization |

These flags configure the native C++ compiler used by `gshai`; they do not change Shay source syntax. The existing `-o` and `-ob` output modes remain supported.

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
- Expanded functions, recursion, lexical scopes, closures, and control flow.
- Numeric, string, boolean, collection, and memory values.
- Checked memory/buffer operations and expanded math/string/geometry built-ins.
- Embedded and external library loading, plus Zeccurs task execution.
- Native executable generation, object linking, optimization levels, warning controls, and check-only mode.
- C/C++ host-function bridge and embedding API.
- Make/CMake builds and automated regression/diagnostic tests.

These are target capabilities, not claims that the bootstrap currently implements them.
