# Shay UI language: .shui and .shuil

This document describes the **directive-based UI language**. It supersedes the earlier proposed `$app { ... }` syntax for new UI projects. The older parser/renderer remains a legacy implementation until the new frontend is integrated; do not silently translate one syntax into the other.

## File roles

- `.shui`: application source. Selects an entry target, declares UI classes, invokes those classes, describes controls, and defines when the app runs and finishes.
- `.shuil`: platform UI control/translation file. Selects the platform, translation method, and human language, then loads and executes the platform translator.
- The same logical `.shui` app can use different `.shuil` adapters. Platform-specific output belongs in the adapter, not in shared app logic.

## .shui application syntax

The following is the canonical browser example. The Windows form uses `!%#csElementapp <{` instead. Directive spelling and casing are intentional.

```shay
$useentry UI(Browser)

!%#browsElementapp <{
    *#uiContrl "browserutil.shuil"
    *!#classUideclr !u#browserCalc
        $uiSet 0
        $declrmthd doCalc
            x*y + x*y
        finish
    finish
    *!#doUiclass <"!u#browserCalc">
    *#drawui <'square'> <<'568xx654'>>
    *#drawui <'buttoncircle'> <:intercative=yes:>
}>
$dothis
$uiFinish
```

### Directives and rules

| Syntax | Role |
|---|---|
| `$useentry UI(Browser)` | Selects the UI entry target. `Browser` is the target in this example. |
| `!%#browsElementapp <{` | Opens a browser UI application body. |
| `!%#csElementapp <{` | Opens a Windows/C# UI application body. |
| `}>` | Closes the UI application body. |
| `*#uiContrl "file.shuil"` | Loads the named platform control/translation file. |
| `*!#classUideclr !u#Name` | Declares a UI class. UI class names must use the `!u#` prefix. |
| `$uiSet value` | Marks whether the current code is UI code. This directive is valid only in `.shui`; it is not a UI state initializer. |
| `$declrmthd name` | Declares a method inside a UI class. |
| `*!#doUiclass <"!u#Name">` | Invokes the named UI class. References use the same `!u#` prefix. |
| `*#drawui <'kind'>` | Declares/draws a UI element of the given kind. |
| `<<'id'>>` | Optional explicit element ID following a `drawui` element kind. |
| `<:key=value:>` | Optional element metadata. The spelling `intercative` in the example is preserved as supplied; its accepted values and event semantics are not yet finalized. |
| `$dothis` | Executes the application. |
| `$uiFinish` | Marks the end of the UI application lifecycle. |

A class declaration is closed by `finish`; each method is also closed by `finish`. Therefore the sample has one `finish` for `doCalc` and another for `browserCalc`.

### UI class naming

- UI class declaration and invocation names use `!u#`.
- This namespace is distinct from ordinary Shay class names (`!`) and method/translator class names in `.shuil` (`!m#`).
- A missing or incorrect prefix is a syntax/validation error, not an alias.

## .shuil platform adapter

A `.shuil` file configures how the selected platform receives the UI and how the UI is translated.

```shay
#*$ContrlDefine <browser>
#*!MethdUi <javasTrans>

#*!LangUI <US>
#*!classmethdDeclr !m#javasMain
    $uiset 0
    $declrmthd TransMain
        $mthdset 1

        #*$javasport [uicontrol.js]
        #*$doJavaScript
    finish
finish
```

| Syntax | Role |
|---|---|
| `#*$ContrlDefine <browser>` | Selects the platform control target. |
| `#*!MethdUi <javasTrans>` | Selects the UI translation method/engine. |
| `#*!LangUI <US>` | Selects the human language of UI text (US English in the example), not a programming language. |
| `#*!classmethdDeclr !m#Name` | Declares a platform translation class/method container. The `!m#` namespace is distinct from `!u#`. |
| `$uiset 0` | Appears in the supplied `.shuil` example. Its relationship to the case-sensitive `$uiSet` directive in `.shui` remains intentionally distinct until specified. |
| `$mthdset 1` | Sets the method execution mode/flag; exact values and semantics remain to be specified. |
| `#*$javasport [uicontrol.js]` | Loads Shay's integrated JavaScript translator file. |
| `#*$doJavaScript` | Runs the JavaScript translation stage. |
| `#*$csharpsport [uicontrol.cs]` | Selects the integrated C# translator for Windows. |
| `#*$doCSharp` | Runs the C# translation stage. |

