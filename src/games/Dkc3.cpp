#include "games/Dkc3.h"

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
    return "DKC3 stage settings and entrances can be edited. Visual tilemap "
           "editing is planned for the next build.";
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
