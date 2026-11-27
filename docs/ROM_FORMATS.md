# ROM Formats

This page documents the structures DK Editor currently understands. Addresses are
SNES addresses unless marked as file offsets. A 4 MiB HiROM image maps a SNES
address `$XX:YYYY` to file offset `((XX & 0x3F) << 16) | YYYY` for the banks used
here, which is equivalent to `address & 0x3FFFFF` for this project.

The editor strips an optional 512-byte copier header before using any address.

## Common cartridge header

- Internal title at `0xFFC0`, 21 bytes.
- Version byte at `0xFFDB`.
- Checksum complement at `0xFFDC`, checksum at `0xFFDE`.
- DKC1 title: `DONKEY KONG COUNTRY`.
- DKC2 title: `DIDDY'S KONG QUEST`.
- DKC3 title: `DONKEY KONG COUNTRY 3`.

Checksum repair:

1. Set `0xFFDC..0xFFDF` to `FF FF 00 00`.
2. Sum every byte of the image.
3. Write the 16-bit sum at `0xFFDE` and its complement at `0xFFDC`.

## Compression

### DKC CHR (DKC1 tilesets)

The stream begins with a 0x80-byte header. Commands are bytes:

| Command | Meaning |
| --- | --- |
| `0x00` | End of stream |
| `0x01..0x3F` | Copy that many raw bytes |
| `0x40..0x7F` | Repeat the next byte `(cmd & 0x3F)` times |
| `0x80..0xBF` | Copy `(cmd & 0x3F)` bytes from a 16-bit back-reference |
| `0xC0..0xFF` | Emit the 16-bit word at header offset `2 * (cmd & 0x3F)` |

Implemented by `dkcomp` (`DKCCHR_COMP`).

### Big Data (DKC2/DKC3 tilesets, map chips, tilemaps)

A 38-byte frequency table followed by nibble-oriented commands. The vendor
library `dkcomp` implements both decompression and compression. DK Editor uses it
for:

- DKC2 map chips, level graphics and tilemaps.
- Future DKC3 terrain support.

### Small Data (DKC3 tilemaps)

A dedicated DKC3 tilemap format with bit-packed fields. `dkcomp` supports it
(`SD_COMP`); DK Editor does not yet use it because the DKC3 level-to-tilemap
mapping is still being researched.

## Tile graphics

### 8x8 tiles

SNES 4bpp planar layout, 32 bytes per tile:

```
bytes 0..15:  rows 0..7, plane 0 then plane 1
bytes 16..31: rows 0..7, plane 2 then plane 3
```

For each pixel, bit 7 is the leftmost pixel. The four plane bits form a palette
index 0..15. Index 0 is transparent for level terrain.

### 32x32 metatiles

Both DKC1 and DKC2 use 16-word records (4x4 character references). Each word:

- bits 0-9: character index
- bits 10-12: palette row
- bit 14: horizontal flip
- bit 15: vertical flip

### Palette

8 rows of 16 RGB555 words, 256 bytes total. Row 0 is the first 16 colours.
Colour 0 of each row is transparent. The editor converts `0bbbbbgggggrrrrr` to
8-bit RGB by shifting each 5-bit component left by 3.

## DKC1

### Level data

DKC1 levels are not compressed. A horizontal stage is stored as columns of 16
tiles:

```
word index = x * 16 + y
```

A vertical stage is stored as rows of 64 tiles:

```
word index = y * 64 + x
```

Each word is a tilemap entry:

- bits 0-9: metatile index
- bit 14: horizontal flip
- bit 15: vertical flip

The list of DKC1 stages, offsets, sizes, theme/header, palette offsets and tileset
indices is in `src/games/Dkc1Levels.inc`.

### Themes and Map32

The header/theme index selects:

- Map32 metatile base and count (see `kMap32Base` and `kMetaCount` in
  `src/games/Dkc1.cpp`).
- Theme name shown in the level list.

### Tileset resolution

The game resolves a tileset index through tables at `$B9A994`, `$B9A998` and
`$B9A995`. DK Editor mirrors that routine in `resolveTileset()`.

A few level codes prepend or shift tileset data:

