# Shay syntax notes

This document records the accepted Shay vocabulary and the current grammar. Shay is an independent language with its own syntax.

## Output and variables

All four output/input statements use the self-closing `/>` operator: `instxt/>`, `instxtexit/>`, `insval/>`, and `inpvarforw/>`. Their previous hyphen forms are no longer accepted.

```shay
instxt/>"Hello, world!"
brew username = "Alex"
insval/>username
```

The parser accepts string, numeric (including negative), and boolean literals for `brew`. String escapes currently supported are `\\n`, `\\t`, `\\r`, `\\\\`, and `\\"`. Zeccurs files (`.shaz`) are variable-free.

## Log files

Use `$logindxfl` to select the log extension and `$logindxnm` to select the filename. The selected extension is applied to the filename. If the filename already has an extension, Shay replaces it with the selected extension. Both directives are required before the first `logit/>`. If either is missing, Shay reports an error instead of creating a default file. Existing log files are appended to rather than overwritten.

```shay
$logindxfl <.log>
$logindxnm "test"

logit/>"Something went wrong in LN:5
instxt-"hi"
------^
Unexpected Symbol"
```

Extensions such as `.log`, `.txt`, and `.shl` are supported. A `logit/>` string may span multiple source lines; its line breaks and indentation are preserved. Each log entry receives a trailing newline when needed. Logging works in both the interpreter and compiled executables, including inside function and conditional bodies.


## Functions

Declare functions with `$declr`, return a value with `forw`, and close the declaration with `finish`:

```shay
$declr add(x, y)
    forw x + y
finish

insval/>add(10, 20)
```

Functions can be declared in `.shay` programs and `.shaii` libraries. A function may use a single top-level `forw` expression or mix statements with a `forw` return inside its body (including an `ifit` or `elseit` branch). Expressions support numeric literals, parameter names, parentheses, unary minus, and the arithmetic operators `+`, `-`, `*`, and `/`. Use `insval/>functionName(arguments)` to print a returned value. When a function marks itself failed with `!#functionName`, its returned value is not printed. The compiler generates native function code; the interpreter evaluates the same arithmetic expression syntax. Nested function calls, local declarations, and non-numeric return values are not implemented yet.

## Conditions and logical operators

Use `ifit` to begin a conditional block and `exec` to close it. Numeric comparisons support `>`, `<`, `>=`, `<=`, `==`, and `!=`. A single `=` is also accepted as an equality shorthand. Combine comparisons with Shay's logical operators:

- `andit` — both conditions must be true.
- `orit` — at least one condition must be true.
- `notit` — reverses the following condition.

```shay
brew age = 20
brew score = 85

ifit age >= 18 andit score > 70
    instxt/>"Eligible"
elseit
    instxt/>"Not eligible"
exec

ifit notit age < 18 orit score == 100
    instxt/>"Special case"
exec

ifit (age >= 18 andit score > 70) orit score == 100
    instxt/>"Grouped condition"
exec
```

Logical precedence is `notit`, then `andit`, then `orit`. Parentheses can group conditions. The interpreter and native compiler share these condition semantics.

Conditional bodies currently support output statements, function calls, nested conditionals, and an optional `elseit` fallback branch; other statements are not implemented yet.

## Error conditions and termination

Use `!#functionName` as a statement inside that same function to explicitly mark the current invocation as failed. Use `ifit !#functionName` in the caller to check the latest call result. The status belongs to the named function call; it is not the status of the previous unrelated statement and does not treat a false-y return value as an error. A marker statement must name the function currently executing, and the failure remains set when that invocation returns.

```shay
$declr useme(x, y)
    ifit x*y = 34
        !#useme
    elseit
        instxt/>"Result:"
        forw x*y
    exec
finish

insval/>useme(17, 2)
ifit !#useme
    instxtexit/>"Error!"
exec

$declr divide(left, right)
    forw left / right
finish

insval/>divide(10, 0)
ifit !#divide
    instxt/>"The divide operation failed"
elseit
    instxt/>"The divide operation succeeded"
exec
```

A successful later call to the same function clears its previous failure status. The interpreter and native compiler both detect division by zero and retain the failure reason. If a failure is not checked with `!#`, the tool reports the function and its reason.

Use `instxtexit/>"message"` to print a message and terminate the program with a non-zero exit status:

```shay
insval/>divide(10, 0)
ifit !#divide
    instxtexit/>"Error!"
exec
```

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

Loops, nested lexical scopes, collections, typed values, general expressions outside numeric function returns, and REPL behavior are not implemented yet. Function bodies are currently limited to an optional return expression or supported statement bodies.
