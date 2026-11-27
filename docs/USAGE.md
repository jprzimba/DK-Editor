# User Manual

## Opening a ROM

1. Start `DKEditor.exe`.
2. Choose **File > Open ROM...** (`Ctrl+O`).
3. Select a clean 4 MiB `.smc`/`.sfc` dump of:
   - Donkey Kong Country (US v1.2)
   - Donkey Kong Country 2: Diddy's Kong Quest (US v1.1)
   - Donkey Kong Country 3: Dixie Kong's Double Trouble (US v1.0)

A 512-byte copier header is detected and removed automatically. Saved files are
written headerless (without the copier header). The title bar of the level panel
shows the detected game and version.

You can also pass a ROM on the command line:

```bat
DKEditor.exe "Donkey Kong Country (U) (V1.2) [!].smc"
```

An optional second argument selects a level by its two-digit id (hex or decimal),
for example `DKEditor.exe rom.smc 0x2F` opens Slipslide Ride.

## Window layout

- **Left panel** - ROM information, search box and grouped level list.
- **Centre** - level canvas.
- **Right panel** - tools, selected tile, tile palette, level information and
  game notes.
- **Bottom** - status line, cursor tile, modified state and zoom.

Levels that cannot be edited in this build are shown in grey. Hover them to read
the reason.

The canvas shows the level's own graphics with the scenery drawn behind them.
DKC2 stages use their real Layer 2 background; DKC1 stages use a sky gradient
matching the level type. The scenery is decoration only: it is never edited or
saved over, and the **Background** checkbox hides it when you want to see the
terrain map alone.

## Navigating the level

| Action | Input |
| --- | --- |
| Zoom in/out at cursor | Mouse wheel |
| Scroll horizontally/vertically | Bottom/right scrollbar of the canvas |
| Pan | Middle-drag, or Space + left-drag |
| Fit the whole level | `F` or **Fit** |
| Toggle the 32x32 grid | **Grid** checkbox |
| Toggle the scenery | **Background** checkbox |
| Hover a tile | Move the pointer; the status line shows tile X/Y and value |

The zoom range is 5% to 400%. When a level is wider or taller than the canvas,
native scrollbars appear at the bottom and right edges; you can drag them to move
through the level without changing the zoom. The default view is 100% (or
height-fit for very tall levels). **Fit** shows the entire level.

## Selecting a tile

The **Tile palette** on the right shows every 32x32 metatile for the current
level, rendered with the level's own tileset and palette.

- Click a cell to select it.
- The large preview under **Selected tile** shows the active tile.
- Hovering a cell outlines it.
- The blue border marks the current selection.

## Painting

| Tool | Behaviour |
| --- | --- |
| **Paint** | Left click/drag writes the selected tile. |
| **Pick** | Left click reads the tile under the cursor. |
| **Erase** | Left click/drag writes tile 0 (empty). |

Right click/drag always erases, regardless of the active tool. Alt + left click
always picks.

**Flip horizontally (X)** sets bit 14 on painted tiles. **Flip vertically (Y)**
sets bit 15. The picker updates both checkboxes from the tile under the cursor.

Painting only rebuilds the affected 32x32 pixels, so large levels stay responsive.

## Undo and redo

- `Ctrl+Z` undoes the last stroke.
- `Ctrl+Y` redoes it.
- The toolbar buttons do the same.
- History is kept per level and is cleared when you switch levels.

A stroke starts on mouse-down and ends when the button is released, so dragging
across a row is one undo step.

## Saving

- **File > Save ROM** (`Ctrl+S`) writes to the same file.
- **File > Save ROM As...** (`Ctrl+Shift+S`) writes a copy.

The editor always recalculates the SNES checksum.

### DKC1

Level data is uncompressed and is written back to its original slot. Nothing else
in the ROM changes.

### DKC2

The game shares one compressed tilemap between several level entries. The editor
recompresses the whole shared tilemap:

- If the new stream fits in the original compressed slot, it is written there.
- Otherwise the editor finds a large zero-filled run in the ROM, writes the new
  stream there, and updates the tilemap pointer for that tilemap id.
- The ROM size stays 4 MiB.

Keep a backup. Editing one entry can affect other entries that share the same
tilemap. If the ROM has no suitable free space, the save fails with a clear error
and leaves the file untouched.

### DKC3

This build edits stage settings only (see below). Saving writes the changed
fields and fixes the checksum. Terrain editing is planned for the next milestone.

## DKC3 stage settings

Select a DKC3 level and use the **Stage settings** panel:

- Music track
- Parrot flag (Kong's Cove)
- Koin flag (Bear's shop)
- Start, midpoint, Bonus 1, Bonus 2 and Warp parameters and coordinates

Coordinates are shown unbiased; the ROM's internal +256 bias is handled for you.
Changing a value marks the ROM as modified; press **Save ROM** to apply it.

## Level list

- Type in **Search levels...** to filter by name, group or two-digit id.
- Groups are alphabetical (theme names for DKC1, Levels/Bonuses for DKC2 and
  stage types for DKC3).
- The selected level is scrolled into view when you open a ROM or switch levels.

## Keyboard shortcuts

| Shortcut | Action |
| --- | --- |
| `Ctrl+O` | Open ROM |
| `Ctrl+S` | Save ROM |
| `Ctrl+Shift+S` | Save ROM As |
| `Ctrl+Z` | Undo |
| `Ctrl+Y` | Redo |
| `F` | Fit level to window |

## Troubleshooting

### "Unrecognised ROM title"

The ROM is not one of the supported US releases, is interleaved, or is corrupted.
Use a clean No-Intro style dump.

### "No map chip data for this tilemap"

The selected DKC2 entry does not have playable terrain (for example a cabin or a
control room). It is listed only for completeness.

### Save fails on DKC2

The recompressed tilemap is larger than the original and the editor could not
find a sufficiently large zero-filled run. The ROM has not been modified at that
point. Try a smaller edit or use a clean ROM.

### The level looks wrong after a special DKC1 edit

Snow, Ice, Temple and Millstone stages use special tileset prefixes. Their
rendering is implemented from public notes but is still on the visual-verification
list. Report which stage and tile look wrong.

## Backups

The editor does not create automatic backups in this build. Copy your ROM before
your first edit, for example:

```bat
copy "Donkey Kong Country (U) (V1.2) [!].smc" "dkc1.backup.smc"
```
