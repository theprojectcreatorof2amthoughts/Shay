# Shay syntax notes

This document records the accepted Shay vocabulary and the current grammar. Shay is an independent language with its own syntax.

## Output and variables

```shay
instxt-"Hello, world!"
brew username = "Alex"
insval-username
```

The parser accepts string and boolean literals, numeric literals, and arithmetic numeric expressions for `brew`. Numeric initializers can use previously declared numeric variables, parentheses, unary `+`/`-`, and `+`, `-`, `*`, `/` with normal arithmetic precedence. For example: `brew total = unit_price * quantity + 2`. A string or boolean variable cannot be used as a numeric operand. String escapes currently supported are `\\n`, `\\t`, `\\r`, `\\\\`, and `\\"`. Zeccurs files (`.shaz`) are variable-free.

## Functions

Declare functions with `$declr`, return a value with `forw`, and close the declaration with `finish`:

```shay
$declr add(x, y)
    forw x + y
finish

insval-add(10, 20)
```

Functions can be declared in `.shay` programs and `.shaii` libraries. A function currently has zero or more numeric parameters and an optional single `forw` expression. Expressions support numeric literals, parameter names, parentheses, unary minus, and the arithmetic operators `+`, `-`, `*`, and `/`. Use `insval-functionName(arguments)` to print a returned value. The compiler generates native function code; the interpreter evaluates the same arithmetic expression syntax. Nested function calls, local declarations, multiple statements in a function body, and non-numeric return values are not implemented yet.

## Conditions

Use `ifit` to begin a conditional block and `exec` to close it:

```shay
brew x = 20
brew y = 10

ifit x > y
    instxt-"X is greater than Y!"
exec
```

Conditions currently support numeric comparisons: `>`, `<`, `>=`, `<=`, `==`, and `!=`. The interpreter and compiler both execute the block only when the condition is true. Conditional bodies currently support output statements and function calls; nested conditionals and other statements inside conditional blocks are not implemented yet.

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

Loops, nested conditionals, collections, typed values, general expressions outside numeric function returns, error handling, and REPL behavior are not implemented yet. Function bodies are currently limited to a single optional return expression.
