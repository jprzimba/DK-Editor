#include "games/Dkc3.h"

#include "core/Compression.h"
#include "core/Graphics.h"
#include "games/SpriteData.h"

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
    std::size_t mapSize = 0;  // bytes when the map is stored raw
    std::size_t gfx = 0;      // file offset of the tileset
    std::size_t gfxSize = 0;  // bytes when the tileset is stored raw
};

struct Dkc3BgSet {
    Dkc3BgLayer back;
    Dkc3BgLayer front;
};

// Validated DKC3 backgrounds (tilemap + tileset) per stage theme.  The map and
// the tileset may be stored with different codecs; the loader tries them in
// order (DKC3 tilemap, Big Data, Small Data, raw).
const Dkc3BgSet kBgNone{};
const Dkc3BgSet kBgCave{
    {}, {0x3275D5, 0, 0x32587C}};  // TM/GFX_Layer2_CaveLevels
const Dkc3BgSet kBgForest{
    {}, {0x2712B8, 0, 0x2717A6}};  // TM/GFX_Layer2_ForestLevels
const Dkc3BgSet kBgRiver{
    {}, {0x288388, 0, 0x28861B}};  // TM/GFX_Layer2_RiverbankLevels
const Dkc3BgSet kBgMill{
    {}, {0x1DF873, 0, 0x1DFE8F}};  // TM/GFX_Layer2_MillLevels
const Dkc3BgSet kBgMountain{
    {}, {0x2A5F1C, 0, 0x2A426A}};  // TM/GFX_Layer2_MountainLevels
const Dkc3BgSet kBgFactory{
    {}, {0x29B0BD, 0, 0x2C576C, 0x1F60}};  // TM_Layer3_Factory + raw GFX_Layer2_Factory
const Dkc3BgSet kBgDrain{
    {}, {0x2A7D8A, 0x800, 0x2A68F2}};  // TM/GFX_Layer2_DrainPipeLevels
const Dkc3BgSet kBgPier{
    {}, {0x2591C9, 0, 0x254C41}};  // TM_Layer1_PierLevelWaterllineAndMountains

