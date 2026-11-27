#include "games/Dkc3.h"

#include "core/Compression.h"
#include "core/Graphics.h"

#include <algorithm>
#include <cstdio>

namespace dk {

namespace {

struct Dkc3Def {
    int id;
    const char* name;
    std::size_t address;
    int type;
};

static const Dkc3Def kLevels[] = {
#include "games/Dkc3Levels.inc"
};

std::string typeGroup(int type) {
    switch (type) {
    case 1: return "Regular Stages";
    case 2: return "Bonus Stages";
    case 3: return "Bosses";
    case 5: return "Swanky's Sideshow";
    case 6: return "Water Exit Rooms";
    default: return "Other";
    }
}

LevelProperty byteProp(std::size_t offset, const char* label, int bytes = 1,
                       int minimum = 0, int maximum = 255, int bit = -1,
                       int displayOffset = 0) {
    LevelProperty p;
    p.label = label;
    p.offset = offset;
    p.bytes = bytes;
    p.minimum = minimum;
    p.maximum = maximum;
    p.bit = bit;
    p.displayOffset = displayOffset;
    return p;
}

// A background layer: a tilemap plus the 8x8 tileset it references.
struct Dkc3BgLayer {
    std::size_t map = 0;      // file offset of the tilemap
    std::size_t mapSize = 0;  // bytes, for uncompressed maps
    bool mapCompressed = true;
    std::size_t gfx = 0;  // file offset of the compressed tileset
};

struct Dkc3BgSet {
    Dkc3BgLayer back;
    Dkc3BgLayer front;
};

// Validated DKC3 backgrounds (tilemap + tileset) per stage theme.
const Dkc3BgSet kBgNone{};
const Dkc3BgSet kBgCave{
    {}, {0x3275D5, 0, true, 0x32587C}};  // TM/GFX_Layer2_CaveLevels
const Dkc3BgSet kBgForest{
    {}, {0x2712B8, 0, true, 0x2717A6}};  // TM/GFX_Layer2_ForestLevels
const Dkc3BgSet kBgRiver{
    {}, {0x288388, 0, true, 0x28861B}};  // TM/GFX_Layer2_RiverbankLevels
const Dkc3BgSet kBgDrain{
    {}, {0x2A7D8A, 0x800, false, 0x2A68F2}};  // TM/GFX_Layer2_DrainPipeLevels
const Dkc3BgSet kBgPier{
    {},
    {0x2591C9, 0, true, 0x254C41}};  // TM_Layer1_PierLevelWaterllineAndMountains

const Dkc3BgSet& backgroundForStage(int id) {
    switch (id) {
    case 0x44: case 0x45: case 0x46:  // Buzzer Barrage / Pot-Hole Panic / Creepy Caverns
        return kBgCave;
    case 0x2E: case 0x2F: case 0x30: case 0x31:  // forest stages
        return kBgForest;
    case 0x32: case 0x33: case 0x34:  // riverbank stages
        return kBgRiver;
    case 0x3F: case 0x40:  // pipe stages
        return kBgDrain;
    case 0x25: case 0x26: case 0x27:  // lakeside stages
        return kBgPier;
    default:
        return kBgNone;
    }
}

Palette loadFgbgPalette(const Rom& rom, int paletteId) {
    Palette pal{};
    const std::size_t table = 0x3D54E9;
    const std::size_t entry = table + static_cast<std::size_t>(paletteId) * 2;
    if (!inRange(rom.data, entry, 2)) return pal;
    const std::size_t base = 0x3D0000 + rom.word(entry);
    for (int row = 0; row < 8; ++row) {
        for (int col = 0; col < 16; ++col) {
            Color c{};
            const std::size_t o = base + static_cast<std::size_t>(row * 16 + col) * 2;
            if (inRange(rom.data, o, 2)) {
                const u16 raw = rd16(rom.data, o);
                c.r = static_cast<u8>((raw & 0x1F) << 3);
                c.g = static_cast<u8>(((raw >> 5) & 0x1F) << 3);
                c.b = static_cast<u8>(((raw >> 10) & 0x1F) << 3);
                c.a = 255;
            }
            pal[row][col] = c;
        }
    }
    return pal;
}

// Draws one background layer into an existing RGBA image.  Index-0 pixels stay
// transparent so lower layers and the sky gradient show through.
bool drawBackgroundLayer(const Rom& rom, const Dkc3BgLayer& layer, const Palette& palette,
                         std::vector<u32>& image, int width, int height) {
    if (layer.map == 0 || layer.gfx == 0) return false;

    std::vector<u16> words;
    std::string error;
    if (layer.mapCompressed) {
        if (!decompressTilemapDkc3(rom.data, layer.map, words, error)) return false;
    } else {
        const std::size_t bytes = std::min<std::size_t>(layer.mapSize, rom.data.size());
        if (bytes < 64 || !inRange(rom.data, layer.map, bytes)) return false;
        words.resize(bytes / 2);
        for (std::size_t i = 0; i < words.size(); ++i) {
            words[i] = rd16(rom.data, layer.map + i * 2);
        }
    }
    if (words.size() < static_cast<std::size_t>(width)) return false;

    std::vector<u8> gfxData;
    if (!decompress(DkFormat::BigData, rom.data, layer.gfx, gfxData, error)) return false;
    const Charset charset = decodeCharset(gfxData);
    if (charset.chars.empty()) return false;

    int maxIndex = 0;
    for (u16 v : words) maxIndex = std::max(maxIndex, static_cast<int>(v & 0x3FF));
    const int charBase =
        std::max(0, maxIndex - static_cast<int>(charset.chars.size()) + 1);

    const int rows = height / 8;
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t index = static_cast<std::size_t>(y) * width + x;
            if (index >= words.size()) continue;
            const u16 v = words[index];
            const int ci = static_cast<int>(v & 0x3FF) - charBase;
            if (ci < 0 || ci >= static_cast<int>(charset.chars.size())) continue;
            const int pi = (v >> 10) & 7;
            const bool fh = (v & 0x4000) != 0;
            const bool fv = (v & 0x8000) != 0;
            const CharTile& ch = charset.chars[ci];
            const PaletteRow& row = palette[pi];
            for (int ty = 0; ty < 8; ++ty) {
                for (int tx = 0; tx < 8; ++tx) {
                    const int sx = fh ? 7 - tx : tx;
                    const int sy = fv ? 7 - ty : ty;
                    const u8 idx = ch.px[sy * 8 + sx];
                    if (idx == 0) continue;
                    const int px = x * 8 + tx;
                    const int py = y * 8 + ty;
                    image[static_cast<std::size_t>(py) * (width * 8) + px] =
                        packColor(row[idx]);
                }
            }
        }
    }
    return true;
}

