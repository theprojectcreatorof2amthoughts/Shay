# Shay syntax (initial implementation)

Shay is being implemented as a hybrid C/C++ language toolchain: the runtime is C11 and the frontend/driver is C++20.

## File types

- `.shay`: main Shay source.
- `.shaii`: Shay library.
- `.shaz`: Zeccurs task source. Zeccurs is variable-free.

## Output

```shay
instxt-"Hello World!"
```

## Variables

```shay
brew greeting = "Hello from Shay"
insval-greeting
```

The first implementation stores values as strings. Typed values and expression evaluation are not implemented yet.

## Imports

```shay
$shaiiport "library.shaii"
$shazport "task.shaz"
```

Import rules:

- `.shay` may import `.shaii` and `.shaz`.
- `.shaz` may import `.shaii` and other `.shaz` files.
- `.shaii` may import `.shaz`, but may not import another `.shaii`.
- The import path is relative to the importing file.
- Each file is loaded at most once per program.
- Circular imports are diagnosed.

The library declaration/definition syntax is still being designed. This initial runtime currently supports imports and the implemented output statements; it does not yet define library exports or functions.

## Zeccurs restrictions

A `.shaz` file cannot declare variables with `brew` or read Shay variables with `insval-`. It can output text and import allowed files.

## Build

```sh
make
make test
./shay examples/hello.shay
```
