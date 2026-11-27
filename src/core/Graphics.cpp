#include "core/Graphics.h"

#include <algorithm>

namespace dk {

Palette loadPalette(const std::vector<u8>& data, std::size_t offset) {
    Palette pal{};
    for (int row = 0; row < 8; ++row) {
        for (int col = 0; col < 16; ++col) {
            const std::size_t o = offset + static_cast<std::size_t>(row * 16 + col) * 2;
            Color c{};
            if (inRange(data, o, 2)) {
                const u16 raw = rd16(data, o);
                c.r = static_cast<u8>((raw & 0x1F) << 3);
                c.g = static_cast<u8>(((raw >> 5) & 0x1F) << 3);
                c.b = static_cast<u8>(((raw >> 10) & 0x1F) << 3);
                c.a = col == 0 ? 0 : 255;
            }
            pal[row][col] = c;
        }
    }
    return pal;
}

Charset decodeCharset(const std::vector<u8>& tileData, int bpp) {
    Charset out;
    if (bpp != 2 && bpp != 4 && bpp != 8) bpp = 4;
    const std::size_t tileBytes = static_cast<std::size_t>(bpp) * 8;
    out.chars.reserve(tileData.size() / tileBytes);
    for (std::size_t base = 0; base + tileBytes <= tileData.size(); base += tileBytes) {
        CharTile t{};
        for (int y = 0; y < 8; ++y) {
            for (int x = 0; x < 8; ++x) {
                const int bit = 7 - x;
                u8 v = 0;
                for (int p = 0; p < bpp; ++p) {
                    const std::size_t off = base + static_cast<std::size_t>((p / 2) * 16) +
                                            static_cast<std::size_t>(y * 2 + (p % 2));
                    if (off >= tileData.size()) continue;
                    v |= static_cast<u8>(((tileData[off] >> bit) & 1) << p);
                }
                t.px[y * 8 + x] = v;
            }
        }
        out.chars.push_back(t);
    }
    return out;
}

std::vector<Metatile> decodeMetatiles(const std::vector<u8>& mapData) {
    std::vector<Metatile> out;
    out.reserve(mapData.size() / 32);
    for (std::size_t base = 0; base + 32 <= mapData.size(); base += 32) {
        Metatile m{};
        for (int i = 0; i < 16; ++i) {
            m.refs[i] = rd16(mapData, base + static_cast<std::size_t>(i) * 2);
        }
        out.push_back(m);
    }
    return out;
}

u32 packColor(const Color& c) {
    return static_cast<u32>(c.r) | (static_cast<u32>(c.g) << 8) |
           (static_cast<u32>(c.b) << 16) | (static_cast<u32>(c.a) << 24);
}

Color unpackColor(u32 c) {
    return Color{static_cast<u8>(c & 0xFF), static_cast<u8>((c >> 8) & 0xFF),
                 static_cast<u8>((c >> 16) & 0xFF), static_cast<u8>((c >> 24) & 0xFF)};
}

void renderMetatile(std::vector<u32>& image, int imageWidth, int dx, int dy,
                    const Metatile& meta, const Charset& charset,
                    const Palette& palette, bool flipH, bool flipV) {
    if (imageWidth <= 0) return;
    for (int ty = 0; ty < 4; ++ty) {
        for (int tx = 0; tx < 4; ++tx) {
            const u16 ref = meta.refs[ty * 4 + tx];
            const int ci = ref & 0x3FF;
            const int pi = (ref >> 10) & 7;
            const bool fh = (ref & 0x4000) != 0;
            const bool fv = (ref & 0x8000) != 0;
            if (ci < 0 || ci >= static_cast<int>(charset.chars.size())) continue;
            const CharTile& ch = charset.chars[ci];
            const PaletteRow& row = palette[pi];
            for (int y = 0; y < 8; ++y) {
                for (int x = 0; x < 8; ++x) {
                    const int sx = fh ? 7 - x : x;
                    const int sy = fv ? 7 - y : y;
                    const u8 idx = ch.px[sy * 8 + sx];
                    if (idx == 0) continue;
                    int px = dx + tx * 8 + x;
                    int py = dy + ty * 8 + y;
                    if (flipH) px = dx + 31 - (tx * 8 + x);
                    if (flipV) py = dy + 31 - (ty * 8 + y);
                    if (px < 0 || py < 0) continue;
                    if (px >= imageWidth) continue;
                    const std::size_t off = static_cast<std::size_t>(py) * imageWidth + px;
                    if (off >= image.size()) continue;
                    image[off] = packColor(row[idx]);
                }
            }
        }
    }
}

std::vector<u32> renderMetatileThumbnail(const Metatile& meta, const Charset& charset,
                                         const Palette& palette) {
    std::vector<u32> out(32 * 32, 0);
    for (int y = 0; y < 32; ++y) {
        for (int x = 0; x < 32; ++x) {
            const bool checker = ((x / 8 + y / 8) & 1) != 0;
            out[y * 32 + x] = checker ? packColor(Color{48, 48, 52, 255})
                                      : packColor(Color{36, 36, 40, 255});
        }
    }
    renderMetatile(out, 32, 0, 0, meta, charset, palette, false, false);
    return out;
}

void renderBackgroundTilemap(std::vector<u32>& image, int tileWidth, int tileHeight,
                             const std::vector<u8>& mapData, const Charset& charset,
                             const Palette& palette, bool transparentZero) {
    if (tileWidth <= 0 || tileHeight <= 0) {
        image.clear();
        return;
    }
    const int pw = tileWidth * 8;
    const int ph = tileHeight * 8;
    image.assign(static_cast<std::size_t>(pw) * ph, 0);
    for (int ty = 0; ty < tileHeight; ++ty) {
        for (int tx = 0; tx < tileWidth; ++tx) {
            const std::size_t entry = static_cast<std::size_t>(ty * tileWidth + tx) * 2;
            if (entry + 1 >= mapData.size()) continue;
            const int ci = mapData[entry];
            const u8 attr = mapData[entry + 1];
            const int pi = (attr >> 2) & 7;
            const bool fh = (attr & 0x40) != 0;
            const bool fv = (attr & 0x80) != 0;
            if (ci < 0 || ci >= static_cast<int>(charset.chars.size())) continue;
            const CharTile& ch = charset.chars[ci];
            const PaletteRow& row = palette[pi];
            for (int y = 0; y < 8; ++y) {
                for (int x = 0; x < 8; ++x) {
                    const int sx = fh ? 7 - x : x;
                    const int sy = fv ? 7 - y : y;
                    const u8 idx = ch.px[sy * 8 + sx];
                    if (idx == 0 && transparentZero) continue;
                    const std::size_t off =
                        static_cast<std::size_t>((ty * 8 + y) * pw + tx * 8 + x);
                    image[off] = packColor(row[idx]);
                }
            }
        }
    }
}

u32 backgroundPixel(int x, int y, int pixelWidth, int pixelHeight, Color backgroundTop,
                    Color backgroundBottom, const std::vector<u32>& backgroundImage,
                    int backgroundWidth, int backgroundHeight, bool useBg, bool tileBg) {
    const float t = pixelHeight > 1 ? static_cast<float>(y) / (pixelHeight - 1) : 0.0f;
    const Color grad{
        static_cast<u8>(backgroundTop.r + (backgroundBottom.r - backgroundTop.r) * t),
        static_cast<u8>(backgroundTop.g + (backgroundBottom.g - backgroundTop.g) * t),
        static_cast<u8>(backgroundTop.b + (backgroundBottom.b - backgroundTop.b) * t),
        255};
    u32 c = packColor(grad);
    if (!useBg) return c;
    const int bgY = tileBg ? y % backgroundHeight : (y * backgroundHeight) / pixelHeight;
    const int bgX = x % backgroundWidth;
    const u32 s = backgroundImage[static_cast<std::size_t>(bgY) * backgroundWidth + bgX];
    if ((s >> 24) != 0) {
        // Slight dim so edited terrain keeps reading as the foreground.
        c = packColor(Color{static_cast<u8>((s & 0xFF) * 4 / 5),
                            static_cast<u8>(((s >> 8) & 0xFF) * 4 / 5),
                            static_cast<u8>(((s >> 16) & 0xFF) * 4 / 5), 255});
    }
    return c;
}

bool backgroundUsable(const std::vector<u32>& backgroundImage, int backgroundWidth,
                      int backgroundHeight) {
    return !backgroundImage.empty() && backgroundWidth > 0 && backgroundHeight > 0 &&
           backgroundImage.size() >=
               static_cast<std::size_t>(backgroundWidth) * backgroundHeight;
}

void fillLevelBackground(std::vector<u32>& image, int imageWidth, int levelPixelWidth,
                         int levelPixelHeight, int dx, int dy, int width, int height,
                         Color backgroundTop, Color backgroundBottom,
                         const std::vector<u32>& backgroundImage,
                         int backgroundWidth, int backgroundHeight) {
    if (imageWidth <= 0 || width <= 0 || height <= 0) return;
    const bool useBg = backgroundUsable(backgroundImage, backgroundWidth, backgroundHeight);
    const bool tileBg = useBg && levelPixelHeight > backgroundHeight * 3;
    for (int y = 0; y < height; ++y) {
        const int py = dy + y;
        if (py < 0 || py >= levelPixelHeight) continue;
        const std::size_t row = static_cast<std::size_t>(py) * imageWidth;
        for (int x = 0; x < width; ++x) {
            const int px = dx + x;
            if (px < 0 || px >= levelPixelWidth) continue;
            const std::size_t off = row + px;
            if (off >= image.size()) continue;
            image[off] = backgroundPixel(px, py, levelPixelWidth, levelPixelHeight,
                                        backgroundTop, backgroundBottom, backgroundImage,
                                        backgroundWidth, backgroundHeight, useBg, tileBg);
        }
    }
}

void buildLevelImage(std::vector<u32>& image, int width, int height,
                     const std::vector<u16>& tiles, int indexMask,
                     const std::vector<Metatile>& metas, const Charset& charset,
                     const Palette& palette, Color backgroundTop,
                     Color backgroundBottom,
                     const std::vector<u32>& backgroundImage,
                     int backgroundWidth, int backgroundHeight) {
    const int pw = width * 32;
    const int ph = height * 32;
    image.assign(static_cast<std::size_t>(pw) * ph, 0);

    const bool useBg = backgroundUsable(backgroundImage, backgroundWidth, backgroundHeight);
    // Horizontal levels stretch the backdrop to the level height so the sky and
    // horizon stay put.  Very tall levels would turn it into a smear, so those
    // repeat it instead.
    const bool tileBg = useBg && ph > backgroundHeight * 3;
    for (int y = 0; y < ph; ++y) {
        for (int x = 0; x < pw; ++x) {
            image[static_cast<std::size_t>(y) * pw + x] =
                backgroundPixel(x, y, pw, ph, backgroundTop, backgroundBottom,
                                backgroundImage, backgroundWidth, backgroundHeight,
                                useBg, tileBg);
        }
    }

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t ti = static_cast<std::size_t>(y) * width + x;
            if (ti >= tiles.size()) continue;
            const u16 t = tiles[ti];
            const int mi = t & indexMask;
            if (mi < 0 || mi >= static_cast<int>(metas.size())) continue;
            const bool fh = (t & 0x4000) != 0;
            const bool fv = (t & 0x8000) != 0;
            renderMetatile(image, pw, x * 32, y * 32, metas[mi], charset, palette, fh, fv);
        }
    }
}

} // namespace dk
