# Shay language capability specification

## Goal
Shay should cover the practical programming capabilities of Y_ and Y++, then extend them. This is a feature target, not a claim that every feature is implemented today. Shay keeps its own grammar and vocabulary; it is not Y_ or Y++ with renamed extensions.

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

## Capability parity target

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
5. Interop/ecosystem: C/C++ ABI, embedding API, library packaging, docs, examples, platform CI.
6. Beyond parity: package management, typed interfaces/generics after syntax design, async/task orchestration, debugger/profiler hooks, stable ABI/versioning.

## Compatibility policy
Capability parity does not mean syntax compatibility. Do not copy Y_ or Y++ keywords into Shay. New Shay syntax must be proposed, documented, tested, and approved before implementation. Zeccurs remains variable-free unless its language design is explicitly revised.
