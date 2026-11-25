# Shay entry system

The `$useentry` directive activates an optional syntax extension by its lowercase filename stem.

```shay
$useentry rayinit
$useentry bininit
```

Names contain lowercase English letters only. Missing extensions remain optional; an entry is activated only when a matching package is found.

## Binary extension packages

Both `gshai` and `shaii` discover `.shaye` packages recursively under `srchfile/siil/entryinit/` and `~/.shay/entries/`. Repository-local entries take precedence over user-installed entries with the same name.

A `.shaye` is a binary package, not a text file with a header. It begins with the `SHAYE` magic and a format version, followed by bytecode records. Alias records map a new lowercase keyword to an existing Shay grammar keyword. Definition records store a named syntax definition and its pattern lines. Payload strings are byte-encoded, so opening the package in a text editor does not show the original extension source.

The byte encoding is not encryption. Anyone who implements the format can decode it; it is a binary package format, not a security boundary.

## Creating syntax extensions

Use `$entrysyn` for aliases to existing grammar keywords:

```shay
$entrysyn raydeclr declr
$entrysyn sayhello instxt
```

Use `$entrysyndef` to declare a named syntax pattern with one or more pattern lines. The block ends with `finish`:

```shay
$entrysyndef hexnum
    ::0x0000::
    ::0xFFFF::
finish

$entrysyndef binnum
    ::0b0000::
    ::0b1111::
finish
```

Compile declarations with:

```sh
gshai -et bininit.shaye bininit-source.shay
```

Each definition is stored as a separate binary record, and the loader registers it under the active entry name. Pattern lines are preserved as authored; they are not interpreted as executable code. A definition record by itself does not implement a new lexer token, parser production, or runtime operation. Those semantics must be implemented by Shay's compiler/interpreter and associated with the definition. In particular, defining `hexnum`, `binnum`, or sector-operation patterns does not by itself add hexadecimal/binary numeric evaluation or BaseFul sector functionality.

The `.shaye` package format supports both alias and definition records in one package. Existing alias-only packages remain readable.

## Installing user entries

A normal build creates `~/.shay/entries/`. To install packages recursively while preserving subdirectories:

```sh
make insent ~/your_shaye_folder
```

The package filename remains its entry identity. For example, `tools/rayinit.shaye` is activated with `$useentry rayinit`. Set `ENTRY_HOME` to override the destination.

## Package validation

The loader rejects unknown opcodes, truncated records, invalid names, empty definition blocks, missing end markers, and trailing bytes. Invalid packages are ignored during discovery and reported if directly loaded.

The standard `.shaii` libraries remain under `srchfile/siil/`; this extension format does not change library imports.
