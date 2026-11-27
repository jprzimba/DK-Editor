# Credits and References

DK Editor is a clean-room C++ editor built from public reverse-engineering
documentation and direct analysis of the user's own ROM dumps. No game assets or
ROM data are distributed with this repository.

## Libraries

- **Dear ImGui** - Omar Cornut and contributors, MIT.
  <https://github.com/ocornut/imgui>
- **dkcomp** - Kingizor, MIT. Big Data, Small Data, DKC CHR and other Donkey Kong
  compression formats.
  <https://github.com/Kingizor/dkcomp>

## Reverse-engineering references

The following public projects and documents were used to understand the level
structures. They are not bundled; `.research/` is excluded from version control.

### DKC1

- **RainbowZ Editor** by mike19283 - DKC1 level offsets, sizes, themes, palette
  offsets, tileset indices and Map32 sizes.
  <https://github.com/mike19283/RainbowZ-Editor>
- **DKC1 disassembly** by Yoshifanatic1 - level data pointers, tileset and
  metatile tables, level ID names.
  <https://github.com/Yoshifanatic1/Donkey-Kong-Country-1-Disassembly>
- **DKC disassembly** by DKC-Recomp - data layout and asset pointers.
  <https://github.com/DKC-Recomp/DKC-disassembly>
- **DKC Atlas** - map dimensions and visual references.
  <https://www.dkc-atlas.com/>
- Raccoon Sam's public notes on the DKC1 column-major tilemap format.

### DKC2

- **dkc2-maps** by francislr - C++ level viewer with level config, style,
  submap, tilemap and graphics parsing.
  <https://github.com/francislr/dkc2-maps>
- **DKC2 disassembly** by p4plus2 - RAM map and level config field names.
  <https://github.com/p4plus2/dkc2-disassembly>
- **DKC2 disassembly** by Yoshifanatic1 - asset pointers confirming DKC2
  graphics, map-chip and level-data addresses.
  <https://github.com/Yoshifanatic1/Donkey-Kong-Country-2-Disassembly>
- **DKC-Twool** by xDRL64 - DKC2 level graphics tool and reference asset lists.
  <https://github.com/xDRL64/DKC-Twool>
- **Kingizor's compression research** and Karaage Lab compression documents.

### DKC3

- **dkc3-level-properties** by Glan9 - stage config addresses and field offsets.
  <https://github.com/Glan9/dkc3-level-properties>
- **DKC3 disassembly** by Yoshifanatic1 - level data regions, Map32 bases and
  asset pointers.
  <https://github.com/Yoshifanatic1/Donkey-Kong-Country-3-Disassembly>
- **Karaage Lab DKC3 documents** - compression format and tilemap compression.
  <https://ss1.xrea.com/karaagelabdkc.s341.xrea.com/en/dkc3/>
- **dkcomp** formats for Big Data and Small Data.

### General

- **SNESdev Wiki** - SNES tile formats and memory layout.
  <https://snes.nesdev.org/wiki/Tiles>
- **Data Crystal** - cartridge header and ROM map conventions.
  <https://datacrystal.tcrf.net/>

## Assets

No tilesets, palettes, music, sprites or ROMs are included. All graphics shown by
the editor are decoded at runtime from the ROM supplied by the user.

## Trademarks

Donkey Kong Country and related names, characters and imagery are trademarks of
Nintendo and Rare. This project is not affiliated with or endorsed by Nintendo or
Rare.