- Codes `0x42`, `0x43`, `0x55`, `0x56`, `0x57`, `0x52`, `0x53`: discard the
  first 0x20 bytes and prepend the data at `$DBCCD2` (0x22C0 bytes).
- Codes `0x24`, `0xA7`, `0x67`, `0x66`, `0x69`, `0x6A`, `0x6B`: replace the first
  0x20 bytes with zeros and append 0x2000 zero bytes.
- Code `0x68` (Gangplank Galleon): use the raw data at `$0F0000` (0x8500 bytes)
  instead of a compressed tileset.

### DKC1 save

The editor writes the tilemap bytes back at the original file offset in the
original column-major or row-major order. The ROM size does not change.

## DKC2

DKC2 is a HiROM 4 MiB image using compressed level resources.

### Level configuration

- Pointer table at file offset `0x3D0000`. Each 16-bit entry is an offset inside
  bank `$3D`; the config address is `0x3D0000 + entry`.
- Config layout:
  - `+0`: type word (4 and 5 have no editable style).
  - If type == 1: `+2` bonus type byte, style word at `+3`.
  - Otherwise style word at `+2`.
  - After the style word: submap index byte.
- Style record at `0x3D0000 + styleWord`:
  - `+0` routine 1 word
  - `+2` routine 2 word
  - `+4` music byte
  - `+6` palette offset word (0 means use the map default)
  - `+13` graphics index byte
  - `+18` tilemap id byte

The level names come from the public DKC2 maps project and are stored in
`src/games/Dkc2Levels.inc`.

### Map chips

- 3-byte pointer table at `0x35BB2E + tilemapId * 3`.
- Pointer is a SNES address; mask with `0x3FFFFF` for the file offset.
- Big Data decompression yields 32-byte metatile records.

### Level graphics

- Pointer list at `0x3D819A + graphicsIndex * 2`; the word is added to
  `0x3D819A` to get the chunk list.
- Each chunk is 7 bytes: `bank`, `addr`, `op`, `size`.
- `op == 0xA000`: Big Data stream at `bank:addr`.
- `op == 0x2000`: raw data of `size` bytes at `bank:addr`.
- The first usable chunk is decoded into 8x8 characters.

### Tilemap settings

- Tilemap type: `0x35BC54 + tilemapId * 2`. Bit `0x0400` means Big Data; otherwise
  the data is a 256-byte uncompressed block.
- Submap list: `0x35BC7E + tilemapId * 2` gives an offset into bank `$35`
  (`0x350000 + word`).
  - `+0` total map width in pixels
  - `+2` total map height in pixels
  - `+4 + submapIndex * 10`: left, right, top, bottom, layout code
- Tilemap pointer: `0x35BAEF + tilemapId * 3` (SNES address).
- The decompressed tilemap can contain several submaps.
  - Horizontal (layout code 0..3): `index = worldX * stride + worldY`, with
    `stride = totalHeight / 32`.
  - Vertical (layout code 4..7 or bit `0x40`): `index = worldX + worldY * stride`,
    with `stride = totalWidth / 32`.
- Tilemap entry: bits 0-11 metatile index, bit 14 horizontal flip, bit 15
  vertical flip.

### Backgrounds (Layer 2)

DKC2 backgrounds are ordinary compressed graphic resources plus a small 8x8
tilemap. Their file offsets, used by `backgroundForLevel()` in `Dkc2.cpp`:

| Theme | Tileset | Tilemap | bpp |
| --- | --- | --- | --- |
| Ship Deck ocean/sky | `0x29FDB3` | `0x07FCB8` | 4 |
| Ship Mast clouds | `0x2A121C` | `0x0AFABE` | 4 |
| Haunted Hall | `0x295221` | `0x0CFEDF` | 4 |
| Ship hold back wall | `0x29A905` | `0x09FF37` | 4 |
| Brambles | `0x2AB6F0` | `0x380000` | 4 |
| Honeycomb cells | `0x16F791` | `0x14FC2D` | 4 |
| Forest trees | `0x3B6FC0` | `0x15FEB3` | 4 |
| Carnival / track | `0x2A23D4` | `0x0FFFD9` | 4 |
| Lava cave rock | `0x295623` | `0x1CFA5E` | 4 |
| Cave sparkles | `0x2B2B84` | `0x19F7C9` | 2 |

