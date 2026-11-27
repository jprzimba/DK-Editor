# TASKS.md

Status and roadmap for DK Editor. All items are in English and are kept in sync
with the code.

## Current milestone: first testable build

### Completed

- [x] C++20 / CMake / Win32 + DirectX 11 + Dear ImGui application shell.
- [x] Headerless and 512-byte-headered 4 MiB HiROM loading.
- [x] Game and version auto-detection for DKC1, DKC2 and DKC3.
- [x] SNES checksum calculation and repair on save.
- [x] Vendored MIT `dkcomp` (Big Data, Small Data, DKC CHR).
- [x] Common 8x8 4bpp decoder, palette decoder, Map32 metatile decoder and
      RGBA level compositor.
- [x] DKC1 level catalogue (65 stages and sub-rooms).
- [x] DKC1 uncompressed tilemap load/save, including vertical stages.
- [x] DKC1 DKC CHR tilesets, Map32 metatiles and palettes.
- [x] DKC2 level catalogue (192 entries) and config/style/submap parsing.
- [x] DKC2 Big Data tilesets, map chips and tilemaps.
- [x] DKC2 recompression, free-space relocation and pointer-table update.
- [x] DKC2 double-save without reload after relocation.
- [x] DKC3 stage-settings editing.
- [x] DKC3 background preview: real stage art (tilemap + tileset + FGBG
      palette) for lakeside, mill, forest, riverbank, mountain, pipe and cave
      stages, and a themed gradient for the remaining themes.
- [x] Level canvas with fit, zoom, pan, grid, hover and painting.
- [x] Native horizontal and vertical canvas scrollbars (no need to zoom out).
- [x] DKC2 Layer 2 backgrounds: Ship Deck ocean, Ship Mast clouds, Haunted Hall,
      ship hold, brambles, honeycomb, forest, carnival and lava cave themes are
      decoded from Big Data and composited behind the terrain.
- [x] DKC1 themed sky gradients per level type as an interim backdrop.
- [x] Tile palette with real metatile thumbnails and selection.
- [x] Paint, pick and erase tools, horizontal/vertical flips.
- [x] Lunar Magic style rectangular area selection with fill, clear, copy and
      paste, plus `Ctrl+A`, `Ctrl+C`, `Ctrl+V`, `Delete` and `Esc` shortcuts.
- [x] Item bar under the toolbar with the collectables of each game rendered as
      icons (bananas, balloons, KONG letters, barrels, coins, cogs, tokens).
- [x] No tile is pre-selected when a level opens; paint waits for a palette pick.
- [x] Fit toggles between the whole level and the previous zoom.
- [x] Undo/redo snapshots per level.
- [x] Targeted 32x32 partial texture upload while painting.
- [x] Searchable, grouped level list with unsupported-entry notes.
- [x] Save / Save As / Open dialogs and keyboard shortcuts.
- [x] Headless self-test for all three games and both save paths.
- [x] English `README.md`, `AGENTS.md`, `TASKS.md` and `docs/`.
- [x] `.gitignore` excludes ROMs, `.research/`, build outputs and test images.

### Known limitations

- [ ] DKC3 terrain editing is not implemented; only stage settings are editable.
- [ ] DKC3 background art is still missing for snow, factory and reef stages;
      those show a themed gradient for now.
- [ ] DKC2 levels that share a tilemap are edited together. The UI does not yet
      warn before saving a shared map.
- [ ] DKC1/DKC2 object, sprite, banana, camera and entrance editing is not
      implemented.
- [ ] No PNG export of the rendered level.
- [ ] No palette editor.
- [ ] No level resizing, layer editing or per-level background editing.
- [ ] DKC1 backgrounds are still themed gradients (sky colours per level type).
      The real DKC1 Layer 3 artwork is not decoded yet.
- [ ] DKC3 canvases show a background preview only; there is no terrain or
      editing surface yet.
- [ ] DKC2 background selection is a per-level table (from level ids and stage
      themes); a few exotic levels still fall back to the gradient.
- [ ] DKC2 `K rool cabin` and similar entries without map chips are marked
      unsupported.
- [ ] DKC1 `Gangplank Galleon` uses a special raw tileset path; verify visually
      against a reference in the next build.
- [ ] DKC1 Snow/Ice/Temple tileset prefixes are implemented from public notes but
      have not been compared frame-by-frame with the game.
