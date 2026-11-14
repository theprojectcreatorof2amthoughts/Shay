# Shay syntax notes

This document records the accepted Shay vocabulary and the current bootstrap grammar. Shay is independent of Y_ and Y++.

## Current statements

```shay
instxt-"Hello, world!"
brew username = "Alex"
insval-username
```

The bootstrap accepts string, numeric, and boolean literals for `brew`. Zeccurs files (`.shaz`) are variable-free.

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

A line whose first non-whitespace characters are `##` is a comment.

## Planned

Functions, control flow, collections, typed values, expressions, error handling, REPL behavior, and native code generation require their own grammar and implementation. Do not substitute C, C++, Y_, or Y++ keywords for Shay syntax.