Both resources use Big Data. Each map entry is two bytes: character index and
attributes (bits 2-4 palette row, bit 6 horizontal flip, bit 7 vertical flip).
The map is 32 tiles wide and `decompressed size / 64` tiles tall.

### DKC2 save

1. Copy the edited region back into the shared decompressed tilemap.
2. Compress the whole buffer with Big Data.
3. If the result fits the original compressed slot, write it in place and zero
   the tail.
4. Otherwise scan the ROM for the highest sufficiently large zero-filled run,
   write the stream there, and update the 3-byte pointer at
   `0x35BAEF + tilemapId * 3`.
5. Update the in-memory context so a second save without reload keeps using the
   new location.

The ROM size stays 4 MiB. The editor never expands the image.

## DKC3

### Stage configuration

### DKC3 stage configuration

DKC3 stage config records are listed in `src/games/Dkc3Levels.inc`. They are
variable-length structures at file offsets such as `0x3D0599` (regular stages are
`0x36` bytes apart). The documented layout is:

```
dw type
db music
dw tilesetConfig        ; pointer into bank $FD (11-byte theme record)
db layout               ; Horizontal / TallHorizontal / Vertical / Square / ...
db palette              ; index into the palette table at $FD54E9
db ? dw camera
db gimmick
dw ?
dw flags
... entrance list (5 bytes each), terminated by a $FF byte ...
dw destination ids ..., $FFFF, $FFFF
```

The editor currently exposes:

| Offset | Meaning |
| --- | --- |
| `+2` | Music track |
| `+13` bit 4 | Parrot flag |
| `+14` bit 4 | Koin flag |
| `+15` / `+16` / `+18` | Start parameter / X / Y |
| `+20` / `+21` / `+23` | Midpoint parameter / X / Y |
| `+25` / `+26` / `+28` | Bonus 1 parameter / X / Y |
| `+30` / `+31` / `+33` | Bonus 2 parameter / X / Y |
| `+35` / `+36` / `+38` | Warp parameter / X / Y |

Coordinates are stored biased by 256 in the ROM; the UI displays the unbiased
value.

### DKC3 palettes

- Palette pointer table: `0xFD54E9` (file `0x3D54E9`), one 16-bit offset per id.
- Palette data starts at `0xFD0000 + offset`, 0x100 bytes (8 rows of 16 RGB555
  colours).
- The stage configuration's palette byte selects the stage's `FGBG_*` palette.

### DKC3 tilemap compression

DKC3 layer tilemaps use a dedicated format (documented by Karaage Lab), not Big
Data:

- 3-byte header: field-expansion specifier (bit 0 priority, bit 1 H flip, bit 2
  V flip) and a 16-bit word count.
- Fields are written in order: priority, H flip, V flip, palette index, tile
  index, each bit-packed MSB first.
- Optional fields use `0 X` (one word) or `1 X YYYYYY` (run of `Y` words);
  palette uses 3-bit values with 4-bit runs; the tile index uses 2-bit commands
  (single, run, ascending run, descending run).
- Implemented in `decompressTilemapDkc3()` (`src/core/Compression.*`).

### DKC3 backgrounds

- Theme tilemaps and tilesets live in the compressed tilemap / graphics tables
  (`TM_Layer2_*`, `GFX_Layer2_*`, ... in the asset listing).
- Backgrounds are drawn with the stage's `FGBG_*` palette. Tilesets are loaded at
  a VRAM character base that is recovered from the highest index used by the map.
- Currently wired for cave, forest, riverbank, pipe and lakeside stages as a
  read-only preview.

### DKC3 terrain

Not implemented yet. The remaining piece is the per-stage map offset and layout
inside the Big Data "LevelData" bundles (see `TASKS.md`).

## Safety notes

- Always keep a backup before editing a ROM.
- DKC2 tilemaps are shared by several entries. Editing one may affect others that
  use the same tilemap id.
- The relocation path relies on zero-filled free runs. If a ROM has been modified
  and its free space is no longer zero-filled, relocation may fail with a clear
  error instead of overwriting data.
- The checksum is recalculated on every save.
