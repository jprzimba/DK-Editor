#include "games/Dkc2.h"

#include "games/SpriteData.h"

#include <algorithm>
#include <array>
#include <cstring>

namespace dk {

namespace {

struct NameEntry {
    int id;
    const char* name;
};

static const NameEntry kNames[] = {
#include "games/Dkc2Levels.inc"
};

struct Dkc2Config {
    bool valid = false;
    int type = 0;
    int bonusType = 0;
    std::size_t style = 0;
    int mapIndex = 0;
    int mapId = 0;
    int graphics = 0;
    int routine1 = 0;
    int routine2 = 0;
    std::size_t palette = 0;
    std::string note;
};

struct Dkc2Ctx : LevelContext {
    int mapId = 0;
    int mapIndex = 0;
    int x0 = 0, x1 = 0, y0 = 0, y1 = 0;
    int stride = 16;
    bool vertical = false;
    std::size_t tilemapFile = 0;
    std::size_t compressedSize = 0;
    bool compressed = true;
    std::vector<u8> fullBuffer;
};

const char* nameFor(int id) {
    for (const auto& n : kNames) {
        if (n.id == id) return n.name;
    }
    return "";
}

std::string fallbackName(int id) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "Level %02X", id);
    return buf;
}

std::size_t snesToFile(u32 addr) {
    return static_cast<std::size_t>(addr & 0x3FFFFF);
}

bool readRecordGfx(const Rom& rom, int graphics, std::vector<u8>& data,
                   bool& compressed, std::string& error) {
    if (graphics < 0 || graphics > 0xFF) {
        error = "Invalid DKC2 graphics index.";
        return false;
    }
    const std::size_t base = 0x3D819A;
    const std::size_t listOff = base + static_cast<std::size_t>(graphics) * 2;
    if (!inRange(rom.data, listOff, 2)) {
        error = "Graphics pointer table is outside the ROM.";
        return false;
    }
    std::size_t pos = base + rom.word(listOff);
    for (int i = 0; i < 8; ++i) {
        if (!inRange(rom.data, pos, 7)) break;
        const u8 bank = rom.byte(pos);
        if (bank == 0) break;
        const u16 addr = rom.word(pos + 1);
        const u16 op = rom.word(pos + 3);
        const u16 size = rom.word(pos + 5);
        if (op == 0xA000 || op == 0x2000) {
            const std::size_t file = snesToFile((static_cast<u32>(bank) << 16) | addr);
            if (op == 0xA000) {
                compressed = true;
                return decompress(DkFormat::BigData, rom.data, file, data, error);
            }
            compressed = false;
            if (!rom.copyOut(file, size, data)) {
                error = "Uncompressed level graphics are outside the ROM.";
                return false;
            }
            return true;
        }
        pos += 7;
    }
    error = "No usable DKC2 graphics chunk found.";
    return false;
}

bool parseConfig(const Rom& rom, int id, Dkc2Config& cfg, std::string& error) {
    cfg = Dkc2Config{};
    error.clear();
    const std::size_t table = 0x3D0000;
    if (!inRange(rom.data, table + static_cast<std::size_t>(id) * 2, 2)) {
        error = "Level config table is outside the ROM.";
        return false;
    }
    const std::size_t base = table + rom.word(table + static_cast<std::size_t>(id) * 2);
    if (!inRange(rom.data, base, 4)) {
        error = "Level config points outside the ROM.";
        return false;
    }
    cfg.type = rom.word(base);
    if (cfg.type == 4 || cfg.type == 5) {
        cfg.note = "This entry has no editable level data.";
        return true;
    }
    std::size_t p = base + 2;
    if (cfg.type == 1) {
        if (!inRange(rom.data, p, 1)) return false;
        cfg.bonusType = rom.byte(p);
        ++p;
    }
    if (!inRange(rom.data, p, 3)) {
        error = "Truncated level config.";
        return false;
    }
    const u16 styleOff = rom.word(p);
    p += 2;
    cfg.mapIndex = rom.byte(p);
    cfg.style = table + styleOff;
    if (!inRange(rom.data, cfg.style, 21)) {
        cfg.note = "Level style is outside the ROM.";
        return true;
    }
    cfg.routine1 = rom.word(cfg.style);
    cfg.routine2 = rom.word(cfg.style + 2);
    cfg.palette = rom.word(cfg.style + 6);
    cfg.graphics = rom.byte(cfg.style + 13);
    cfg.mapId = rom.byte(cfg.style + 18);
    cfg.valid = cfg.mapId < 32;
    if (!cfg.valid) cfg.note = "Unsupported tilemap id.";
    return true;
}

