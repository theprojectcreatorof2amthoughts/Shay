# Shay

Shay is an independent hybrid programming language with a C11 runtime and a C++20 compiler toolchain. It provides two separate command-line programs:

- **`gshai`** — the Shay compiler. It creates native executables or relocatable `.sho` object files.
- **`shaii`** — the Shay interpreter. It runs Shay source files and provides syntax-check mode.

Shay has its own original grammar and vocabulary, including `brew` for variable declarations and `instxt/>"..."` for text output.

## File formats

| Extension | Meaning |
|---|---|
| `.shay` | Shay program |
| `.shaii` | Shay library |
| `.shaz` | Zeccur task file |
| `.shui` | Shay UI application source, processed by `gshai` or `shaii`; composes language entries |
| `.shuil` | Shay UI adapter, loaded by `.shui`; configures the platform translator and its language backbone |
| `.sho` | Compiled Shay object file (native relocatable object) |

## Vocabulary
| Vocabulary | Meaning |
|---|---|
| `Zeccur` | A non variable code file |
| `Shay` | Arabic word for tea (شاي) |

## Imports

```shay
$shaiiport [math.shaii]          ## implemented library from srchfile/siil/
$shaiiport "external/custom.shaii" ## external file path on disk
$shazport [startup.shaz]         ## Zeccur task
```

| Importing file | `$shaiiport` | `$shazport` |
|---|---:|---:|
| `.shay` | Yes | Yes |
| `.shaz` | Yes | Yes |
| `.shaii` | No | Yes |

Bracketed `$shaiiport [name.shaii]` imports an implemented library, searched beside the importer and then through `srchfile/siil/`. Quoted `$shaiiport "path/to/file.shaii"` imports that external file relative to the importing file (or from an absolute path) and does not search `srchfile/siil/`. `$shazport` continues to use bracket syntax for Zeccur tasks. Duplicate files are loaded once and circular imports are diagnosed. Zeccurs is task-oriented and variable-free.

## Language entries

Shay source files can configure their language backbone with multiple lowercase $useentry directives. The system applies to .shay, .shui, and .shuil files. Entry names contain lowercase letters only and may be supplied by an optional `.shaye` package in `srchfile/siil/entryinit/`; a missing optional entry is skipped.

```shay
$useentry reginit
$useentry advsynt
$useentry exprinit
$useentry varinit
$useentry funcinit
```

For UI applications, add $useentry uiinit. The .shuil adapter—not $useentry—selects Browser or Windows and its translation backend. See [the useentry specification](docs/USEENTRY_SPEC.md) for the full catalog and [the SHAY UI specification](docs/SHUI_SPEC.md) for UI source structure.

## Siil standard library

The repository includes a standard-library directory named `srchfile/siil/`. `$shaiiport` checks beside the importing file first, then checks `SHAY_STDLIB` if set, followed by `srchfile/siil/` in the current working directory and ancestor directories. Set `SHAY_STDLIB` to a library directory (for example, `/usr/local/share/shay/siil`) when using an installed copy. `make install` installs the bundled libraries under `/usr/local/share/shay/siil` by default.

Bundled modules include math, units, ASCII codes, text fragments, symbols, geometry, physics, time, booleans, data delimiters, messages, and numeric constants. Libraries can declare reusable functions with `$declr`, return numeric expressions with `forw`, or execute statement bodies containing `ifit` blocks and output statements. `$declr` is restricted to `.shaii` files. Classes are also declared only in `.shaii` files with `$classfunc !name`; each `$declrmthd method_name` declares one no-argument method. `finish` closes each method and then the class. In either `.shay` or `.shaii`, `dofunc function_name` executes a previously declared no-argument function, while `$doclass !name` executes every method in declaration order. `#%` starts a comment (the existing `##` comment form remains supported). See [`srchfile/siil/README.md`](srchfile/siil/README.md) for the complete list.

```shay
$shaiiport [math.shaii]
$shaiiport [text.shaii]
insval/>math_pi
insval/>text_space
```

## Initial syntax

```shay
instxt/>"Hello, world!"
brew username = "Alex"
insval/>username

brew age = 20
ifit age >= 18 andit notit age == 21
    instxt/>"Adult"
elseit
    instxt/>"Not an eligible adult"
exec
```

