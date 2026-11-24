# Siil — Shay Standard Library

Siil is the standard-library directory for Shay. Import a library from any `.shay` or `.shaz` file with:

```shay
$shaiiport [math.shaii]
```

The interpreter and compiler search in this order:

1. Beside the importing file.
2. The directory named by `SHAY_STDLIB`, when that environment variable is set.
3. A `srchfile/siil/` directory in the current working directory.
4. A `srchfile/siil/` directory in each parent directory of the importer.

The first matching file wins. Zeccur task imports remain relative to the importing file. Libraries are loaded once, and circular imports are diagnosed.

## Available libraries

| Library | Contents |
|---|---|
| `math.shaii` | Common mathematical constants |
| `units.shaii` | Time, length, and mass conversion factors |
| `ascii.shaii` | Common ASCII control and punctuation codes |
| `text.shaii` | Common text fragments and whitespace |
| `symbols.shaii` | Common programming and data symbols |
| `geometry.shaii` | Circle and shape constants |
| `physics.shaii` | Common physical constants |
| `time.shaii` | Time-unit lengths in seconds |
| `boolean.shaii` | Boolean values and logical labels |
| `data.shaii` | Common data-format delimiters |
| `messages.shaii` | Reusable user-facing messages |
| `numbers.shaii` | Numeric boundary and counting constants |

## Example

```shay
$shaiiport [math.shaii]
$shaiiport "text.shaii"

insval-math_pi
insval-text_space
```

## Current library model

The current Shay grammar supports literal `brew` declarations, `insval-` output, `instxt-` output, and imports. These starter libraries therefore provide named constants and reusable text values. They do not claim to provide callable functions yet. Library variables share the program's current global variable namespace, so each library uses a prefix to avoid collisions.

A library should normally declare values only. Importing it executes its top-level statements in source order.
