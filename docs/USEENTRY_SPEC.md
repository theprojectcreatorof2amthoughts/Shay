# Shay entry system

The `$useentry` directive activates an optional syntax extension by its lowercase filename stem.

```shay
$useentry rayinit
$useentry bininit
```

Names contain lowercase English letters only. Missing extensions remain optional; an entry is activated only when a matching package is found.

## Binary extension packages

Both `gshai` and `shaii` discover `.shpak` packages recursively under `srchfile/siil/entryinit/` and `~/.shay/entries/`. Repository-local entries take precedence over user-installed entries with the same name.

A `.shpak` is a binary package, not a text file with a header. It begins with the `SHPAK` magic and a format version, followed by bytecode records. Alias records map a new lowercase keyword to an existing Shay grammar keyword. Definition records store a named syntax definition and its pattern lines. The source payload contains the entry source plus the contents of every directly imported `.shaii` library. `$shaiiport` lines are resolved at package-build time and removed from the packaged entry source; library source is embedded into the package, so runtime use does not require the original `.shaii` files.

The byte encoding is not encryption. Anyone who implements the format can decode it; it is a binary package format, not a security boundary.

## Creating syntax extensions

Use `$entrysyn` for aliases to existing grammar keywords:

```shay
$entrysyn raydeclr declr
$entrysyn sayhello instxt
```

Use `$entrysyndef` to declare a zero-argument function-style entry syntax. The name decorations are optional: `dobase`, `$dobase`, `dobase()`, and `$dobase()` all declare the same syntax name. The definition body contains one or more syntax-pattern lines and ends with `finish`:

```shay
$entrysyndef dobase
    ::0xF014::
    ::0xFFFF::
finish

$entrysyndef binnum()
    ::0b0000::
    ::0b1111::
finish
```

When the entry is active with `$useentry`, its defined name is callable as a zero-argument shorthand for a same-named function declared in an imported `.shaii` library. Both call spellings are accepted:

```shay
$useentry baseinit
$dobase
dobase
```

These calls are equivalent to `dofunc dobase`. The imported function must exist and must not declare parameters. The entry package does not synthesize the function body: it supplies the syntax name, while the implementation remains in the imported library. Pattern lines are preserved in the package as syntax metadata.

Compile declarations with the dedicated entry compiler:

```sh
shentrypak bininit-source.shay -o bininit.shpak
```

`shentrypak` validates entry declarations and ordinary Shay syntax, resolves and validates imported `.shaii` libraries, then embeds those libraries before writing the package. Errors include the source filename, line, column, source line, and caret. A package is not emitted when validation fails. `gshai` no longer compiles entry packages; use it only for ordinary Shay applications.

Each definition is stored as a separate binary record, and the loader registers it under the active entry name. Pattern lines are preserved as authored as the body of the function-style syntax definition. Declaring and packaging the function signature does not, by itself, add a new lexer token, parser production, or runtime operation; those execution semantics still need their corresponding compiler/interpreter implementation.

The `.shpak` package format supports both alias and definition records in one package. Existing `.shaye` packages remain readable for migration, but new packages are written as `.shpak`.

## Installing user entries

A normal build creates `~/.shay/entries/`. `make insent` installs `.shpak` files recursively and preserves their subdirectories. To install packages recursively while preserving subdirectories:

```sh
make insent ~/your_shpak_folder
```

The package filename remains its entry identity. For example, `tools/rayinit.shpak` is activated with `$useentry rayinit`. Set `ENTRY_HOME` to override the destination.

## Package validation

The loader rejects unknown opcodes, truncated records, invalid names, empty definition blocks, missing end markers, and trailing bytes. Invalid packages are ignored during discovery and reported if directly loaded.

The standard `.shaii` libraries remain under `srchfile/siil/` for ordinary builds. When `shentrypak` builds an entry package, it resolves each imported `.shaii` using the normal Shay import search rules and embeds it. Missing or invalid imports stop package creation. `.shaii` libraries retain their own declaration permissions inside the package, and duplicate imports are embedded only once. As in ordinary Shay builds, `.shaii` libraries cannot import other `.shaii` libraries.

## Dedicated package compiler

`shentrypak` is built alongside `gshai` and `shaii`, and is installed with them. Its output is a binary `.shpak` file. It accepts one or more `.shay` entry-source files:

```sh
shentrypak -o output.shpak first.shay second.shay
shentrypak --help
```

`gshai -et` has been retired; use `shentrypak` for all entry-package compilation.
