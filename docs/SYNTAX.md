# Shay syntax notes

This document records the accepted Shay vocabulary and the current grammar. Shay is an independent language with its own syntax.

## Current statements

```shay
instxt-"Hello, world!"
brew username = "Alex"
insval-username
```

The current parser accepts string, numeric (including negative), and boolean literals for `brew`. String escapes currently supported are `\\n`, `\\t`, `\\r`, `\\\\`, and `\\"`. Zeccurs files (`.shaz`) are variable-free.

## Imports

```shay
$shaiiport [library.shaii]
$shazport [task.shaz]
```

| Importing file | `$shaiiport` | `$shazport` |
|---|---:|---:|
| `.shay` | Allowed | Allowed |
| `.shaz` | Allowed | Allowed |
| `.shaii` | Forbidden | Allowed |

Imports first resolve beside the importing file. `$shaiiport` additionally searches the project `siil/` standard-library directory (current working directory, then importer ancestors) and the optional `SHAY_STDLIB` directory. `$shazport` remains importer-relative. Circular imports are errors and duplicate imports load once.

## Toolchain

- `shaii` is the interpreter. It executes source files and supports `--check`.
- `gshai` is the compiler.
- `gshai -ob program.shay` emits `program.sho`, a native relocatable object.
- `gshai -o program.shay` emits a standalone executable named `program`.
- `gshai -o output-path program.shay` chooses the executable path.
- `gshai -ob output.sho program.shay` chooses the object path.

The compiler currently translates the supported Shay statement subset into native C++ code and invokes the host C++ compiler/linker. It embeds the small runtime write primitive into generated outputs. This is a native compilation workflow, but it is not yet a direct machine-code backend.

## Comments

A `##` comment runs to the end of the line.

## Frontend

The shared frontend tokenizes source into identifiers, numbers, strings, symbols, and line boundaries. The parser converts those tokens into a statement AST. Lexer and parser diagnostics include source line information.

## Not yet part of the grammar

Functions, control flow, collections, typed values, general expressions, error handling, and REPL behavior are not implemented yet. Their syntax will be designed as original Shay syntax.