bool findFreeSpace(const Rom& rom, std::size_t size, std::size_t& out) {
    const std::size_t need = size + 0x200;
    const std::size_t start = 0x100000;
    const std::size_t limit = rom.data.size();
    std::size_t best = 0;
    std::size_t i = start;
    while (i < limit) {
        if (rom.data[i] != 0x00) {
            ++i;
            continue;
        }
        std::size_t j = i;
        while (j < limit && rom.data[j] == 0x00) ++j;
        if (j - i >= need) {
            std::size_t cand = (i + 0xFF) & ~static_cast<std::size_t>(0xFF);
            if (cand + size <= j) best = cand; // prefer the highest usable run
        }
        i = j;
    }
    if (best == 0) return false;
    out = best;
    return true;
}

void loadFlatPalette(const Rom& rom, std::size_t offset, std::array<Color, 128>& out) {
    out.fill(Color{0, 0, 0, 0});
    for (int i = 0; i < 128; ++i) {
        const std::size_t o = offset + static_cast<std::size_t>(i) * 2;
        if (!inRange(rom.data, o, 2)) continue;
        const u16 raw = rd16(rom.data, o);
        Color c;
        c.r = static_cast<u8>((raw & 0x1F) << 3);
        c.g = static_cast<u8>(((raw >> 5) & 0x1F) << 3);
        c.b = static_cast<u8>(((raw >> 10) & 0x1F) << 3);
        c.a = (i % 16) == 0 ? 0 : 255;
        out[i] = c;
    }
}

void copyFlatPalette(const Rom& rom, std::size_t src, std::array<Color, 128>& dst, int dest,
                     int count) {
    if (dest < 0 || count <= 0) return;
    for (int i = 0; i < count && dest + i < 128; ++i) {
        const std::size_t o = src + static_cast<std::size_t>(i) * 2;
        if (!inRange(rom.data, o, 2)) break;
        const u16 raw = rd16(rom.data, o);
        Color c;
        c.r = static_cast<u8>((raw & 0x1F) << 3);
        c.g = static_cast<u8>(((raw >> 5) & 0x1F) << 3);
        c.b = static_cast<u8>(((raw >> 10) & 0x1F) << 3);
        c.a = ((dest + i) % 16) == 0 ? 0 : 255;
        dst[dest + i] = c;
    }
}

Palette loadLevelPalette(const Rom& rom, std::size_t base, int routine1) {
    std::array<Color, 128> flat{};
    loadFlatPalette(rom, base, flat);
    switch (routine1) {
    case 0x00: copyFlatPalette(rom, 0x3D1710, flat, 0, 128); break;
    case 0x01: copyFlatPalette(rom, 0x3D268E, flat, 16, 16); break;
    case 0x02: copyFlatPalette(rom, 0x3D2AEE, flat, 0, 128); break;
    case 0x03: copyFlatPalette(rom, 0x3D29EE, flat, 0, 128); break;
    case 0x04: copyFlatPalette(rom, 0x3D324E, flat, 0, 128); break;
    case 0x05: copyFlatPalette(rom, 0x3D304E, flat, 0, 128); break;
    case 0x06: copyFlatPalette(rom, 0x3D2EEE, flat, 0, 128); break;
    case 0x07: copyFlatPalette(rom, 0x3D15F0, flat, 12, 16); break;
    case 0x08: copyFlatPalette(rom, 0x3D0DD0, flat, 0, 16); break;
    case 0x0A: copyFlatPalette(rom, 0x3D326E, flat, 16, 16); break;
    case 0x0C: copyFlatPalette(rom, 0x3D2BEE, flat, 0, 128); break;
    case 0x0D: copyFlatPalette(rom, 0x3D07F0, flat, 0, 128); break;
    case 0x0E: copyFlatPalette(rom, 0x3D268E, flat, 112, 16); break;
    case 0x10: copyFlatPalette(rom, 0x3D3A4E, flat, 0, 128); break;
    case 0x12: copyFlatPalette(rom, 0x3D2DCE, flat, 0, 16); break;
    case 0x14: copyFlatPalette(rom, 0x3D1610, flat, 0, 128); break;
    default: break;
    }
    Palette pal{};
    for (int row = 0; row < 8; ++row) {
        for (int col = 0; col < 16; ++col) {
            pal[row][col] = flat[row * 16 + col];
        }
    }
    return pal;
}

