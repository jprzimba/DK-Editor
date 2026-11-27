# Testing

## Automated self-test

`DKEditorSelftest.exe` is a headless console program that exercises the ROM,
game, rendering and save layers. It does not open a window.

Run it from the repository root with the three ROMs:

```bat
build\Release\DKEditorSelftest.exe ^
  "Donkey Kong Country (U) (V1.2) [!].smc" ^
  "Donkey Kong Country 2 - Diddy's Kong Quest (U) (V1.1) [!].smc" ^
  "Donkey Kong Country 3 - Dixie Kong's Double Trouble (U) [!].smc"
```

It can also scan the current directory:

```bat
build\Release\DKEditorSelftest.exe
```

### What it checks

DKC1:

- Detect the game and version.
- Find and load Jungle Hijinxs.
- 168x16 dimensions.
- Metatiles and tileset present.
- Rendered image has non-background pixels.
- Every supported DKC1 level loads without an error.
- Edit a tile, save, reload and compare.
- Write a ROM copy, reopen it and compare the checksum.

DKC2:

- Detect the game and version.
- Find and load Pirate Panic.
- 223x16 dimensions.
- Map chips and tileset present.
- Rendered image has non-background pixels.
- Every supported DKC2 level loads without an error.
- Edit and save in place.
- Stress the compressor with many changed tiles; verify the tilemap can be
  relocated and the pointer table updated.
- Save a second time without reloading and verify the relocated buffer is still
  used.
- Write a ROM copy, reopen it and compare the checksum.

DKC3:

- Detect the game and version.
- Find and load Lakeside Limbo properties.
- Edit the music byte, save, reload and verify.
- Write a ROM copy, reopen it and compare the checksum.

The last line must be:

```
ALL TESTS PASSED (0 failures)
```

The self-test writes temporary `.smc` files in the current directory and removes
them on success. Do not leave them behind in commits.

## Manual UI checklist

Run the editor with each ROM and verify:

### DKC1

- [ ] Window opens with the level list, canvas and tool panel visible.
- [ ] **Jungle Hijinxs** is selected by default.
- [ ] Terrain is rendered with jungle graphics, not coloured blocks.
- [ ] The tile palette shows jungle metatiles.
- [ ] Hovering the canvas updates the cursor X/Y and tile value.
- [ ] Left-drag paints the selected tile.
- [ ] Right-drag erases.
- [ ] Alt+left picks the tile under the cursor and updates the flip checkboxes.
- [ ] Undo and redo restore the stroke.
- [ ] **Save** succeeds and the status line reports the checksum.
- [ ] Reopening the ROM shows the saved edits.
- [ ] Vertical stages (Coral Capers, Clam City, Croctopus Chase, Poison Pond,
      Slipslide Ride) render with the correct width/height.

### DKC2

- [ ] **Pirate Panic** is selected by default.
- [ ] Ship-deck graphics render correctly.
- [ ] The level list groups Levels and Bonuses/Sub-rooms.
- [ ] Unsupported entries are grey and show a tooltip.
- [ ] A small edit saves in place.
- [ ] A large edit saves via relocation and the ROM still loads.
- [ ] Saving twice without reloading keeps the edit.

### DKC3

- [ ] **Lakeside Limbo** is selected.
- [ ] The canvas explains that terrain editing is not in this build.
- [ ] Stage settings show music, flags and coordinates.
- [ ] Editing a value marks the ROM as modified.
- [ ] Saving writes the value and fixes the checksum.
- [ ] Reloading the level shows the new value.

### Cross-cutting

- [ ] Search filters levels by name and id.
- [ ] The selected level scrolls into view.
- [ ] `Ctrl+O`, `Ctrl+S`, `Ctrl+Z`, `Ctrl+Y` and `F` work.
- [ ] The window can be resized and maximized without losing the layout.
- [ ] Closing and reopening works repeatedly.

## Regression checklist for contributors

Before committing:

1. Build Release without warnings that indicate real problems.
2. Run the full self-test.
3. Run the manual checklist for any game whose code or UI was touched.
4. Confirm `git status` is clean of ROMs, `.research/`, build outputs and
   screenshots outside `docs/images/`.
5. Update `TASKS.md` if a limitation changed.
