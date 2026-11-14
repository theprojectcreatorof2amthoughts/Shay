# Shay language capability specification

## Goal

Shay is being developed as a practical, general-purpose programming language with an original grammar, runtime, compiler, and ecosystem. Its syntax and vocabulary must remain distinctly Shay; new constructs are designed for this language rather than copied from another language.

## Language layers

- **Shay (.shay):** general-purpose source, values, expressions, functions, control flow, and application logic.
- **Shaii (.shaii):** reusable library source. May import Zeccurs, but may not import another Shaii library.
- **Zeccurs (.shaz):** variable-free task source. May import Shaii libraries and other Zeccurs files.

## Import permissions

| Importer | `$shaiiport "x.shaii"` | `$shazport "x.shaz"` |
|---|---:|---:|
| .shay | Allowed | Allowed |
| .shaz | Allowed | Allowed |
| .shaii | Forbidden | Allowed |

Imports resolve relative to the importing file. The loader must canonicalize paths, load each module once, report missing files at the import site, detect cycles, preserve dependency-chain diagnostics, and keep execution order deterministic.

## Language capability targets

### Language and execution

- Lexer with source spans, escaped strings, comments, and precise malformed-token diagnostics.
- Parser and typed AST; regex-driven statement execution is only bootstrap scaffolding.
- Expression precedence, unary/binary operators, short-circuit logic, comparisons, grouping, and calls.
- Numbers, strings, booleans, empty/null, characters, arrays/collections, functions, and checked memory values.
- Variables, constants, assignment, nested lexical scopes, closures, recursion, and return values.
- Conditional branches, while/counting loops, loop control, and bounded execution safeguards.
- String conversion, inspection, substring/replacement, repetition, aggregation, and formatting.
- Numeric conversion, predicates, math, geometry, interpolation, and angle helpers.
- Checked byte buffers with bounds checks and explicit size limits.

### Runtime and tooling

- Persistent REPL with multiline input, diagnostics, and environment continuity.
- Interpreter and native executable workflow with consistent semantics.
- Check-only mode, output path, optimization levels, warning controls, verbose/debug modes, and useful exit codes.
- Multiple diagnostics per compilation, source excerpts/carets, import backtraces, and warning categories.
- Standard library and Zeccurs task runner with deterministic dependency resolution.
- C ABI for embedding and C/C++ native-function registration; documented ownership and error contracts.
- Make and CMake builds, unit/integration/regression tests, and CI on supported platforms.

### Native compilation

The native backend must produce a standalone executable from a Shay entry file and its reachable dependencies. It must package/embed the runtime and modules, preserve source diagnostics, and expose target/compiler selection. Optimization levels must be documented and tested. Do not call a host-compiler invocation direct Shay machine-code generation unless that accurately describes the backend.

## Implementation stages

1. Bootstrap correctness: build reliably; lock down extensions, import permissions, Zeccurs variable prohibition, diagnostics, regression tests.
2. Language core: lexer, parser, AST, values, expressions, scopes, functions, control flow, REPL.
3. Standard capabilities: strings, math, geometry, collections, checked memory, I/O, standard library.
4. Compiler: native executable generation, check mode, optimization/warnings, dependency embedding, reproducible builds.
5. Interoperability and ecosystem: C/C++ ABI, embedding API, library packaging, docs, examples, platform CI.
6. Beyond the core: package management, typed interfaces/generics after syntax design, async/task orchestration, debugger/profiler hooks, stable ABI/versioning.

## Language design policy

Capability goals describe what Shay should eventually support, not what is already implemented. Do not copy keywords or grammar from unrelated languages. Every new Shay construct must be proposed, documented, tested, and approved before implementation. Zeccurs remains variable-free unless its language design is explicitly revised.