const Dkc3BgSet& backgroundForStage(int id) {
    switch (id) {
    case 0x25: case 0x26: case 0x27:  // lakeside / pier stages
        return kBgPier;
    case 0x28: case 0x29: case 0x2A:  // mill stages
    case 0x47: case 0x48: case 0x49:  // mill stages (world 7)
        return kBgMill;
    case 0x2E: case 0x2F: case 0x30: case 0x31:  // forest stages
        return kBgForest;
    case 0x32: case 0x33: case 0x34:  // riverbank stages
        return kBgRiver;
    case 0x38: case 0x39: case 0x3A:  // factory stages (world 3)
    case 0x3B: case 0x3C: case 0x3D:  // factory stages (world 6)
        return kBgNone;               // art still being mapped
    case 0x3E:                        // Low-G Labyrinth
    case 0x41: case 0x42: case 0x43:  // mountain stages
    case 0x4A: case 0x4B: case 0x4C:  // mountain stages (late)
        return kBgMountain;
    case 0x3F: case 0x40:  // pipe stages
        return kBgDrain;
    case 0x35: case 0x36: case 0x37:  // reef stages (art still being mapped)
        return kBgNone;
    case 0x44: case 0x45: case 0x46:  // cave stages
        return kBgCave;
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

// Loads a background tilemap, trying the DKC3 tilemap format first and then
// Big Data, Small Data and finally a raw 16-bit array.
bool loadBackgroundMap(const Rom& rom, const Dkc3BgLayer& layer, std::vector<u16>& words) {
    if (layer.map == 0 || !inRange(rom.data, layer.map, 16)) return false;
    std::string error;
    if (decompressTilemapDkc3(rom.data, layer.map, words, error) && !words.empty()) {
        return true;
    }
    std::vector<u8> bytes;
    if (decompress(DkFormat::BigData, rom.data, layer.map, bytes, error) &&
        bytes.size() >= 64) {
        words.resize(bytes.size() / 2);
        for (std::size_t i = 0; i < words.size(); ++i) {
            words[i] = static_cast<u16>(bytes[i * 2] | (bytes[i * 2 + 1] << 8));
        }
        return true;
    }
    bytes.clear();
    error.clear();
    if (decompress(DkFormat::SmallData, rom.data, layer.map, bytes, error) &&
        bytes.size() >= 64) {
        words.resize(bytes.size() / 2);
        for (std::size_t i = 0; i < words.size(); ++i) {
            words[i] = static_cast<u16>(bytes[i * 2] | (bytes[i * 2 + 1] << 8));
        }
        return true;
    }
    if (layer.mapSize >= 64) {
        const std::size_t bytesCount = std::min<std::size_t>(layer.mapSize, rom.data.size());
        if (!inRange(rom.data, layer.map, bytesCount)) return false;
        words.resize(bytesCount / 2);
        for (std::size_t i = 0; i < words.size(); ++i) {
            words[i] = rd16(rom.data, layer.map + i * 2);
        }
        return true;
    }
    return false;
}

// Draws one background layer into an existing RGBA image.  Index-0 pixels stay
// transparent so lower layers and the sky gradient show through.
bool drawBackgroundLayer(const Rom& rom, const Dkc3BgLayer& layer, const Palette& palette,
                         std::vector<u32>& image, int width, int height) {
    if (layer.map == 0 || layer.gfx == 0) return false;

    std::vector<u16> words;
    if (!loadBackgroundMap(rom, layer, words)) return false;
    if (words.size() < static_cast<std::size_t>(width)) return false;

    std::string error;
    std::vector<u8> gfxData;
    bool gfxOk = false;
    if (layer.gfxSize != 0) {
        const std::size_t size = std::min<std::size_t>(layer.gfxSize, rom.data.size());
        if (inRange(rom.data, layer.gfx, size)) {
            gfxData.assign(rom.data.begin() + static_cast<std::ptrdiff_t>(layer.gfx),
                           rom.data.begin() + static_cast<std::ptrdiff_t>(layer.gfx + size));
            gfxOk = true;
        }
    } else {
        gfxOk = decompress(DkFormat::BigData, rom.data, layer.gfx, gfxData, error) &&
                gfxData.size() >= 32;
        if (!gfxOk) {
            gfxData.clear();
            error.clear();
            gfxOk = decompress(DkFormat::SmallData, rom.data, layer.gfx, gfxData, error) &&
                    gfxData.size() >= 32;
        }
    }
    if (!gfxOk) return false;
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
                     std::vector<u32>& image, int& width, int& height, bool& hasArt) {
    const Dkc3BgSet& set = backgroundForStage(levelId);

    std::vector<u16> probe;
    int words = 0;
    hasArt = false;
    if (set.front.map != 0 && loadBackgroundMap(rom, set.front, probe)) {
        words = static_cast<int>(probe.size());
        hasArt = words >= 32;
    } else if (set.back.map != 0 && loadBackgroundMap(rom, set.back, probe)) {
        words = static_cast<int>(probe.size());
        hasArt = words >= 32;
    }
    if (!hasArt) words = 32 * 32;  // fall back to one gradient screen

    constexpr int kTileWidth = 32;
    const int rows = std::max(1, words / kTileWidth);
    const int pixelsW = kTileWidth * 8;
    int pixelsH = rows * 8;
    // The editor works in 32x32 cells, so pad the height to a whole cell.
    pixelsH += (32 - (pixelsH % 32)) % 32;

    image.assign(static_cast<std::size_t>(pixelsW) * pixelsH, 0);
    // Themed sky gradient behind transparent parts.
    Color top{24, 44, 82, 255};
    Color bottom{96, 150, 204, 255};
    if (levelId >= 0x2B && levelId <= 0x2D) {  // snow
        top = Color{150, 176, 208, 255};
        bottom = Color{238, 246, 255, 255};
    } else if (levelId >= 0x35 && levelId <= 0x37) {  // reef
        top = Color{8, 52, 78, 255};
        bottom = Color{56, 156, 168, 255};
    } else if (levelId >= 0x3F && levelId <= 0x40) {  // pipes
        top = Color{28, 8, 40, 255};
        bottom = Color{96, 44, 120, 255};
    } else if (levelId >= 0x44 && levelId <= 0x46) {  // cave
        top = Color{20, 12, 8, 255};
        bottom = Color{70, 46, 34, 255};
    } else if (levelId >= 0x28 && levelId <= 0x2A) {  // mill
        top = Color{18, 14, 10, 255};
        bottom = Color{80, 60, 40, 255};
    } else if ((levelId >= 0x32 && levelId <= 0x34) ||
               (levelId >= 0x2E && levelId <= 0x31)) {  // river / forest
        top = Color{30, 60, 40, 255};
        bottom = Color{120, 180, 120, 255};
    }
    for (int y = 0; y < pixelsH; ++y) {
        const float t = pixelsH > 1 ? static_cast<float>(y) / (pixelsH - 1) : 0.0f;
        const Color c{static_cast<u8>(top.r + (bottom.r - top.r) * t),
                      static_cast<u8>(top.g + (bottom.g - top.g) * t),
                      static_cast<u8>(top.b + (bottom.b - top.b) * t), 255};
        for (int x = 0; x < pixelsW; ++x) {
            image[static_cast<std::size_t>(y) * pixelsW + x] = packColor(c);
        }
    }

    if (hasArt) {
        drawBackgroundLayer(rom, set.back, palette, image, kTileWidth, pixelsH);
        drawBackgroundLayer(rom, set.front, palette, image, kTileWidth, pixelsH);
    }

    // Repeat the backdrop horizontally so the canvas shows a level-wide view.
    constexpr int kRepeat = 2;
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

    // Optional background preview.  Every stage gets an image; themes whose
    // art is not mapped yet receive a themed gradient instead.
    const int paletteId = rom.byte(a + 6);
    out.palette = loadFgbgPalette(rom, paletteId);
    loadSpriteBlock(rom, levelId, out);

    int bgWidth = 0, bgHeight = 0;
    bool hasArt = false;
    if (buildBackground(rom, levelId, out.palette, out.image, bgWidth, bgHeight, hasArt)) {
        // Grow the canvas so every placed sprite is inside the map view.
        int extentW = bgWidth;
        int extentH = bgHeight;
        for (const SpriteObject& s : out.sprites) {
            extentW = std::max(extentW, static_cast<int>(s.x) + 96);
            extentH = std::max(extentH, static_cast<int>(s.y) + 96);
        }
        extentW = std::min(16384, (extentW + 31) / 32 * 32);
        extentH = std::min(16384, (extentH + 31) / 32 * 32);
        if (extentW > bgWidth || extentH > bgHeight) {
            std::vector<u32> grown(static_cast<std::size_t>(extentW) * extentH, 0);
            for (int y = 0; y < extentH; ++y) {
                const int srcY = std::min(bgHeight - 1, y * bgHeight / extentH);
                for (int x = 0; x < extentW; ++x) {
                    grown[static_cast<std::size_t>(y) * extentW + x] =
                        out.image[static_cast<std::size_t>(srcY) * bgWidth + (x % bgWidth)];
                }
            }
            out.image.swap(grown);
            bgWidth = extentW;
            bgHeight = extentH;
        }
        out.width = bgWidth / 32;
        out.height = bgHeight / 32;
        out.info.note = hasArt
            ? "Background preview; sprites can be moved with the Sprites tool."
            : "Themed backdrop; this stage's background art is still being mapped.";
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
    return saveSpriteBlock(rom, level, error);
}

} // namespace dk