struct Dkc2BgDef {
    u32 gfx;
    u32 tilemap;
    int bpp;
    bool transparentZero;
};

// DKC2 background (Layer 2) assets, addressed by file offset.
const Dkc2BgDef kBgShipDeck{0x29FDB3, 0x07FCB8, 4, false};   // ocean + sky
const Dkc2BgDef kBgShipMast{0x2A121C, 0x0AFABE, 4, false};   // clouds
const Dkc2BgDef kBgHaunted{0x295221, 0x0CFEDF, 4, false};    // ghost ship hall
const Dkc2BgDef kBgShipHold{0x29A905, 0x09FF37, 4, false};   // hull back wall
const Dkc2BgDef kBgBrambles{0x2AB6F0, 0x380000, 4, true};    // bramble silhouette
const Dkc2BgDef kBgHive{0x16F791, 0x14FC2D, 4, false};       // honeycomb cells
const Dkc2BgDef kBgForest{0x3B6FC0, 0x15FEB3, 4, true};      // forest trees
const Dkc2BgDef kBgCoaster{0x2A23D4, 0x0FFFD9, 4, false};    // carnival + track
const Dkc2BgDef kBgMine{0x2B2B84, 0x19F7C9, 2, true};        // cave sparkles
const Dkc2BgDef kBgRock{0x295623, 0x1CFA5E, 4, false};       // lava rock cave

const Dkc2BgDef* backgroundForLevel(int levelId) {
    switch (levelId) {
    case 0x03: case 0x04: case 0x05:      // Pirate Panic / Gangplank Galley / Rattle Battle
        return &kBgShipDeck;
    case 0x09: case 0x0B: case 0x0C: case 0x0D: case 0x1A: // mast-climbing stages
        return &kBgShipMast;
    case 0x10:                            // Haunted Hall
        return &kBgHaunted;
    case 0x0A: case 0x15:                 // Slime Climb / Lockjaw's Locker
        return &kBgShipHold;
    case 0x02: case 0x13:                 // Rambi Rumble / Parrot Chute Panic
        return &kBgBrambles;
    case 0x11:                            // Hornet Hole
        return &kBgHive;
    case 0x17: case 0x18: case 0x19:      // Web Woods / Gusty Glade / Ghostly Grove
        return &kBgForest;
    case 0x0E: case 0x0F:                 // Target Terror / Rickety Race
        return &kBgCoaster;
    case 0x07: case 0x08: case 0x14: case 0x16: // volcano and lava stages
        return &kBgRock;
    default:
        return nullptr;
    }
}

bool loadDkc2Background(const Rom& rom, int levelId, const Palette& palette,
                        LoadedLevel& out) {
    const Dkc2BgDef* def = backgroundForLevel(levelId);
    if (!def) return false;

    std::string error;
    std::vector<u8> gfxData;
    std::vector<u8> mapData;
    if (!decompress(DkFormat::BigData, rom.data, def->gfx, gfxData, error)) return false;
    if (!decompress(DkFormat::BigData, rom.data, def->tilemap, mapData, error)) return false;

    constexpr int kTilesW = 32;
    const int tilesH = static_cast<int>(mapData.size() / 64);
    if (tilesH < 4) return false;
    const Charset chars = decodeCharset(gfxData, def->bpp);
    if (chars.chars.empty()) return false;

    renderBackgroundTilemap(out.bgImage, kTilesW, tilesH, mapData, chars, palette,
                            def->transparentZero);
    if (out.bgImage.empty()) return false;
    out.bgWidth = kTilesW * 8;
    out.bgHeight = tilesH * 8;
    return true;
}

} // namespace

