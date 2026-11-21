# .shui — Hybrid UI File Format (Proposal)

## Goal

A `.shui` file describes an application interface declaratively and uses Shay for behavior. The same document must be usable by more than one renderer:

- **Windows desktop renderer** — native desktop windows and controls.
- **Browser renderer** — semantic HTML/CSS/DOM output with browser event handling.

The file format, parser, UI tree, IDs, properties, and event model are backend-independent. Platform renderers must not define new language syntax.

## Design principles

1. One source file and one UI tree; multiple renderers.
2. Shay remains the behavior language. Do not embed JavaScript or C++ in UI files.
3. Stable IDs connect declarative elements to Shay state and events.
4. Layout is responsive and expressed in logical units, not platform pixels.
5. Accessibility semantics are part of each control's definition.
6. A renderer reports unsupported capabilities as diagnostics; it must not silently discard them.
7. UI files are parsed and validated before a window or browser page is created.

## Proposed syntax (not implemented yet)

This is a design sketch. It is not accepted by `gshai` or `shaii` until the parser/runtime implementation lands.

```shay
$app "Counter" {
    window main {
        title: "Shay Counter"
        size: 480 x 320
        layout: column
        gap: 12

        text heading {
            value: "Counter"
            style: title
        }

        text count {
            bind: counter
        }

        button increment {
            label: "Add one"
            on_click: increment_counter
        }
    }

    logic {
        brew counter = 0

        $declr increment_counter()
            counter = counter + 1
        finish
    }
}
```

The exact grammar and statement integration are subject to parser design review. In particular, the existing `brew` and function syntax must remain consistent with ordinary Shay files.

## Initial element set

- Containers: `window`, `panel`, `row`, `column`, `scroll`.
- Display: `text`, `image`, `separator`.
- Input: `button`, `text_input`, `checkbox`, `dropdown`.
- Layout: logical width/height, min/max constraints, padding, gap, alignment.
- Behavior: `bind`, `on_click`, `on_change`, `on_submit`.

## Runtime model

1. Load and parse the `.shui` document.
2. Validate unique IDs, element properties, bindings, and event handler names.
3. Build an immutable UI description tree.
4. Initialize the Shay logic environment and state bindings.
5. Select a renderer (desktop or browser).
6. Translate UI events into queued Shay callbacks.
7. Apply state changes on the UI thread and schedule a renderer update.

The UI thread owns native/browser UI objects. Shay callbacks must not mutate renderer objects directly from worker threads.

## Renderer contract

Each renderer consumes the same validated UI tree and exposes the same event/state contract.

- Desktop: native window lifecycle, keyboard navigation, focus, accessibility, DPI scaling.
- Browser: semantic HTML, keyboard navigation, focus, accessibility, responsive CSS.
- Shared: stable IDs, value conversion, event ordering, validation, error reporting.

No renderer-specific property may change the meaning of shared Shay code.

## CLI direction

Proposed commands (names are provisional):

- `shay run app.shui --target desktop`
- `shay run app.shui --target web`
- `shay build app.shui --target desktop`
- `shay build app.shui --target web`

The implementation should integrate with the existing `gshai`, `shaii`, and project build flow rather than introduce a competing compiler frontend.

## Delivery gates

1. Grammar and AST for `.shui`; parser-only tests.
2. Shared UI tree and validation; no renderer yet.
3. Browser renderer and a runnable sample.
4. Windows desktop renderer and the same sample.
5. Event callbacks, state binding, and cross-renderer conformance tests.
6. Packaging and project-build integration.

A feature is not considered cross-platform complete until the same sample passes on both targets.


## Implemented parser/browser slice

The initial `shui` command now implements a small, validated subset of this format:

```shay
$app "Counter" {
    window main {
        title: "Counter"
        size: 480 x 320
        layout: column

        text heading {
            value: "Counter"
            style: title
        }

        text count {
            bind: counter
        }

        button increment {
            label: "Add one"
            on_click: increment_counter
        }
    }

    logic {
        brew counter = 0
        $declr increment_counter()
            counter = counter + 1
        finish
    }
}
```

Commands:

- `shui validate app.shui` parses and validates the UI tree.
- `shui build app.shui --target web -o app.html` emits a standalone HTML/CSS page.
- `shui run app.shui --target desktop` launches the native Win32 renderer on Windows.

The browser output includes semantic controls and dispatches `shay:action` and `shay:input` browser events. The native Windows renderer is available through `shui run app.shui --target desktop`; it creates Win32 controls and currently sends button-action names to the debugger. The logic block is preserved as tokens, but neither renderer executes Shay logic or synchronizes bound state yet. That shared event/runtime bridge is the next integration milestone.