// Builds the background preview for a stage.  Returns false when the theme has
// no decoded assets yet.  width/height are returned in pixels.
bool buildBackground(const Rom& rom, int levelId, const Palette& palette,
                     std::vector<u32>& image, int& width, int& height) {
    const Dkc3BgSet& set = backgroundForStage(levelId);
    if (set.front.map == 0 && set.back.map == 0) return false;

    std::vector<u16> probe;
    std::string error;
    int words = 0;
    const Dkc3BgLayer& first = set.front.map ? set.front : set.back;
    if (first.mapCompressed) {
        if (!decompressTilemapDkc3(rom.data, first.map, probe, error)) return false;
        words = static_cast<int>(probe.size());
    } else {
        words = static_cast<int>(std::min<std::size_t>(first.mapSize, rom.data.size()) / 2);
    }
    if (words < 32) return false;

    constexpr int kTileWidth = 32;
    const int rows = words / kTileWidth;
    const int pixelsW = kTileWidth * 8;
    int pixelsH = rows * 8;
    // The editor works in 32x32 cells, so pad the height to a whole cell.
    pixelsH += (32 - (pixelsH % 32)) % 32;

    image.assign(static_cast<std::size_t>(pixelsW) * pixelsH, 0);
    // Sky gradient behind transparent parts.
    const Color top{24, 44, 82, 255};
    const Color bottom{96, 150, 204, 255};
    for (int y = 0; y < pixelsH; ++y) {
        const float t = pixelsH > 1 ? static_cast<float>(y) / (pixelsH - 1) : 0.0f;
        const Color c{static_cast<u8>(top.r + (bottom.r - top.r) * t),
                      static_cast<u8>(top.g + (bottom.g - top.g) * t),
                      static_cast<u8>(top.b + (bottom.b - top.b) * t), 255};
        for (int x = 0; x < pixelsW; ++x) {
            image[static_cast<std::size_t>(y) * pixelsW + x] = packColor(c);
        }
    }

    drawBackgroundLayer(rom, set.back, palette, image, kTileWidth, pixelsH);
    drawBackgroundLayer(rom, set.front, palette, image, kTileWidth, pixelsH);

    // Repeat the backdrop horizontally so the canvas shows a level-wide view.
    constexpr int kRepeat = 4;
    std::vector<u32> wide(static_cast<std::size_t>(pixelsW) * kRepeat * pixelsH, 0);
    for (int y = 0; y < pixelsH; ++y) {
        for (int r = 0; r < kRepeat; ++r) {
            for (int x = 0; x < pixelsW; ++x) {
                wide[static_cast<std::size_t>(y) * pixelsW * kRepeat + r * pixelsW + x] =
                    image[static_cast<std::size_t>(y) * pixelsW + x];
            }
        }
    }
    image.swap(wide);
    width = pixelsW * kRepeat;
    height = pixelsH;
    return true;
}

} // namespace

