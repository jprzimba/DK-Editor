#include "games/SpriteData.h"

#include <algorithm>
#include <cstring>

namespace dk {

namespace {

constexpr std::size_t kRecordSize = 8;
constexpr std::size_t kMaxSprites = 2048;

std::size_t blockOffset(const Rom& rom, int levelId) {
    const std::size_t entry = kSpriteTableFile + static_cast<std::size_t>(levelId) * 2;
    if (!inRange(rom.data, entry, 2)) return 0;
    return rom.word(entry);
}

} // namespace

bool loadSpriteBlock(const Rom& rom, int levelId, LoadedLevel& out) {
    out.sprites.clear();
    out.spriteTableEntry = kSpriteTableFile + static_cast<std::size_t>(levelId) * 2;
    out.spriteTableBank = kSpriteBankFile;
    out.spriteLimit = 0;
    if (!inRange(rom.data, out.spriteTableEntry, 2)) return false;

    const std::size_t offset = blockOffset(rom, levelId);
    if (offset == 0) return false;
    const std::size_t block = kSpriteBankFile + offset;
    if (!inRange(rom.data, block, 2)) return false;

    for (std::size_t i = 0; i < kMaxSprites; ++i) {
        const std::size_t pos = block + i * kRecordSize;
        if (!inRange(rom.data, pos, kRecordSize)) break;
        SpriteObject obj;
        obj.id = rom.word(pos);
        if (obj.id == 0) {
            out.spriteLimit = pos + 2;
            return true;
        }
        obj.x = static_cast<u16>(rom.word(pos + 2) >= 0x100 ? rom.word(pos + 2) - 0x100 : 0);
        obj.y = static_cast<u16>(rom.word(pos + 4) >= 0x100 ? rom.word(pos + 4) - 0x100 : 0);
        obj.extra = rom.word(pos + 6);
        obj.offset = pos;
        out.sprites.push_back(obj);
    }
    out.spriteLimit = block;
    return true;
}

bool saveSpriteBlock(Rom& rom, const LoadedLevel& level, std::string& error) {
    error.clear();
    if (level.spriteTableEntry == 0) return true;  // game without sprite support

    const std::size_t bytes = (level.sprites.size() + 1) * kRecordSize;
    if (level.sprites.size() >= kMaxSprites) {
        error = "Too many sprites in this level.";
        return false;
    }

    // Serialize the list.
    std::vector<u8> data(bytes, 0);
    for (std::size_t i = 0; i < level.sprites.size(); ++i) {
        const SpriteObject& s = level.sprites[i];
        const std::size_t p = i * kRecordSize;
        wr16(data, p, s.id);
        wr16(data, p + 2, static_cast<u16>(std::min<int>(0xFFFE, s.x + 0x100)));
        wr16(data, p + 4, static_cast<u16>(std::min<int>(0xFFFE, s.y + 0x100)));
        wr16(data, p + 6, s.extra);
    }
    // Terminator: 8-byte zero record keeps later writes aligned.
    const std::size_t last = level.sprites.size() * kRecordSize;
    wr16(data, last, 0);
    wr16(data, last + 2, 0);

    const std::size_t oldOffset = blockOffset(rom, level.spriteTableEntry >= kSpriteTableFile
                                                       ? static_cast<int>((level.spriteTableEntry -
                                                                           kSpriteTableFile) / 2)
                                                       : 0);
    const std::size_t oldBlock = kSpriteBankFile + oldOffset;

    // Reuse the original slot when the list still fits inside the old run.
    std::size_t oldSize = 0;
    if (oldOffset != 0) {
        for (std::size_t i = 0; i < kMaxSprites; ++i) {
            const std::size_t pos = oldBlock + i * kRecordSize;
            if (!inRange(rom.data, pos, 2)) break;
            oldSize = (i + 1) * kRecordSize;
            if (rom.word(pos) == 0) break;
        }
    }
    std::size_t target = 0;
    if (oldOffset != 0 && data.size() <= oldSize) {
        target = oldBlock;
    } else {
        // Find a zero-filled run inside the sprite bank that does not overlap
        // any other block in the table.
        const std::size_t bankEnd = kSpriteBankFile + kSpriteBankSize;
        const std::size_t need = data.size() + 8;
        std::size_t best = 0;
        for (std::size_t p = kSpriteBankFile + 0x400; p + need <= bankEnd; ++p) {
            if (rom.data[p] != 0) continue;
            std::size_t run = 0;
            while (p + run < bankEnd && rom.data[p + run] == 0) ++run;
            if (run < need) {
                p += run;
                continue;
            }
            // Reject runs that start inside another level's block.
            bool clash = false;
            for (int id = 0; id < 256 && !clash; ++id) {
                const std::size_t entry = kSpriteTableFile + static_cast<std::size_t>(id) * 2;
                if (!inRange(rom.data, entry, 2)) continue;
                const std::size_t off = rom.word(entry);
                if (off == 0) continue;
                const std::size_t start = kSpriteBankFile + off;
                if (p >= start && p < start + 4) clash = true;
            }
            if (!clash) {
                best = p;
                break;
            }
            p += std::max<std::size_t>(run, 2);
        }
        if (best == 0) {
            error = "No free space in the sprite bank for this level.";
            return false;
        }
        target = best;
    }

    if (!inRange(rom.data, target, data.size())) {
        error = "Sprite data would be written outside the ROM.";
        return false;
    }
    std::memcpy(rom.data.data() + target, data.data(), data.size());
    wr16(rom.data, level.spriteTableEntry,
         static_cast<u16>(target - kSpriteBankFile));
    return true;
}

} // namespace dk
