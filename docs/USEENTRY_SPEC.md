# Shay entry system

The `$useentry` directive activates an optional syntax extension by its lowercase filename stem.

```shay
$useentry rayinit
$useentry myfeature
```

Names contain lowercase English letters only. Missing extensions remain optional; an entry is activated only when a matching package is found.

## Binary extension packages

Both `gshai` and `shaii` discover `.shaye` packages recursively under `srchfile/siil/entryinit/` and `~/.shay/entries/`. Repository-local entries take precedence over user-installed entries with the same name.

A `.shaye` is a binary package, not a text file with a header. It begins with the `SHAYE` magic and a format version, followed by bytecode records. The current record defines a syntax alias: a new lowercase keyword is mapped to an existing Shay grammar keyword. The payload strings are byte-encoded, so opening the package in a text editor does not show the original extension source.

The byte encoding is not encryption. Anyone who implements the format can decode it; it is a binary package format, not a security boundary.

## Creating syntax extensions

Write extension declarations in a source file and compile them with `gshai -et`:

```shay
$entrysyn raydeclr declr
$entrysyn sayhello instxt
```

Then compile:

```sh
gshai -et rayinit.shaye rayinit-source.shay
```

Each `$entrysyn new_keyword existing_keyword` declaration creates an alias to a grammar keyword understood by Shay. There is no `$entrysyndef` behavior category: the bytecode records the mapping directly. An alias is available only after its package is activated with `$useentry`.

The bundled `rayinit.shaye` maps `raydeclr` to `declr`, allowing:

```shay
$useentry rayinit
$raydeclr rayui()
    instxt/>"Ray UI callback"
finish
```

The extension system adds new spellings for existing grammar productions. It does not execute native code or invent new runtime operations; new grammar/runtime primitives still need a corresponding implementation in Shay itself.

## Installing user entries

A normal build creates `~/.shay/entries/`. To install packages recursively while preserving subdirectories:

```sh
make insent ~/your_shaye_folder
```

The package filename remains its entry identity. For example, `tools/rayinit.shaye` is activated with `$useentry rayinit`. Set `ENTRY_HOME` to override the destination.

## Package validation

The loader rejects unknown opcodes, truncated records, invalid names, missing end markers, and trailing bytes. Invalid packages are ignored during discovery and reported if directly loaded.

The standard `.shaii` libraries remain under `srchfile/siil/`; this extension format does not change library imports.
