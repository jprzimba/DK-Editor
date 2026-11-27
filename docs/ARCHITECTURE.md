# Architecture

DK Editor is a native Windows desktop application. The UI is Dear ImGui rendered
through DirectX 11; the ROM, compression and level logic are independent of the UI
and are also exercised by the headless self-test.

## High-level data flow

```
ROM file
  -> Rom::load()                 header detection, game/version detection
  -> createGame(kind)            Dkc1Game / Dkc2Game / Dkc3Game
  -> Game::init()                level catalogue
  -> Game::loadLevel(id)         tilemap + charset + metatiles + palette
  -> LoadedLevel
       -> buildLevelImage()      RGBA pixels for the canvas
       -> uploadAtlasTexture()   RGBA thumbnails for the tile palette
  -> user edits LoadedLevel::tiles
  -> Game::saveLevel()           writes the game's native representation
  -> Rom::save()                 checksum + file write
```

`EditorApp` owns the active `Rom`, the active `Game`, the selected
`LoadedLevel`, the undo/redo stacks and the DirectX textures. Nothing in
`src/core/` or `src/games/` depends on ImGui or DirectX.

## Modules

### `src/core/Rom.*`

- Reads the file, strips an optional 512-byte copier header, and keeps the
  headerless 4 MiB image.
- Detects the game from the HiROM title at `0xFFC0`: DKC2 contains "DIDDY",
  DKC3 contains "COUNTRY 3", otherwise DKC1.
- Provides bounds-checked `byte`, `word`, `longptr`, `copyOut` and `copyIn`.
- `fixChecksum()` treats the four checksum bytes as `FF FF 00 00`, sums the image
  and writes the complement/checksum pair at `0xFFDC`.

### `src/core/Compression.*`

A thin C++ wrapper around the vendored `dkcomp` C library:

- `BigData` for DKC2/DKC3 tilesets, map chips and tilemaps.
- `SmallData` for DKC3 tilemaps (reserved for the DKC3 terrain milestone).
- `DkcChr` for DKC1 tilesets.

The wrapper uses `extern "C"` so the C symbols link correctly from C++.

### `src/core/Graphics.*`

- `loadPalette()` reads 8 rows of 16 RGB555 colours. Colour 0 of each row is
  marked transparent (alpha 0).
- `decodeCharset()` turns a 32-byte SNES 4bpp planar tile into 64 palette
  indices. Byte layout is:
  `[row0 plane0, plane1] ... [row7 plane0, plane1]`,
  `[row0 plane2, plane3] ... [row7 plane2, plane3]`.
- `decodeMetatiles()` reads 4x4 character references. Each reference is:
  bits 0-9 character, bits 10-12 palette row, bit 14 horizontal flip, bit 15
  vertical flip.
- `renderMetatile()` composites one 32x32 tile into an RGBA image. Index-0 pixels
  are skipped so the level background shows through.
- `buildLevelImage()` composites the whole tilemap. Tilemap words use the low
  10 bits (DKC1) or 12 bits (DKC2) as the metatile index and bits 14/15 as the
  whole-tile flips.

### `src/games/Game.h`

Common value types:

- `LevelEntry`: catalogue row (id, name, group, supported/visual flags, note).
- `LoadedLevel`: editable tilemap (`row-major y*width+x`), metatiles, charset,
  palette, RGBA image, optional generic `LevelProperty` list, and a
  game-specific `LevelContext`.
- `Game`: polymorphic interface used by the UI and self-test.

`LevelContext` is a small polymorphic payload. DKC2 stores the shared tilemap
buffer, map id, submap region, stride and compressed-slot information there.
DKC1 needs no extra context because its level offset is fixed metadata.

### `src/games/Dkc1.*`

- Catalogue generated into `Dkc1Levels.inc` (65 entries).
- Theme/header index selects the Map32 base and metatile count.
- `resolveTileset()` mirrors the game's DKC1 tileset index routine using tables at
  `$B9A994`.
- Horizontal maps are column-major (`x*16+y`); vertical maps are row-major
  (`y*64+x`).
- Save writes the same order in place.
- Special tileset prefixes are applied for Snow/Ice/Temple/Millstone-style
  codes. This area is a known visual-verification item in `TASKS.md`.

### `src/games/Dkc2.*`

- Level config pointer table: `0x3D0000` (offset into bank `$3D`).
- Config: `type`, optional bonus byte, style offset, submap index.
- Style: graphics index at `+13`, default palette at `+6`, tilemap id at `+18`.
- The base palette is overlaid with the `routine1` sub-palette used by that
  level style, matching the game's palette setup.
- Map chips: 3-byte pointer at `0x35BB2E + mapId*3`.
- Graphics: pointer list at `0x3D819A`; entries are 7 bytes
  (`bank, addr, op, size`). `0xA000` is Big Data, `0x2000` is raw.
- Tilemap: type at `0x35BC54`, submap list at `0x35BC7E`, pointer at
  `0x35BAEF`.
- The decompressed tilemap can contain multiple submaps. A level reads only its
  region using the world bounds and stride.
- Save recompresses the whole shared tilemap, writes it to the original slot if
  it fits, otherwise relocates it to the highest suitable zero-filled run, then
  always updates the tilemap pointer and the `Dkc2Ctx` cache.

### `src/games/Dkc3.*`

- Catalogue generated from the public `dkc3-level-properties` data (132 entries).
- Each entry points at a 40-byte level config record.
- The editor exposes music, start/midpoint/bonus/warp parameters and coordinates,
  parrot and Koin flags. Values are written directly, preserving neighbouring
  fields.
- Terrain rendering is intentionally absent in this build.

### `src/ui/EditorApp.*`

- Owns the main window layout: level list, canvas and tool panel.
- `drawCanvas()` maps mouse coordinates through zoom/pan to tile coordinates.
- Painting rebuilds only the affected 32x32 pixels and uploads that region.
- The tile palette is a single RGBA atlas built from the current level's
  metatiles and palette.
- Undo stores a full tilemap snapshot before a stroke; redo stores the previous
  state. This is simple and robust for the level sizes involved.

### `src/ui/Texture.*`

- Creates `DXGI_FORMAT_R8G8B8A8_UNORM` textures and shader resource views.
- Supports full uploads and partial uploads (`uploadRegion`) for fast painting.

## Save safety

1. All edits mutate the in-memory `Rom` or `LoadedLevel`.
2. `Game::saveLevel()` writes exactly the game's native representation.
3. `Rom::save()` fixes the checksum and writes the whole image atomically from the
   editor's point of view (a normal file truncate/write).
4. The self-test reloads every saved ROM and checks the checksum.

The editor never expands the ROM. DKC2 relocation only uses a zero-filled run and
updates the game's pointer table. Keep a backup of a ROM before large edits.

## Threading

Everything runs on the UI thread. Level loading, decompression and rendering are
fast enough for the supported level sizes; no worker threads are used.

## Dependencies

- Dear ImGui 1.91.9b, MIT.
- Kingizor `dkcomp`, MIT.
- Windows SDK (DirectX 11, Win32, comdlg32, shell32).

No runtime downloads, no package manager, no network access.
