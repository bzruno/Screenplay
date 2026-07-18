# Screenplay Editor

Professional screenplay editor in C++17 — Final Draft–style layout engine with a minimalist Material Design 3 UI.

## Architecture

```
Script (semantic model)
  └── LayoutEngine (pure fn) → PageList
        └── Renderer (stateless) → Screen
EditorController (input FSM) → mutates Script
UndoStack (Script snapshots only)
AutocompleteSystem (Trie, context-aware)
IFontMetrics (FreeType backend — no heuristics)
```

## Dependencies

| Library   | Purpose                      |
|-----------|------------------------------|
| Qt 6      | Window, input, QPainter      |
| FreeType  | Real font metrics & rendering|

## Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

### macOS (Homebrew)
```bash
brew install qt freetype
cmake -B build -DCMAKE_PREFIX_PATH=$(brew --prefix qt)
cmake --build build
```

### Ubuntu / Debian
```bash
sudo apt install qt6-base-dev libfreetype-dev
cmake -B build
cmake --build build
```

### Windows (vcpkg)
```powershell
vcpkg install qt6 freetype
cmake -B build -DCMAKE_TOOLCHAIN_FILE=...vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
```

## Keyboard Shortcuts

| Key          | Action                                 |
|--------------|----------------------------------------|
| Enter        | Advance block type (Scene→Action→etc) |
| Tab          | Cycle block type forward               |
| Shift+Tab    | Cycle block type backward              |
| Ctrl+Z       | Undo                                   |
| Ctrl+Shift+Z | Redo                                   |
| Ctrl+S       | Save (JSON)                            |

## Export Formats

- **Fountain** (.fountain) — plain text, industry standard
- **FDX** (.fdx) — Final Draft XML
- **JSON** (.spl) — internal lossless format

## WGA Standard Margins (US Letter)

| Element      | Left indent | Right indent |
|--------------|-------------|--------------|
| Scene Heading| 0           | 0            |
| Action       | 0           | 0            |
| Character    | ~2 in       | ~1 in        |
| Parenthetical| ~1.5 in     | ~1.5 in      |
| Dialogue     | ~1.5 in     | ~1.5 in      |
| Transition   | 0 (R-align) | 0            |

## Font

Place `CourierPrime-Regular.ttf` (or any Courier variant) in `fonts/`.  
Download free: https://quoteunquoteapps.com/courierprime/
