#pragma once

#include "core/Rom.h"

#include <array>
#include <cstdint>
#include <vector>

namespace dk {

struct Color {
    u8 r = 0, g = 0, b = 0, a = 255;
};

using PaletteRow = std::array<Color, 16>;
using Palette = std::array<PaletteRow, 8>;

// A decoded 8x8 tile, one palette index per pixel.
struct CharTile {
    std::array<u8, 64> px{};
};

struct Charset {
    std::vector<CharTile> chars;
};

// 4x4 characters making a 32x32 metatile.  Each word is:
//   bits 0-9   character index
//   bits 10-12 palette row
//   bit  14    horizontal flip
//   bit  15    vertical flip
struct Metatile {
    std::array<u16, 16> refs{};
};

Palette loadPalette(const std::vector<u8>& data, std::size_t offset);
// Decodes SNES planar characters.  bpp is 2, 4 or 8 (16, 32 or 64 bytes per tile).
Charset decodeCharset(const std::vector<u8>& tileData, int bpp = 4);
std::vector<Metatile> decodeMetatiles(const std::vector<u8>& mapData);

u32 packColor(const Color& c);
Color unpackColor(u32 c);

// Draws a metatile at pixel position (dx,dy).  Index-0 pixels are left transparent.
void renderMetatile(std::vector<u32>& image, int imageWidth, int dx, int dy,
                    const Metatile& meta, const Charset& charset,
                    const Palette& palette, bool flipH, bool flipV);

// 32x32 RGBA thumbnail.  Transparent pixels are written as checkerboard.
std::vector<u32> renderMetatileThumbnail(const Metatile& meta, const Charset& charset,
                                         const Palette& palette);

// Renders a background layer tilemap.  Each entry is two bytes:
//   byte 0 = 8x8 character index
//   byte 1 = attributes: bits 2-4 palette row, bit 6 H flip, bit 7 V flip
// When transparentZero is true the layer is treated as an overlay and index-0
// pixels are left transparent so the sky gradient shows through.
void renderBackgroundTilemap(std::vector<u32>& image, int tileWidth, int tileHeight,
                             const std::vector<u8>& mapData, const Charset& charset,
                             const Palette& palette, bool transparentZero);

// Repaints a rectangle of the level backdrop (sky gradient plus the optional
// scenery layer) without touching terrain.  Tile edits use this so painting can
// never erase the background under a tile.
void fillLevelBackground(std::vector<u32>& image, int imageWidth, int levelPixelWidth,
                         int levelPixelHeight, int dx, int dy, int width, int height,
                         Color backgroundTop, Color backgroundBottom,
                         const std::vector<u32>& backgroundImage = {},
                         int backgroundWidth = 0, int backgroundHeight = 0);

void buildLevelImage(std::vector<u32>& image, int width, int height,
                     const std::vector<u16>& tiles, int indexMask,
                     const std::vector<Metatile>& metas, const Charset& charset,
                     const Palette& palette, Color backgroundTop,
                     Color backgroundBottom,
                     const std::vector<u32>& backgroundImage = {},
                     int backgroundWidth = 0, int backgroundHeight = 0);

} // namespace dk
