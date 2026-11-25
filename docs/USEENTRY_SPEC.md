# Shay entry system

The `$useentry` directive selects an optional Shay extension by its lowercase entry name. Built-in entry names remain valid, and extension names are discovered from `srchfile/siil/entryinit/`.

## Syntax

```shay
$useentry reginit
$useentry advsynt
$useentry myfeature
```

Names contain lowercase English letters only (a through z). Multiple directives are allowed. Repeating an entry is harmless. An entry with no matching package is skipped, so optional extensions do not make a project fail simply because they are not installed.

## Extension package discovery

Both `gshai` and `shaii` discover `.shaye` packages recursively in the repository's `srchfile/siil/entryinit/` directory and in the user's persistent `~/.shay/entries/` directory. Repository-local entries take precedence when two packages have the same entry name.

```text
Shay repository/
└── srchfile/siil/entryinit/*.shaye

User home/
└── .shay/entries/**/*.shaye
```

The package filename, without `.shaye`, is its entry name. For example, `myfeature.shaye` is activated by `$useentry myfeature`. A source file activates the package corresponding to each `$useentry` directive. Packages can contain Shay library declarations and their definitions become available to the importing program.

Repository-local discovery walks the source file's directory ancestors and the current working directory. The user entry directory is independent of the repository, so installed packages remain available after the Git checkout is moved or deleted. `HOME` is used on Unix-like systems and `USERPROFILE` is the Windows fallback.

## Installing user entries

A normal `make` build creates `~/.shay/entries/`. To install every `.shaye` file found recursively under a chosen folder, run:

```sh
make insent ~/your_shaye_folder
```

The folder's relative subdirectory structure is preserved. The package filename remains its entry identity, so `tools/math/main.shaye` is selected with `$useentry main`. Existing files with the same relative path are replaced. The equivalent variable form is `make insent ENTRY_DIR=~/your_shaye_folder`. You can override the destination with `ENTRY_HOME=/some/path`.

## Creating a package

```sh
gshai -et calculator.shaye calculator.shay
```

The current portable `.shaye` container format has a `SHAYE/1` header followed by its packaged Shay source payload. The payload is parsed by the normal Shay frontend when loaded; this keeps one package usable by both the native compiler and interpreter. It is a portable source package, not a machine-code binary.

## Entry-defined syntax

A `.shaye` package can register additional `$` directives with the general entry-syntax mechanism:

```shay
$entrysyn raydeclr
$entrysyndef function
```

`$entrysyn` names the new directive. The following `$entrysyndef` assigns its behavior. The initial supported behavior is `function`, which reuses Shay's existing function-declaration grammar and runtime. The syntax is registered to the package's filename entry name and becomes available only after that entry is selected with `$useentry`.

The bundled `rayinit.shaye` registers `$raydeclr` as a function declaration. Shay's parser reads that registration from the package rather than containing a special case for the keyword. Other `.shaye` packages can register their own function-declaration keywords the same way.

This is a declarative syntax extension, not arbitrary native-code injection. Additional behavior types can be added as Shay gains more extension hooks.

`$raydeclr` declarations are accepted in ordinary `.shay` files after `$useentry rayinit`; they are registered callable functions, ready for a Ray UI host integration. The package itself does not contain a Ray UI renderer.

## Standard library

The standard `.shaii` libraries live in `srchfile/siil/`. Existing `$shaiiport [name.shaii]` imports resolve through that directory, in addition to importer-relative paths and the optional `SHAY_STDLIB` override.

## Optional behavior

- A missing package for a syntactically valid lowercase entry name is ignored.
- An installed package is loaded once, even if the same entry is requested repeatedly.
- Invalid names containing capitals, digits, or symbols remain syntax errors.
- Built-in names continue to work without a corresponding `.shaye` package.
- User-installed entries in `~/.shay/entries/` survive deletion or replacement of the Shay repository.

## Binary and hexadecimal literals

The optional `bininit` entry enables native base-prefixed integer literals:

```shay
$useentry bininit
brew flags = ::0b101010::
brew mask = ::0xF76A::
brew combined = flags + mask
ifit combined == 63340
    instxt/>"Binary and hexadecimal values are numeric"
exec
```

Binary digits are limited to `0` and `1`; hexadecimal digits accept `0-9`, `a-f`, and `A-F`. Values are range-checked as unsigned 64-bit integers and normalized to decimal numeric tokens, so assignments, arithmetic, comparisons, and expressions share the existing numeric implementation in both `gshai` and `shaii`. The syntax is rejected unless `$useentry bininit` appears before the literal.

The package declares the numeric syntax hook in its source payload:

```shay
$entrysyn binliteral
$entrysyndef number
```

Create the package with `gshai -et bininit.shaye main.shay`, then place it in `srchfile/siil/entryinit/` or install it under `~/.shay/entries/`.
