# AGENTS.md

This file is the working agreement for anyone — human or AI agent — changing this
repository.

## Project language

- All documentation is written in **English**, including `README.md`,
  `AGENTS.md`, `TASKS.md` and everything under `docs/`.
- Keep user-facing UI text in English.
- Code comments and identifiers are in English.

## Non-negotiables

1. **Never commit ROMs or extracted game assets.** `*.smc`, `*.sfc`, `*.nes` and
   the entire `.research/` directory are ignored. `.research/` contains downloaded
   reference projects, extracted assets, screenshots and one-off analysis scripts.
2. **Keep the editor self-contained.** Third-party code lives under
   `third_party/` and must keep its licence file. Do not add package managers or
   network dependencies at runtime.
3. **Do not break save safety.** Every save path must preserve the ROM size,
   update the SNES checksum, and round-trip through `DKEditorSelftest`.
4. **Keep documentation current.** If behaviour, controls, supported formats or
   limitations change, update `README.md`, `TASKS.md` and the relevant `docs/`
   page in the same change.
5. **No source ROM in the repository.** The editor is only tested against the
   user's local dumps.

## Repository layout

```
CMakeLists.txt            Build for DKEditor.exe and DKEditorSelftest.exe
build.bat                 One-click configure + Release build
run.bat                   Launch the built editor
src/main.cpp              Win32 + DirectX 11 + Dear ImGui bootstrap
src/core/                 ROM, compression, palettes and tile rendering
src/games/                Per-game level parsers and savers
src/games/*Levels.inc     Generated level metadata tables
src/ui/                   Editor UI, canvas, texture helper
src/resources/            Windows icon resource (.rc/.ico) and source art
docs/                     English user and developer documentation
third_party/imgui         Dear ImGui (MIT), vendored
third_party/dkcomp        Kingizor's DKC compression library (MIT), vendored
tools/                    Local screenshot / UI smoke-test scripts
```

The previous analysis workspace is `.research/` and is intentionally ignored.

## Build and test

```bat
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Release --parallel
build\Release\DKEditorSelftest.exe "Donkey Kong Country (U) (V1.2) [!].smc" ^
  "Donkey Kong Country 2 - Diddy's Kong Quest (U) (V1.1) [!].smc" ^
  "Donkey Kong Country 3 - Dixie Kong's Double Trouble (U) [!].smc"
```

The self-test must end with `ALL TESTS PASSED`. It covers:

- DKC1 level load, render, save, reload and checksum.
- DKC2 level load, render, in-place save, recompression, relocation and
  double-save without reload.
- DKC3 level property load, edit, save, reload and checksum.

Do not finish a task with failing tests or a failing build.

## Architecture in one page

- `Rom` owns the headerless 4 MiB image, detects the game/version, reads and
  writes bytes, and fixes the checksum (`src/core/Rom.*`).
- `Compression` wraps `dkcomp`: Big Data (DKC2/DKC3), Small Data (DKC3) and DKC
  CHR (DKC1) (`src/core/Compression.*`).
- `Graphics` decodes SNES 4bpp 8x8 characters, Map32-style metatiles and RGB555
  palettes, and composites level images (`src/core/Graphics.*`).
- `Game` is the polymorphic interface (`src/games/Game.h`). Each game module
  builds a level catalogue, loads a `LoadedLevel` (tilemap, charset, metatiles,
  palette, image) and saves it back.
- `EditorApp` owns the UI state, level selection, undo stacks, painting, tile
  palette and ROM save flow (`src/ui/EditorApp.*`).
- `Texture` uploads RGBA images to DirectX 11 textures.

### DKC1 specifics

- Level catalog data (`src/games/Dkc1Levels.inc`) stores file offset, byte size,
  theme/header, palette offset, tileset index and vertical flag.
- Horizontal stages are column-major (`word = x * 16 + y`), vertical stages are
  row-major (`word = y * 64 + x`).
- Graphics use DKC CHR tilesets, the theme's Map32 base and 586-or-fewer
  metatiles. A few level types need special tileset prefixes (Snow, Ice, Temple,
  Millstone); see `prepareCharset()` in `Dkc1.cpp`.
- Save writes the tilemap in its original order, in place.

### DKC2 specifics

- The level config table is at `0x3D0000`; each pointer is an offset inside that
  bank. Level style records carry the graphics index, palette and tilemap id.
- Big Data streams are decompressed from `0x35BB2E` (map chips), the graphics
  chunk list at `0x3D819A`, and the tilemap pointer table at `0x35BAEF`.
- Submap entries at `0x35BC7E` define world bounds and layout mode. Horizontal
  maps use `x * stride + y`; vertical maps use `x + y * stride`.
- On save, rebuild the **entire** shared tilemap buffer, compress it, and either
  reuse the original slot or relocate to a free zero run. Always update the
  tilemap pointer and the in-memory context so a second save without reload keeps
  working.

### DKC3 specifics

- Stage config entries are listed in `src/games/Dkc3Levels.inc` (U v1.0 layout).
- Only stage settings are edited today. Terrain editing requires locating the
  compressed tilemap and tileset mapping for each config; this is in `TASKS.md`.
- Property edits write 1- or 2-byte fields and single bits, preserving all other
  bytes.

## Coding conventions

- C++20, MSVC first, no exceptions in normal control flow.
- Use `dk::u8/u16/u24/u32`, `Rom::byte/word/longptr` and bounds-checked copies.
- Any new ROM address should be a named constant or a comment explaining what it
  points to. Prefer file offsets computed with `snToFile()` over raw SNES
  addresses.
- Keep game-specific logic in `src/games/`, not in the UI.
- Do not cache ROM pointers across saves if the save can relocate data.
- UI changes must be made inside a window; top-level ImGui widgets create a stray
  "Debug" window.
- All images uploaded to DX11 must be RGBA (`R8G8B8A8_UNORM`), matching
  `packColor()`.

## Adding or changing level metadata

- DKC1 and DKC2 tables are generated from public reverse-engineering references.
  If you regenerate them, store the generator or document the source in
  `docs/CREDITS.md`. Do not commit ROM-derived binary assets.
- If a new level fails to load, mark it unsupported in the catalogue and add a
  note instead of crashing. The UI shows unsupported entries in grey and exposes
  the note as a tooltip.

## Before you finish

1. Build Release.
2. Run `DKEditorSelftest.exe` against all three local ROMs.
3. If you touched the UI, launch the editor and check DKC1, DKC2 and DKC3 layouts.
4. Update `TASKS.md` and any affected documentation.
5. Confirm `git status` does not contain ROMs, `.research/`, `build/` or
   screenshots outside `docs/images/`.
