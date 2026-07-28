# Screenplay Editor

Professional screenplay editor in C++17 / Qt 6 — a WGA-standard, Final Draft–style
layout engine with a flat, distraction-free interface (Light / Dark / Chamber themes).

## Highlights

- **WGA-standard elements** — Scene Heading, Action, Character, Parenthetical,
  Dialogue, Transition, Shot, General, Act Break — measured with real Courier
  Prime metrics via FreeType (no width heuristics).
- **Two page profiles** — US Industry Standard (Letter) and International (A4).
- **SmartType autocomplete** — context-aware scene headings, character cues and
  transitions.
- **Character-level styling** — bold / italic / underline runs; per-block
  alignment overrides for Action and Transition.
- **Production tools** — revision passes, locked scene numbers (1A / A1),
  OMITTED scenes, forced page breaks, dual dialogue, and reports.
- **Navigation & review** — Scenes navigator, Script Breakdown panel (totals,
  scenes, characters, dialogue), margin Notes, Go to Scene / Page.
- **Writing comfort** — spell check, title-page (cover) editor, Focus Mode,
  full screen, English / Portuguese UI.
- **Bundled Courier Prime** (SIL OFL) — no system font install needed.

## Import / Export

- **Import:** FDX (Final Draft), Fountain
- **Export:** PDF (WGA layout), Fountain, FDX, RTF, Text
- **Native format:** `.spl` (lossless JSON), open supports `.spl` / `.fountain` / `.fdx`

## Architecture

The core (model → layout → render → controller) is **Qt-free and unit-tested**;
Qt is only the shell (window, input, QPainter, PDF/print).

```
Script (semantic model)
  └── LayoutEngine (pure fn) → PageList
        └── Renderer (stateless) → screen / PDF
EditorController (input FSM) → mutates Script (+ undo snapshots)
Autocomplete (Trie, context-aware)
IFontMetrics (FreeType backend — real metrics, no heuristics)
```

### Source layout

```
src/
  main.cpp                 entry point (main())
  screenplay_canvas.hpp    ScreenplayCanvas — editing surface + page painting
  main_window.hpp          MainWindow — chrome, docks, file/menu commands
  ui/main_menu.cpp         menu + floating-toolbar construction
  layout/  editor/  render/  io/  parsing/  reports/  production/
  stats/   spellcheck/ database/ config/  ui/
```

The two former "monster classes" (`ScreenplayCanvas`, `MainWindow`) have been
split into their own headers, and large methods are being peeled out into
dedicated translation units (e.g. `ui/main_menu.cpp`).

## Dependencies

| Library                              | Purpose                          |
|--------------------------------------|----------------------------------|
| Qt 6 (Core, Widgets, PrintSupport)   | Window, input, QPainter, PDF/print |
| FreeType                             | Real font metrics & rendering    |
| Qt 6 Svg (optional)                  | SVG cover artwork                |

## Build (Windows — primary platform)

Developed and tested on Windows 11 with Qt 6 via **vcpkg** and **MSVC**.

```powershell
vcpkg install qt6 freetype
# optional, for SVG cover artwork:
vcpkg install qtsvg:x64-windows

cmake -B build -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
```

The build deploys the required Qt runtime/plugins and copies Courier Prime next
to the executable automatically (`windeployqt` + a post-build step), so the app
runs from `build/` with no extra setup.

> The layout/editor **core** is portable C++17, but the current app shell uses
> Windows-specific window integration (frameless title bar, native resize).
> macOS / Linux builds are not currently maintained.

## Keyboard shortcuts (selection)

| Key            | Action                                   |
|----------------|------------------------------------------|
| Enter          | New element + SmartType where applicable |
| Tab            | Accept SmartType / cycle block type      |
| Ctrl+1 … Ctrl+9| Set element type (Scene … Act Break)     |
| Ctrl+B / I / U | Bold / Italic / Underline                |
| Ctrl+Shift+L/E/R | Align left / center / right            |
| Ctrl+D         | Dual dialogue                            |
| Ctrl+Return    | Page break before                        |
| Ctrl+Z / Ctrl+Shift+Z | Undo / Redo                       |
| Ctrl+S / Ctrl+Shift+S | Save / Save As (`.spl`)           |
| Ctrl+F / Ctrl+H| Find / Replace                           |
| Ctrl+G / Ctrl+Shift+G | Go to Scene / Page                |
| Ctrl+Shift+B   | Script Breakdown panel                   |
| Ctrl+Alt+M / Ctrl+Alt+N | Note on block / All notes       |
| Ctrl+Shift+F   | Focus Mode                               |
| F11            | Full screen                              |

## WGA standard margins (US Letter)

| Element       | Left indent | Right indent |
|---------------|-------------|--------------|
| Scene Heading | 0           | 0            |
| Action        | 0           | 0            |
| Character     | ~2 in       | ~1 in        |
| Parenthetical | ~1.5 in     | ~1.5 in      |
| Dialogue      | ~1.5 in     | ~1.5 in      |
| Transition    | 0 (R-align) | 0            |

## Fonts

Courier Prime ships in `fonts/` under the SIL Open Font License. The layout
engine measures with it and the PDF export embeds it, so nothing extra is
required. Free download / source: <https://quoteunquoteapps.com/courierprime/>
