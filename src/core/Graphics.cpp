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

Charset decodeCharset(const std::vector<u8>& tileData) {
    Charset out;
    out.chars.reserve(tileData.size() / 32);
    for (std::size_t base = 0; base + 32 <= tileData.size(); base += 32) {
        CharTile t{};
        for (int y = 0; y < 8; ++y) {
            const u8 b0 = tileData[base + y * 2 + 0];
            const u8 b1 = tileData[base + y * 2 + 1];
            const u8 b2 = tileData[base + 16 + y * 2 + 0];
            const u8 b3 = tileData[base + 16 + y * 2 + 1];
            for (int x = 0; x < 8; ++x) {
                const int bit = 7 - x;
                u8 v = 0;
                v |= static_cast<u8>(((b0 >> bit) & 1) << 0);
                v |= static_cast<u8>(((b1 >> bit) & 1) << 1);
                v |= static_cast<u8>(((b2 >> bit) & 1) << 2);
                v |= static_cast<u8>(((b3 >> bit) & 1) << 3);
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

void buildLevelImage(std::vector<u32>& image, int width, int height,
                     const std::vector<u16>& tiles, int indexMask,
                     const std::vector<Metatile>& metas, const Charset& charset,
                     const Palette& palette, Color background) {
    const int pw = width * 32;
    const int ph = height * 32;
    image.assign(static_cast<std::size_t>(pw) * ph, packColor(background));
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