bool Dkc3Game::init(Rom& rom, std::string& error) {
    (void)rom;
    error.clear();
    levels_.clear();
    addresses_.clear();
    types_.clear();
    for (const auto& def : kLevels) {
        LevelEntry e;
        e.id = def.id;
        e.name = def.name;
        e.group = typeGroup(def.type);
        e.supported = true;
        e.visual = false;
        e.note = "Visual DKC3 editing is not available in this build.";
        levels_.push_back(std::move(e));
        addresses_.push_back(def.address);
        types_.push_back(def.type);
    }
    return true;
}

std::string Dkc3Game::summary() const {
    return "DKC3 stage settings can be edited. Validated stage backgrounds are "
           "shown as a read-only preview; terrain editing is the next milestone.";
}

bool Dkc3Game::loadLevel(Rom& rom, int levelId, LoadedLevel& out, std::string& error) {
    error.clear();
    out = LoadedLevel{};
    int index = -1;
    for (int i = 0; i < static_cast<int>(levels_.size()); ++i) {
        if (levels_[i].id == levelId) {
            index = i;
            break;
        }
    }
    if (index < 0) {
        error = "Invalid DKC3 level id.";
        return false;
    }
    const std::size_t a = addresses_[index];
    if (!inRange(rom.data, a, 40)) {
        error = "The DKC3 level config is outside the ROM.";
        return false;
    }
    out.info = levels_[index];
    out.info.visual = false;
    out.info.supported = true;
    out.background = Color{10, 12, 18, 255};

    // Optional background preview (validated themes only).
    const int paletteId = rom.byte(a + 6);
    out.palette = loadFgbgPalette(rom, paletteId);
    int bgWidth = 0, bgHeight = 0;
    if (buildBackground(rom, levelId, out.palette, out.image, bgWidth, bgHeight)) {
        out.width = bgWidth / 32;
        out.height = bgHeight / 32;
        out.info.note = "Background preview only; DKC3 terrain editing is coming next.";
    } else {
        out.info.note = "Visual DKC3 editing is not available for this stage yet.";
    }

    auto addByte = [&](std::size_t off, const char* label, int min = 0, int max = 255) {
        LevelProperty p = byteProp(off, label, 1, min, max);
        p.value = rom.byte(off);
        out.properties.push_back(p);
    };
    auto addWord = [&](std::size_t off, const char* label, int displayOffset) {
        LevelProperty p = byteProp(off, label, 2, -32768, 65535, -1, displayOffset);
        p.value = rom.word(off);
        out.properties.push_back(p);
    };
    auto addBit = [&](std::size_t off, int bit, const char* label) {
        LevelProperty p = byteProp(off, label, 1, 0, 1, bit);
        p.value = (rom.byte(off) >> bit) & 1;
        out.properties.push_back(p);
    };

    addByte(a + 2, "Music track");
    addBit(a + 13, 4, "Parrot (Kong's Cove)");
    addBit(a + 14, 4, "Koin (Bear's shop)");
    addByte(a + 15, "Start parameter");
    addWord(a + 16, "Start X", -256);
    addWord(a + 18, "Start Y", -256);
    addByte(a + 20, "Midpoint parameter");
    addWord(a + 21, "Midpoint X", -256);
    addWord(a + 23, "Midpoint Y", -256);
    addByte(a + 25, "Bonus 1 parameter");
    addWord(a + 26, "Bonus 1 X", -256);
    addWord(a + 28, "Bonus 1 Y", -256);
    addByte(a + 30, "Bonus 2 parameter");
    addWord(a + 31, "Bonus 2 X", -256);
    addWord(a + 33, "Bonus 2 Y", -256);
    addByte(a + 35, "Warp parameter");
    addWord(a + 36, "Warp X", -256);
    addWord(a + 38, "Warp Y", -256);

    return true;
}

bool Dkc3Game::saveLevel(Rom& rom, const LoadedLevel& level, std::string& error) {
    error.clear();
    for (const LevelProperty& p : level.properties) {
        if (!inRange(rom.data, p.offset, static_cast<std::size_t>(p.bytes))) {
            error = "A DKC3 property offset is outside the ROM.";
            return false;
        }
        if (p.bit >= 0) {
            u8 v = rom.data[p.offset];
            if (p.value) v = static_cast<u8>(v | (1u << p.bit));
            else v = static_cast<u8>(v & ~(1u << p.bit));
            rom.data[p.offset] = v;
        } else if (p.bytes == 1) {
            rom.data[p.offset] = static_cast<u8>(p.value & 0xFF);
        } else if (p.bytes == 2) {
            wr16(rom.data, p.offset, static_cast<u16>(p.value & 0xFFFF));
        }
    }
    return true;
}

} // namespace dk