- [ ] Undo history is per level and is discarded when switching levels.
- [ ] No unsaved-changes prompt when closing the application.

## Next milestone: DKC3 terrain editing

### Research notes (2026-11)

- Terrain is stored in 18 "LevelData" bundles (`CaveLevels1/2`, `PierLevels`,
  `ForestLevels`, ...) in banks `$E0`-`$E8`; each bundle is Big Data compressed
  and holds the maps for every stage that shares that tileset.
- Terrain maps are **row-major metatile grids** (32x32 metatiles). `CaveLevels1`
  decodes to 16896 words = 4 blocks of 96x44 metatiles, each ending in a fully
  empty right-most column (a clean level boundary). Rendering a block with
  `M32_Layer1_Level_Cave` + `GFX_Layer1_CaveLevels` produces a coherent cave.
- Stage config (bank `$FD`, `level_config_table`) is now fully documented:
  `dw type, db music, dw tilesetConfig, db layout, db palette, db ?, dw camera,
   db gimmick, dw ?, dw flags, entrance list, destination list`. The layout
  byte matches the `Horizontal/TallHorizontal/Vertical/Square/...` enum.
- Each bundle is preceded by a 3-byte-pointer header to its `M32_*` start/end
  and its `GFX_*` start (encoded as `db lo, db hi, db bank|0x40`).
- Layer maps are columns of 16 or 32 metatiles (`Horizontal` / `TallHorizontal`)
  or row-major with 32/64/96 metatiles per row (`Vertical`, `NarrowVertical`,
  `Square`, `ShipHold`), per `Dkc3VideoLevelLayout*` in DKC3Recomp.
- A stage's map offset inside its bundle, the layout code, the palette and the
  background ids are still unidentified.
- Layer 2/3 tilemaps use the DKC3-specific tilemap compression (Karaage Lab
  "Tilemap compression format"); `decompressTilemapDkc3()` implements it.
- Background previews use the level's `FGBG_*` palette (palette id at config
  `+6`, pointer table `$FD54E9`) and the theme's layer-2 tilemap + tileset.
  The tileset is loaded at a VRAM character base that is recovered from the
  highest tile index used by the map.
- Still open: the per-stage map offset/layout inside each bundle, the remaining
  theme asset pairs (snow, factory, mountain, reef, mill) and save support.

### Tasks

- [ ] Identify the per-stage map offset/layout inside each LevelData bundle.
- [ ] Identify the stage palette and background ids.
- [x] Port the DKC3 tilemap decompressor to `src/core/Compression.*`.
- [ ] Decode and render Layer 1 (terrain) with the theme M32 and tileset.
- [ ] Implement save: recompress the bundle, relocate if needed, update pointers.
- [ ] Add DKC3 vertical/special layout support.
- [ ] Extend the self-test with DKC3 terrain round-trips.

## Following milestones

### Object and sprite editing

- [ ] Parse DKC1/DKC2/DKC3 sprite placement tables.
- [ ] Render sprite markers on the canvas.
- [ ] Add, move and delete sprites.
- [ ] Edit sprite parameters, loading zones and groups.
- [ ] Save sprite data with pointer/size validation.

### Level properties

- [ ] DKC1 level type, music, camera and entrance tables.
- [ ] DKC2 music, camera, entrances and exits.
- [ ] Show and edit level names (where safe).

### Quality of life

- [ ] PNG export and import of tilemaps.
- [ ] Palette swap preview and palette editing.
- [ ] Minimap and level overview.
- [ ] Bookmarks and recently opened ROMs.
- [ ] Unsaved-change confirmation.
- [ ] Per-level undo persistence.
- [ ] Shared-tilemap warning for DKC2.
- [ ] Backups (`.bak`) before the first save of a session.
- [ ] Dark/light theme switch.

### Robustness

- [ ] Validate all pointers before reading.
- [ ] Fuzz the tilemap renderer with every supported level.
- [ ] Add a CLI `--info` mode for ROM diagnostics.
- [ ] Add integration tests that diff saved ROMs against the originals.
- [ ] Verify DKC1 special tilesets and DKC2 shared-map behaviour in an emulator.

## How to update this file

- Move completed items to **Completed** and add a short note when behaviour
  changes.
- Add new known limitations instead of hiding them.
- Keep the file in English.