bool Dkc2Game::init(Rom& rom, std::string& error) {
    error.clear();
    levels_.clear();
    for (int id = 0; id < 192; ++id) {
        Dkc2Config cfg;
        std::string e;
        parseConfig(rom, id, cfg, e);
        if (cfg.valid) {
            std::string why;
            const std::size_t mapPtrOff = 0x35BB2E + static_cast<std::size_t>(cfg.mapId) * 3;
            const std::size_t tmPtrOff = 0x35BAEF + static_cast<std::size_t>(cfg.mapId) * 3;
            const std::size_t subOff = 0x35BC7E + static_cast<std::size_t>(cfg.mapId) * 2;
            if (!inRange(rom.data, mapPtrOff, 3) || rom.longptr(mapPtrOff) == 0) {
                why = "No map chip data for this tilemap.";
            } else if (!inRange(rom.data, tmPtrOff, 3) || rom.longptr(tmPtrOff) == 0) {
                why = "No tilemap data for this tilemap.";
            } else if (!inRange(rom.data, subOff, 2)) {
                why = "Missing submap table.";
            } else {
                const std::size_t sub = 0x350000 + rom.word(subOff);
                const std::size_t entry = sub + 4 + static_cast<std::size_t>(cfg.mapIndex) * 10;
                if (!inRange(rom.data, entry, 8)) {
                    why = "Submap entry is outside the ROM.";
                } else {
                    const int l = rom.word(entry);
                    const int r = rom.word(entry + 2);
                    const int t = rom.word(entry + 4);
                    const int b = rom.word(entry + 6);
                    if (r <= l || b <= t) why = "This entry has no playable map.";
                }
            }
            if (!why.empty()) {
                cfg.valid = false;
                cfg.note = why;
            }
        }
        LevelEntry entry;
        entry.id = id;
        const char* n = nameFor(id);
        entry.name = (n && *n) ? n : fallbackName(id);
        entry.supported = cfg.valid;
        entry.visual = cfg.valid;
        if (!cfg.valid) entry.note = cfg.note;
        const bool bonus = entry.name.find("Bonus") != std::string::npos ||
                           entry.name.find("Shortcut") != std::string::npos ||
                           entry.name.find("scene") != std::string::npos ||
                           entry.name.find("Exit") != std::string::npos;
        entry.group = bonus ? "Bonuses & Sub-rooms" : "Levels";
        levels_.push_back(std::move(entry));
    }
    return true;
}

std::string Dkc2Game::summary() const {
    return "DKC2 tilemaps use Big Data compression. Save recompresses the shared "
           "tilemap and relocates it when necessary.";
}

