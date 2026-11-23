# Shay entry system

The $useentry directive configures the language backbone available to one source file. It is not a library import, package loader, or platform adapter. It establishes the syntax and language structures that the file is allowed to use.

The directive is available in .shay, .shui, and .shuil files. A source file may declare multiple entries; their configurations are composed into that file's language environment.

## Syntax

Each directive has one plain lowercase name:

```shay
$useentry reginit
$useentry advsynt
$useentry exprinit
$useentry varinit
$useentry funcinit
$useentry condinit
$useentry loopinit
```

Names contain lowercase English letters only: a through z. Capitals, digits, underscores, hyphens, dots, and other symbols are not permitted. Each name must also exist in the registered entry catalog. Unknown names are errors.

Multiple entries are allowed. Repeating an entry does not import anything and does not load a library. Entries are file-level language configuration.

## Registered entries

### Foundational syntax

| Entry | Language backbone |
|---|---|
| reginit | Foundational Shay syntax and regular declarations |
| advsynt | Advanced syntax structures |
| exprinit | Expressions and expression parsing |
| varinit | Variables, declarations, access, and assignment |
| typeinit | Type definitions and type checking |
| constinit | Constants and immutable values |
| scopeinit | Scope and visibility rules |

### Program structures

| Entry | Language backbone |
|---|---|
| funcinit | Functions and methods |
| classinit | Classes and class members |
| objinit | Object-oriented structures |
| structinit | Structured data declarations |
| enuminit | Enumerations |
| geninit | Generic and reusable type structures |

### Logic and execution

| Entry | Language backbone |
|---|---|
| operinit | Operators and operator precedence |
| logicinit | Boolean and logical expressions |
| condinit | Conditional statements |
| loopinit | Repetition and loop structures |
| flowinit | Execution flow and branching |
| errinit | Error handling and diagnostics |
| asyncinit | Asynchronous execution |

### Data and system features

| Entry | Language backbone |
|---|---|
| strinit | String handling |
| numinit | Numeric values |
| boolinit | Boolean values |
| arrayinit | Arrays and indexed collections |
| meminit | Memory management |
| ptrinit | Pointer operations |
| ioinit | Input and output |
| mathinit | Mathematical operations |

### Application features

| Entry | Language backbone |
|---|---|
| uiinit | UI application structures |
| eventinit | Event-driven execution |
| netinit | Networking structures |
| threadinit | Threading and parallel execution |

## File-specific responsibilities

- .shay: entries configure the language backbone for a regular Shay source file.
- .shui: uiinit enables the UI application structure. Other entries can be combined with it for Shay logic and syntax used by the app.
- .shuil: entries configure the language environment used by the UI adapter definition.

The .shuil adapter still selects the platform and translation method through its own adapter declarations. The .shui app envelope identifies the matching UI structure. $useentry does not select Browser or Windows.

## Relationship to libraries

A library supplies reusable functions, definitions, or other library content. $useentry configures the source file's language backbone. They are separate mechanisms and neither substitutes for the other.

For example, this configures the source language and then imports a library:

```shay
$useentry reginit
$useentry exprinit
$useentry varinit
$shaiiport [math.shaii]
```

## Implementation status

The current Shay frontend recognizes the registered names, validates lowercase-only spelling, permits multiple directives, and treats them as file-level configuration rather than executable statements. The catalog defines the intended language backbones; individual advanced language features are implemented separately as their parser and runtime support is developed.
