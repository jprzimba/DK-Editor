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
Charset decodeCharset(const std::vector<u8>& tileData);
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

void buildLevelImage(std::vector<u32>& image, int width, int height,
                     const std::vector<u16>& tiles, int indexMask,
                     const std::vector<Metatile>& metas, const Charset& charset,
                     const Palette& palette, Color background);

} // namespace dk