For an external JavaScript translator, use a quoted filename inside the brackets:

```shay
#*$javasport ["your_file.js"]
```

Do not confuse this with `*#uiContrl "browserutil.shuil"`, which loads a `.shuil` adapter from the application.


### Native Windows / C# adapter

Windows uses a native Windows Forms backend, not a browser wrapper. The corresponding adapter is:

```shay
#*$ContrlDefine <windows>
#*!MethdUi <csharpTrans>
#*!LangUI <US>
#*!classmethdDeclr !m#csharpMain
    $uiset 0
    $declrmthd TransMain
        $mthdset 1
        #*$csharpsport [uicontrol.cs]
        #*$doCSharp
    finish
finish
```

A `.shui` app can point to this adapter with `*#uiContrl "windowsutil.shuil"`. The first generator maps `square` to a WinForms `Panel`, `buttoncircle` to `Button`, `label` to `Label`, `textbox` to `TextBox`, and `checkbox` to `CheckBox`. IDs become control names. Unsupported element kinds are reported instead of silently discarded.

### Build commands

```sh
shui validate examples/browser-demo.shui
shui build examples/browser-demo.shui -o browser-demo.html

shui validate examples/windows-demo.shui
shui build examples/windows-demo.shui -o windows-demo.cs
dotnet build examples/windows-demo.csproj
```

The browser build emits a standalone HTML file. The Windows build emits C# source plus a .NET 8 Windows Forms project file. Build/run of the generated Windows project requires the .NET SDK and Windows Forms targeting/runtime. The C# generator currently creates the native controls and basic layout; wiring Shay methods to native control events is a later runtime stage.

## Translation pipeline

1. Read the `.shui` source and resolve its selected entry target.
2. Load the matching `.shuil` adapter named by `*#uiContrl`.
3. Validate the adapter's platform, translation method, and UI language declarations.
4. Build a platform-neutral UI description from `drawui` declarations and UI classes.
5. Invoke the adapter's selected translator (for example, JavaScript for Browser).
6. Emit platform output and report unsupported controls or metadata as diagnostics.

The `.shui` source owns app structure and Shay behavior. The `.shuil` adapter owns platform translation. A platform adapter must not silently change shared application semantics.

## Implementation status and compatibility

The old `$app "Name" { ... }` UI format and standalone `shui` frontend are deprecated and must be removed as the integrated implementation lands. `.shui` is a Shay source extension, not a separate language or compiler input. Only `gshai` and `shaii` may process it: `gshai` compiles the Shay program and its UI, while `shaii` interprets the same program and UI. There must be no `shui` executable or independent `shui validate/build` pipeline.

The integrated implementation is staged:
1. Teach the shared Shay frontend to recognize `.shui` and `.shuil` syntax, including the `!%#` UI namespace, without changing ordinary `.shay` grammar.
2. Route `.shui` through `gshai` compilation and `shaii` interpretation; reject standalone UI compilation.
3. Execute UI methods and application state in the existing Shay runtime.
4. Implement a bidirectional event/state bridge from browser JavaScript and Windows C# controls into that runtime.
5. Expand controls, layout, localization, accessibility, and cross-platform conformance tests.

Platform code is an adapter, not a second runtime. Browser JavaScript and Windows C# must send control events to the Shay runtime and receive state updates from it; they must never reimplement Shay methods or become the source of truth for application state. Until each stage lands with tests, the corresponding behavior is not considered implemented.
