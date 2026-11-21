# .shuil — Shay UI platform library

A `.shuil` file describes the platform adapter and translation pipeline used by a `.shui` application. The application chooses a platform; the library defines how that platform is controlled and which translator is used.

## Initial syntax

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

## Directives

- `#*$ContrlDefine <browser>` selects the target platform. The initial example uses `browser`; `windows` is reserved as the native desktop target.
- `#*!MethdUi <javasTrans>` selects the UI translation method. `javasTrans` identifies the Shay-to-JavaScript translation pipeline.
- `#*!LangUI <US>` selects the human language/locale used by UI text. It is not a programming language or translator selection.
- `#*!classmethdDeclr !m#Name` declares a translation class. The `!m#` prefix is required for this class-name form.
- `$uiset 0` stores the UI-library mode flag.
- `$declrmthd Name` declares a translation method.
- `$mthdset 1` stores the method execution flag.
- `#*$javasport [uicontrol.js]` selects Shay's integrated JavaScript translator.
- `#*$javasport ["custom.js"]` selects a user-provided external translator file.
- `#*$doJavaScript` marks the JavaScript translation stage for execution.

The spelling and capitalization above are intentional and case-sensitive.

## Translator selection

The unquoted built-in name `uicontrol.js` is reserved for the integrated translator. External translator paths must be quoted. The current implementation parses and validates this selection; it does **not** execute JavaScript or load external translator code yet. External translators will need a defined API, path resolution rules, permissions, and an isolated execution model before they can run.

## Current implementation

`shui validate-library path/to/library.shuil` validates the initial configuration grammar and reports the selected platform, translation method, UI language, and method count. It does not yet connect a `.shuil` file to a `.shui` application. The existing `.shui` parser and renderers remain on their earlier application grammar while the new application syntax is designed and implemented in a separate milestone.
