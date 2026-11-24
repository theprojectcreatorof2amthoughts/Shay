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

Both `gshai` and `shaii` look only in:

```text
srchfile/
└── siil/
    ├── *.shaii
    └── entryinit/
        └── *.shaye
```

The package filename, without `.shaye`, is its entry name. For example, `myfeature.shaye` is activated by `$useentry myfeature`. A source file activates the package corresponding to each `$useentry` directive. Packages can contain Shay library declarations and their definitions become available to the importing program.

The discovery root is found by walking the source file's directory ancestors and the current working directory. No other directory is searched for `.shaye` packages.

## Creating a package

```sh
gshai -et calculator.shaye calculator.shay
```

The current portable `.shaye` container format has a `SHAYE/1` header followed by its packaged Shay source payload. The payload is parsed by the normal Shay frontend when loaded; this keeps one package usable by both the native compiler and interpreter. It is a portable source package, not a machine-code binary.

## Standard library

The standard `.shaii` libraries live in `srchfile/siil/`. Existing `$shaiiport [name.shaii]` imports resolve through that directory, in addition to importer-relative paths and the optional `SHAY_STDLIB` override.

## Optional behavior

- A missing package for a syntactically valid lowercase entry name is ignored.
- An installed package is loaded once, even if the same entry is requested repeatedly.
- Invalid names containing capitals, digits, or symbols remain syntax errors.
- Built-in names continue to work without a corresponding `.shaye` package.
