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
- [x] DKC3 level catalogue (132 entries) and stage-settings editing.
- [x] Level canvas with fit, zoom, pan, grid, hover and painting.
- [x] Native horizontal and vertical canvas scrollbars (no need to zoom out).
- [x] DKC2 Layer 2 backgrounds: Ship Deck ocean, Ship Mast clouds, Haunted Hall,
      ship hold, brambles, honeycomb, forest, carnival and lava cave themes are
      decoded from Big Data and composited behind the terrain.
- [x] DKC1 themed sky gradients per level type as an interim backdrop.
- [x] Tile palette with real metatile thumbnails and selection.
- [x] Paint, pick and erase tools, horizontal/vertical flips.
- [x] Undo/redo snapshots per level.
- [x] Targeted 32x32 partial texture upload while painting.
- [x] Searchable, grouped level list with unsupported-entry notes.
- [x] Save / Save As / Open dialogs and keyboard shortcuts.
- [x] Headless self-test for all three games and both save paths.
- [x] English `README.md`, `AGENTS.md`, `TASKS.md` and `docs/`.
- [x] `.gitignore` excludes ROMs, `.research/`, build outputs and test images.

### Known limitations

- [ ] DKC3 terrain editing is not implemented; only stage settings are editable.
- [ ] DKC2 levels that share a tilemap are edited together. The UI does not yet
      warn before saving a shared map.
- [ ] DKC1/DKC2 object, sprite, banana, camera and entrance editing is not
      implemented.
- [ ] No PNG export of the rendered level.
- [ ] No palette editor.
- [ ] No level resizing, layer editing or per-level background editing.
- [ ] DKC1 backgrounds are still themed gradients (sky colours per level type).
      The real DKC1 Layer 3 artwork is not decoded yet.
- [ ] DKC3 previews show stage settings only; no terrain or backgrounds.
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

- [ ] Locate the DKC3 level-to-tilemap mapping.
- [ ] Decode DKC3 Small Data tilemaps.
- [ ] Identify the tileset and map-chip assets per stage theme.
- [ ] Render DKC3 terrain with real graphics.
- [ ] Implement recompression and safe relocation for DKC3 tilemaps.
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