bool Dkc2Game::loadLevel(Rom& rom, int levelId, LoadedLevel& out, std::string& error) {
    error.clear();
    out = LoadedLevel{};
    if (levelId < 0 || levelId >= 192) {
        error = "Invalid DKC2 level id.";
        return false;
    }
    Dkc2Config cfg;
    if (!parseConfig(rom, levelId, cfg, error)) return false;
    if (!cfg.valid) {
        error = cfg.note.empty() ? "This DKC2 entry cannot be edited." : cfg.note;
        return false;
    }

    out.info.id = levelId;
    const char* n = nameFor(levelId);
    out.info.name = (n && *n) ? n : fallbackName(levelId);
    out.info.supported = true;
    out.info.visual = true;

    const int mapId = cfg.mapId;
    if (mapId < 0 || mapId >= 32) {
        error = "Invalid DKC2 tilemap id.";
        return false;
    }

    // 32x32 map chips.
    const std::size_t mapPtrOff = 0x35BB2E + static_cast<std::size_t>(mapId) * 3;
    if (!inRange(rom.data, mapPtrOff, 3)) {
        error = "Map chip pointer is outside the ROM.";
        return false;
    }
    const u32 mapChipAddr = rom.longptr(mapPtrOff);
    std::vector<u8> mapChipData;
    if (!decompress(DkFormat::BigData, rom.data, snesToFile(mapChipAddr), mapChipData, error)) {
        error = "Map chip decompression failed: " + error;
        return false;
    }
    out.metas = decodeMetatiles(mapChipData);
    if (out.metas.empty()) {
        error = "The level has no map chips.";
        return false;
    }

    // Level graphics.
    std::vector<u8> gfxData;
    bool gfxCompressed = false;
    if (!readRecordGfx(rom, cfg.graphics, gfxData, gfxCompressed, error)) return false;
    out.charset = decodeCharset(gfxData);
    if (out.charset.chars.empty()) {
        error = "The level graphics produced no tiles.";
        return false;
    }

    // Palette.
    std::size_t palAddr = cfg.palette ? (0x3D0000 + cfg.palette) : 0;
    if (palAddr == 0) {
        const std::size_t o = 0x35BC2A + static_cast<std::size_t>(mapId) * 2;
        if (inRange(rom.data, o, 2)) palAddr = 0x3D0000 | rom.word(o);
    }
    out.palette = loadLevelPalette(rom, palAddr, cfg.routine1);

    // Submap definition.
    const std::size_t subTable = 0x35BC7E;
    const std::size_t subOff = subTable + static_cast<std::size_t>(mapId) * 2;
    if (!inRange(rom.data, subOff, 2)) {
        error = "Submap table is outside the ROM.";
        return false;
    }
    const std::size_t sub = 0x350000 + rom.word(subOff);
    if (!inRange(rom.data, sub, 4 + static_cast<std::size_t>(cfg.mapIndex + 1) * 10)) {
        error = "Submap entry is outside the ROM.";
        return false;
    }
    const int mapX = rom.word(sub);
    const int mapY = rom.word(sub + 2);
    const std::size_t entry = sub + 4 + static_cast<std::size_t>(cfg.mapIndex) * 10;
    const int left = rom.word(entry);
    const int right = rom.word(entry + 2);
    const int top = rom.word(entry + 4);
    const int bottom = rom.word(entry + 6);
    const int code = rom.word(entry + 8);
    if (right <= left || bottom <= top) {
        error = "The level submap is empty.";
        return false;
    }

    // Tilemap.
    const std::size_t typeOff = 0x35BC54 + static_cast<std::size_t>(mapId) * 2;
    if (!inRange(rom.data, typeOff, 2)) {
        error = "Tilemap type table is outside the ROM.";
        return false;
    }
    const int tilemapType = rom.word(typeOff);
    const std::size_t tmPtrOff = 0x35BAEF + static_cast<std::size_t>(mapId) * 3;
    if (!inRange(rom.data, tmPtrOff, 3)) {
        error = "Tilemap pointer is outside the ROM.";
        return false;
    }
    const std::size_t tilemapFile = snesToFile(rom.longptr(tmPtrOff));

    std::vector<u8> tmBuffer;
    bool compressed = (tilemapType & 0x0400) != 0;
    if (compressed) {
        if (!decompress(DkFormat::BigData, rom.data, tilemapFile, tmBuffer, error)) {
            error = "Tilemap decompression failed: " + error;
            return false;
        }
    } else {
        if (!rom.copyOut(tilemapFile, 256, tmBuffer)) {
            error = "Uncompressed tilemap is outside the ROM.";
            return false;
        }
    }

    const bool vertical = (code & 0x40) != 0 || (code >= 4 && code <= 7);
    const int stride = vertical ? (mapX / 32) : (mapY / 32);
    if (stride <= 0) {
        error = "The tilemap has an invalid stride.";
        return false;
    }

    const int x0 = left / 32, x1 = right / 32;
    const int y0 = top / 32, y1 = bottom / 32;
    out.width = x1 - x0;
    out.height = y1 - y0;
    if (out.width <= 0 || out.height <= 0) {
        error = "The level has invalid dimensions.";
        return false;
    }
    out.vertical = vertical;
    out.indexMask = 0x0FFF;
    out.background = Color{6, 8, 16, 255};
    out.tiles.assign(static_cast<std::size_t>(out.width) * out.height, 0);

    for (int y = 0; y < out.height; ++y) {
        for (int x = 0; x < out.width; ++x) {
            const int ax = x0 + x;
            const int ay = y0 + y;
            const std::size_t ti = vertical
                ? static_cast<std::size_t>(ax) + static_cast<std::size_t>(ay) * stride
                : static_cast<std::size_t>(ax) * stride + static_cast<std::size_t>(ay);
            const std::size_t o = ti * 2;
            if (o + 1 < tmBuffer.size()) {
                out.tiles[static_cast<std::size_t>(y) * out.width + x] =
                    static_cast<u16>(tmBuffer[o] | (tmBuffer[o + 1] << 8));
            }
        }
    }

    auto ctx = std::make_shared<Dkc2Ctx>();
    ctx->mapId = mapId;
    ctx->mapIndex = cfg.mapIndex;
    ctx->x0 = x0;
    ctx->x1 = x1;
    ctx->y0 = y0;
    ctx->y1 = y1;
    ctx->stride = stride;
    ctx->vertical = vertical;
    ctx->tilemapFile = tilemapFile;
    ctx->compressed = compressed;
    ctx->compressedSize = compressed
        ? compressedSize(DkFormat::BigData, rom.data, tilemapFile)
        : 256;
    ctx->fullBuffer = std::move(tmBuffer);
    out.context = ctx;

    Color bgTop = out.palette[0][1];
    if (bgTop.a == 0 || (static_cast<int>(bgTop.r) + bgTop.g + bgTop.b) < 24) {
        bgTop = Color{18, 30, 55, 255};
    }
    out.background = bgTop;
    out.backgroundBottom = Color{static_cast<u8>(bgTop.r * 2 / 5),
                                static_cast<u8>(bgTop.g * 2 / 5),
                                static_cast<u8>(bgTop.b * 2 / 5), 255};
    loadDkc2Background(rom, levelId, out.palette, out);

    buildLevelImage(out.image, out.width, out.height, out.tiles, out.indexMask,
                    out.metas, out.charset, out.palette, out.background,
                    out.backgroundBottom, out.bgImage, out.bgWidth, out.bgHeight);
    loadSpriteBlock(rom, levelId, out);
    return true;
}

