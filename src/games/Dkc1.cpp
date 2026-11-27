#include "games/Dkc1.h"

#include <algorithm>
#include <cstring>

namespace dk {

namespace {

struct Dkc1LevelDef {
    const char* name;
    int code;
    std::size_t offset;
    std::size_t size;
    int header;
    std::size_t palette;
    int tileset;
    bool vertical;
};

static const Dkc1LevelDef kLevels[] = {
#include "games/Dkc1Levels.inc"
};

constexpr int kHeaderCount = 14;

const char* kThemeNames[kHeaderCount] = {
    "Jungle",       "Caves",      "Walkways",  "Mines",      "Factories",
    "Underwater",   "Temples",    "Snow",      "Crystal Caves", "Mine Carts",
    "Treetops",     "Forests",    "Gangplank", "Boss Lairs",
};

const std::size_t kMap32Base[kHeaderCount] = {
    0x19A3C0, 0x1ABF00, 0x0BE040, 0x1BA0CC, 0x226200, 0x100000, 0x2C7E28,
    0x20AAD6, 0x213BE5, 0x26A188, 0x035200, 0x03BD00, 0x230200, 0x03EFA0,
};

const int kMetaCount[kHeaderCount] = {
    0x24A, 0x1B4, 0x0FE, 0x15C, 0x0D5, 0x23B, 0x123,
    0x182, 0x189, 0x0E6, 0x080, 0x185, 0x07C, 0x07A,
};

const Color kThemeTop[kHeaderCount] = {
    {34, 92, 170, 255},  {14, 12, 10, 255},  {20, 26, 34, 255},
    {18, 16, 14, 255},   {18, 18, 26, 255},  {8, 30, 64, 255},
    {30, 28, 16, 255},   {70, 105, 160, 255},{14, 22, 44, 255},
    {12, 10, 9, 255},    {32, 82, 130, 255}, {10, 28, 18, 255},
    {16, 12, 26, 255},   {20, 12, 26, 255},
};

const Color kThemeBottom[kHeaderCount] = {
    {172, 210, 235, 255}, {40, 30, 22, 255},  {60, 70, 82, 255},
    {52, 40, 30, 255},    {60, 45, 38, 255},  {2, 10, 26, 255},
    {75, 65, 32, 255},    {205, 225, 245, 255},{55, 85, 120, 255},
    {40, 32, 26, 255},    {120, 180, 210, 255},{45, 80, 40, 255},
    {45, 35, 60, 255},    {60, 30, 60, 255},
};

const Dkc1LevelDef* findLevel(int id) {
    if (id < 0 || id >= static_cast<int>(std::size(kLevels))) return nullptr;
    return &kLevels[id];
}

u8 table8(const Rom& rom, u32 snes) {
    const std::size_t o = snToFile(snes);
    return o < rom.data.size() ? rom.data[o] : 0;
}

u16 table16(const Rom& rom, u32 snes) {
    const std::size_t o = snToFile(snes);
    return inRange(rom.data, o, 2) ? rd16(rom.data, o) : 0;
}

u32 resolveTileset(const Rom& rom, int index, int levelCode) {
    if (index == 0xFF) return 0xE73BDE;
    if (index == 0xF0) return 0x0F0000;

    int x = table16(rom, 0xB9A994 + static_cast<u32>(index) * 2);
    for (int guard = 0; guard < 4096; ++guard) {
        const u8 a = table8(rom, 0xB9A994 + static_cast<u32>(x));
        if (a == 0) return 0;
        if (table8(rom, 0xB9A998 + static_cast<u32>(x)) >= 0x80) {
            const u16 y = table16(rom, 0xB9A995 + static_cast<u32>(x));
            const u16 hi = table16(rom, 0xB9A994 + static_cast<u32>(x)) & 0xFF;
            (void)levelCode;
            return (static_cast<u32>(hi) << 16) | y;
        }
        x += 7;
    }
    return 0;
}

bool prepareCharset(Rom& rom, int code, int tilesetIndex, Charset& out,
                    std::string& error) {
    std::vector<u8> gfx;
    if (code == 0x68) {
        if (!rom.copyOut(snToFile(0x0F0000), 0x8500, gfx)) {
            error = "Gangplank Galleon tileset is outside the ROM.";
            return false;
        }
    } else {
        const u32 addr = resolveTileset(rom, tilesetIndex, code);
        if (addr == 0) {
            error = "Could not resolve the DKC1 tileset.";
            return false;
        }
        if (!decompress(DkFormat::DkcChr, rom.data, snToFile(addr), gfx, error)) {
            error = "DKC1 tileset decompression failed: " + error;
            return false;
        }
    }

    // A few level types use a shifted tileset (extra animated/background tiles).
    auto replaceFront = [](std::vector<u8>& d, std::size_t skip, std::size_t pad) {
        if (d.size() <= skip) return;
        std::vector<u8> n(pad, 0);
        n.insert(n.end(), d.begin() + static_cast<std::ptrdiff_t>(skip), d.end());
        d.swap(n);
    };

    const int specialA[] = {0x42, 0x43, 0x55, 0x56, 0x57, 0x52, 0x53};
    const int specialB[] = {0x24, 0xA7, 0x67, 0x66, 0x69, 0x6A, 0x6B};
    const bool isA = std::find(std::begin(specialA), std::end(specialA), code) != std::end(specialA);
    const bool isB = std::find(std::begin(specialB), std::end(specialB), code) != std::end(specialB);

    if (isA) {
        std::vector<u8> sub;
        if (rom.copyOut(snToFile(0xDBCCD2), 0x22C0, sub) && gfx.size() > 0x20) {
            std::vector<u8> n = sub;
            n.insert(n.end(), gfx.begin() + 0x20, gfx.end());
            n.resize(n.size() + 0x0F20, 0);
            gfx.swap(n);
        }
    } else if (isB) {
        replaceFront(gfx, 0x20, 0x20);
        gfx.resize(gfx.size() + 0x2000, 0);
    }

    out = decodeCharset(gfx);
    if (out.chars.empty()) {
        error = "The DKC1 tileset produced no tiles.";
        return false;
    }
    return true;
}

} // namespace

bool Dkc1Game::init(Rom& rom, std::string& error) {
    (void)rom;
    error.clear();
    levels_.clear();
    int id = 0;
    for (const auto& def : kLevels) {
        LevelEntry e;
        e.id = id++;
        e.name = def.name;
        e.group = (def.header >= 0 && def.header < kHeaderCount) ? kThemeNames[def.header]
                                                                  : "Other";
        e.supported = true;
        e.visual = true;
        levels_.push_back(std::move(e));
    }
    return true;
}

std::string Dkc1Game::summary() const {
    return "DKC1 levels are uncompressed. Save writes the tilemap in place.";
}

bool Dkc1Game::loadLevel(Rom& rom, int levelId, LoadedLevel& out, std::string& error) {
    error.clear();
    out = LoadedLevel{};
    const Dkc1LevelDef* def = findLevel(levelId);
    if (!def) {
        error = "Invalid DKC1 level id.";
        return false;
    }
    if (!inRange(rom.data, def->offset, def->size)) {
        error = "Level data is outside the ROM.";
        return false;
    }

    out.info.id = levelId;
    out.info.name = def->name;
    out.info.supported = true;
    out.info.visual = true;
    out.vertical = def->vertical;
    out.indexMask = 0x3FF;
    if (def->header >= 0 && def->header < kHeaderCount) {
        out.background = kThemeTop[def->header];
        out.backgroundBottom = kThemeBottom[def->header];
    } else {
        out.background = Color{12, 16, 28, 255};
        out.backgroundBottom = Color{4, 6, 12, 255};
    }

    if (def->vertical) {
        out.width = 64;
        out.height = static_cast<int>(def->size / 128);
    } else {
        out.width = static_cast<int>(def->size / 32);
        out.height = 16;
    }
    if (out.width <= 0 || out.height <= 0) {
        error = "The level has invalid dimensions.";
        return false;
    }

    out.tiles.assign(static_cast<std::size_t>(out.width) * out.height, 0);
    if (def->vertical) {
        for (int y = 0; y < out.height; ++y) {
            for (int x = 0; x < out.width; ++x) {
                const std::size_t o = def->offset + static_cast<std::size_t>(y * 64 + x) * 2;
                out.tiles[static_cast<std::size_t>(y) * out.width + x] = rd16(rom.data, o);
            }
        }
    } else {
        for (int x = 0; x < out.width; ++x) {
            for (int y = 0; y < out.height; ++y) {
                const std::size_t o = def->offset + static_cast<std::size_t>(x * 16 + y) * 2;
                out.tiles[static_cast<std::size_t>(y) * out.width + x] = rd16(rom.data, o);
            }
        }
    }

    if (def->header < 0 || def->header >= kHeaderCount) {
        error = "Unknown DKC1 theme.";
        return false;
    }

    const std::size_t metaBytes = static_cast<std::size_t>(kMetaCount[def->header]) * 32;
    std::vector<u8> metaData;
    if (!rom.copyOut(kMap32Base[def->header], metaBytes, metaData)) {
        error = "Metatile data is outside the ROM.";
        return false;
    }
    out.metas = decodeMetatiles(metaData);

    out.palette = loadPalette(rom.data, def->palette);
    if (!prepareCharset(rom, def->code, def->tileset, out.charset, error)) {
        return false;
    }

    buildLevelImage(out.image, out.width, out.height, out.tiles, out.indexMask,
                    out.metas, out.charset, out.palette, out.background,
                    out.backgroundBottom);
    return true;
}

bool Dkc1Game::saveLevel(Rom& rom, const LoadedLevel& level, std::string& error) {
    error.clear();
    const Dkc1LevelDef* def = findLevel(level.info.id);
    if (!def) {
        error = "Invalid DKC1 level id.";
        return false;
    }
    const std::size_t expected = static_cast<std::size_t>(level.width) * level.height * 2;
    if (expected != def->size) {
        error = "Level dimensions do not match the original ROM data.";
        return false;
    }
    if (!inRange(rom.data, def->offset, def->size)) {
        error = "Level data is outside the ROM.";
        return false;
    }

    if (def->vertical) {
        for (int y = 0; y < level.height; ++y) {
            for (int x = 0; x < level.width; ++x) {
                const std::size_t o = def->offset + static_cast<std::size_t>(y * 64 + x) * 2;
                wr16(rom.data, o, level.tiles[static_cast<std::size_t>(y) * level.width + x]);
            }
        }
    } else {
        for (int x = 0; x < level.width; ++x) {
            for (int y = 0; y < level.height; ++y) {
                const std::size_t o = def->offset + static_cast<std::size_t>(x * 16 + y) * 2;
                wr16(rom.data, o, level.tiles[static_cast<std::size_t>(y) * level.width + x]);
            }
        }
    }
    return true;
}

} // namespace dk
