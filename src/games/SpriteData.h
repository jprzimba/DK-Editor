#pragma once

#include "games/Game.h"

#include <string>

namespace dk {

// DKC2/DKC3 store per-level sprite lists in bank $FE.  A 16-bit table at
// $FE0000 maps the level id to a block offset inside the bank.  Each object is
// an 8-byte record (id, x, y, extra) terminated by an id of zero.
constexpr std::size_t kSpriteTableFile = 0x3E0000;
constexpr std::size_t kSpriteBankFile = 0x3E0000;
constexpr std::size_t kSpriteBankSize = 0x10000;

bool loadSpriteBlock(const Rom& rom, int levelId, LoadedLevel& out);

// Writes the level's current sprite list back to the ROM.  The block is written
// in place when it still fits; otherwise it is relocated to a free zero run in
// the same bank and the table entry is updated.
bool saveSpriteBlock(Rom& rom, const LoadedLevel& level, std::string& error);

} // namespace dk