bool Dkc2Game::saveLevel(Rom& rom, const LoadedLevel& level, std::string& error) {
    error.clear();
    auto ctx = std::dynamic_pointer_cast<Dkc2Ctx>(level.context);
    if (!ctx) {
        error = "No DKC2 save context.";
        return false;
    }
    if (!ctx->compressed) {
        error = "This DKC2 tilemap is stored uncompressed and is not supported for saving.";
        return false;
    }

    // Copy the edited region back into the shared tilemap buffer.
    for (int y = 0; y < level.height; ++y) {
        for (int x = 0; x < level.width; ++x) {
            const int ax = ctx->x0 + x;
            const int ay = ctx->y0 + y;
            const std::size_t ti = ctx->vertical
                ? static_cast<std::size_t>(ax) + static_cast<std::size_t>(ay) * ctx->stride
                : static_cast<std::size_t>(ax) * ctx->stride + static_cast<std::size_t>(ay);
            const std::size_t o = ti * 2;
            if (o + 1 >= ctx->fullBuffer.size()) continue;
            const u16 v = level.tiles[static_cast<std::size_t>(y) * level.width + x];
            ctx->fullBuffer[o] = static_cast<u8>(v & 0xFF);
            ctx->fullBuffer[o + 1] = static_cast<u8>(v >> 8);
        }
    }

    std::vector<u8> packed;
    if (!compress(DkFormat::BigData, ctx->fullBuffer, packed, error)) {
        error = "Tilemap compression failed: " + error;
        return false;
    }

    std::size_t writeAt = ctx->tilemapFile;
    std::size_t slot = ctx->compressedSize;
    if (packed.size() > slot) {
        if (!findFreeSpace(rom, packed.size(), writeAt)) {
            error = "The recompressed tilemap is larger than the original and the ROM "
                    "has no free space large enough.";
            return false;
        }
        slot = packed.size();
    }

    // Always point the map to the data we just wrote.  This also keeps a
    // previously relocated tilemap consistent if the user saves twice.
    const std::size_t ptrOff = 0x35BAEF + static_cast<std::size_t>(ctx->mapId) * 3;
    if (!inRange(rom.data, ptrOff, 3)) {
        error = "Could not update the DKC2 tilemap pointer.";
        return false;
    }
    wr24(rom.data, ptrOff, static_cast<u24>(writeAt + 0xC00000));

    if (!inRange(rom.data, writeAt, packed.size())) {
        error = "Could not write the DKC2 tilemap.";
        return false;
    }
    std::memcpy(rom.data.data() + writeAt, packed.data(), packed.size());
    if (slot > packed.size()) {
        std::memset(rom.data.data() + writeAt + packed.size(), 0, slot - packed.size());
    }

    ctx->tilemapFile = writeAt;
    ctx->compressedSize = slot;
    return saveSpriteBlock(rom, level, error);
}

} // namespace dk
