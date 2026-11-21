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
| `<:onclick=method:>` | Binds a browser control click to a no-argument Shay method declared in the UI class. The call runs in the compiled Shay process. |
| `<:bind=variable:>` | Binds a browser input/checkbox to a named Shay runtime value; the value is sent to the runtime and reflected back after events. |
| `<:intercative=yes:>` | Legacy metadata spelling retained for compatibility; it does not itself choose an event handler. |
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

### Build and run embedded Shay logic

Pass `.shui` directly to the existing Shay tools; there is no separate UI executable:

```sh
./build/shaii --check examples/browser-demo.shui
./build/shaii examples/browser-demo.shui
./build/gshai -o build/browser-demo examples/browser-demo.shui

./build/shaii --check examples/windows-demo.shui
./build/gshai -o build/windows-demo examples/windows-demo.shui
```

Both tools accept `.shui` directly. The shared frontend validates the UI envelope and referenced `.shuil` adapter, strips only the platform declaration layer, and sends embedded Shay statements through the same parser/runtime or native compiler path used by `.shay` files. `gshai -o` emits browser HTML or a Windows Forms C# source/project and compiles a loopback HTTP event/state bridge into the executable. For Browser, start the compiled executable, then open the generated HTML. For Windows, run `dotnet build <app>.csproj` and launch the generated `*-ui.exe`; it starts the compiled Shay executable beside it. The bridge binds only to `127.0.0.1:18765` and requires the per-build token embedded in the generated UI.

## Translation pipeline

1. Read the `.shui` source and resolve its selected entry target.
2. Load the matching `.shuil` adapter named by `*#uiContrl`.
3. Validate the adapter's platform, translation method, and UI language declarations.
4. Build a platform-neutral UI description from `drawui` declarations and UI classes.
5. Invoke the adapter's selected translator (for example, JavaScript for Browser).
6. Emit platform output and report unsupported controls or metadata as diagnostics.

The `.shui` source owns app structure and Shay behavior. The `.shuil` adapter owns platform translation. A platform adapter must not silently change shared application semantics.

## Implementation status and compatibility

The old `$app "Name" { ... }` UI format and standalone `shui` frontend have been removed. There is no `shui` executable or independent `shui validate/build` pipeline. `.shui` is accepted directly by `gshai` and `shaii`; `.shuil` adapters are validated while loading the UI source.

Implemented:
1. The shared Shay toolchain accepts `.shui` as an input extension.
2. The `!%#` browser/Windows app envelope, UI class declarations, class references, draw declarations, and adapter configuration are validated.
3. Embedded Shay statements and UI class methods are routed through the existing compiler/interpreter frontend and runtime.
4. Make, CMake, installation, and regression tests no longer build or invoke a standalone `shui` program.

Implemented:
5. Browser DOM generation and browser click/input requests through an authenticated loopback bridge into the compiled Shay runtime.
6. Browser state snapshots and updates for controls using `<:bind=variable:>`.
7. Browser bridge regression tests that start the compiled runtime, invoke a Shay method over HTTP, and reject an invalid token.

Not implemented yet:
1. Native Windows Forms event dispatch and state synchronization through the runtime bridge.
2. Automatic browser launch and a unified UI lifecycle manager.

Platform code remains an adapter, not a second runtime. Browser and Windows Forms event/state bridges are generated from the same `.shui` bindings. The compiled Shay process owns behavior and state on both platforms. Browser launch remains a manual step; the Windows Forms project is built with `dotnet build`.
