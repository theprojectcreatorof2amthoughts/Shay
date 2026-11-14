# Shay syntax notes

This document records the accepted Shay vocabulary and the current compiler grammar. Shay is an independent language with its own syntax.

## Current statements

```shay
instxt-"Hello, world!"
brew username = "Alex"
insval-username
```

The current parser accepts string, numeric (including negative), and boolean literals for `brew`. String escapes currently supported are `\\n`, `\\t`, `\\r`, `\\\\`, and `\\"`. Zeccurs files (`.shaz`) are variable-free.

## Imports

```shay
$shaiiport "library.shaii"
$shazport "task.shaz"
```

| Importing file | `$shaiiport` | `$shazport` |
|---|---:|---:|
| `.shay` | Allowed | Allowed |
| `.shaz` | Allowed | Allowed |
| `.shaii` | Forbidden | Allowed |

Imports are relative to the importing file. Circular imports are errors and duplicate imports load once.

## Comments

A `##` comment runs to the end of the line.

## Compiler pipeline

The compiler tokenizes source into identifiers, numbers, strings, symbols, and line boundaries. The parser converts those tokens into a small statement AST before semantic checks and execution. Lexer and parser diagnostics include source line information.

## Not yet part of the grammar

Functions, control flow, collections, typed values, general expressions, error handling, REPL behavior, and native code generation are not implemented yet. Their syntax will be designed as original Shay syntax.