The current implementation supports string output, literal variable declarations, line-based input with `inpvarforw/>name`, variable output, imported functions, numeric-returning functions, void functions with statement bodies, numeric comparisons, and logical condition operators (`andit`, `orit`, and `notit`). Parentheses group logical conditions, with precedence `notit` then `andit` then `orit`. It also supports `ifit` blocks inside functions, function calls, imports, duplicate-import protection, circular-import detection, and source-located diagnostics. A function may use `forw expression` for a numeric return, or contain statements followed by `finish` for a void function.

### Read input into a variable

`inpvarforw/>name` reads one complete line from standard input. It creates `name` if it has not been declared yet; if `name` already exists, it replaces its previous value. Input is stored as text, including spaces, and can be used with `insval/>name` afterward. A single `insval/>` statement can output up to 10 variables separated by commas; each value is printed on its own line in the order listed. This works at the top level and inside functions or class methods.

```shay
instxt/>"What's your name?"
inpvarforw/>name
instxt/>"Hello,"
insval/>name
insval/>first, second, third
```

The interpreter and compiled executables both read from standard input. `shaii --check` does not wait for input. The first implementation supports `inpvarforw` at the top level of a program.

### Configure log files

`$logindxfl` and `$logindxnm` accept either their original literal form or the name of a variable. Variable values are resolved when the program runs, so they can be filled by `inpvarforw/>`:

```shay
instxt/>"Log extension (for example .shl):"
inpvarforw/>cmd
instxt/>"Name of file:"
inpvarforw/>nmin

$logindxfl cmd
$logindxnm nmin
logit/>"This goes into the selected file"
```

The literal forms remain supported: `$logindxfl <.shl>` and `$logindxnm "my-log"`. The extension must begin with a dot and contain only letters, digits, dots, underscores, or hyphens; the filename cannot be empty. The selected filename's existing extension is replaced by the selected extension.

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

A project can also be compiled from multiple source roots. Files are loaded in the order supplied and share one program namespace and runtime state:

```sh
./build/gshai -o bin/my-program src/main.shay src/ui.shay src/tasks.shaz
```

Use the same form with `-ob output.sho` to produce one relocatable object containing all supplied source roots. An explicit output path is required when compiling more than one source file. Imports remain relative to the file containing each import, and duplicate imports are loaded only once.

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


## UI applications

Shay UI is a source format within Shay, not a separate language. Pass `.shui` files directly to the normal tools:

```sh
./build/shaii --check examples/browser-demo.shui
./build/shaii examples/browser-demo.shui
./build/gshai -o build/browser-demo examples/browser-demo.shui
./build/browser-demo
# On a desktop, Shay opens build/browser-demo.html automatically

./build/gshai -o build/windows-demo examples/windows-demo.shui
dotnet build build/windows-demo.csproj
# Run build/windows-demo-ui.exe
```

The shared frontend validates the composed lowercase `$useentry` directives and referenced `.shuil` adapter, then routes embedded Shay statements through the ordinary Shay parser, interpreter, or native compiler. The adapter selects the platform; `$useentry uiinit` enables the UI language backbone. UI class methods use the existing Shay runtime.

Browser `.shui` builds use JavaScript. Windows `.shui` applications are migrating to Mouz: the Windows `.shuil` adapter selects `mozTrans` and references a `.mozf` source with `#*$mozfsport`; the Mouz compiler owns the `.mozf` → `.mozc` compilation. The legacy C# Windows adapter remains only for compatibility. The new native Mouz window generator and runtime loader are not implemented yet, and Shay refuses to silently generate C# for a Mouz adapter. Browser and legacy Windows Forms `.shui` builds share the same loopback runtime bridge. `<:onclick=method:>` routes clicks into no-argument Shay methods, and `<:bind=variable:>` synchronizes declared Shay values with text boxes, checkboxes, and labels. Windows output includes a `.cs` file and `.csproj`; build it with `dotnet build build/windows-demo.csproj`. The generated Windows app starts the compiled Shay runtime beside it. Browser apps start the compiled executable; it opens the generated `.html` file automatically when a desktop session is available. The browser sends an authenticated heartbeat, and the runtime exits cleanly ninety seconds after the last page heartbeat. The adapters remain bridges to Shay—not separate Shay runtimes.

See [the SHAY UI specification](docs/SHUI_SPEC.md) and the [browser](examples/browser-demo.shui) and [Windows](examples/windows-demo.shui) examples.
