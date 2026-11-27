#pragma once

#include "core/Util.h"

#include <string>
#include <vector>

namespace dk {

enum class GameKind {
    Unknown,
    Dkc1,
    Dkc2,
    Dkc3,
};

struct Rom {
    std::vector<u8> data;
    std::string path;
    bool headered = false;
    GameKind game = GameKind::Unknown;
    int version = 0;
    std::string title;

    bool load(const std::string& filename, std::string& error);
    bool save(const std::string& filename, std::string& error);
    void fixChecksum();

    std::string gameName() const;
    std::string versionName() const;

    u32 checksum() const;
    u32 checksumComplement() const;

    bool valid() const { return data.size() >= 0x10000; }

    // Convenience accessors. All offsets are file offsets into the headerless ROM.
    u8 byte(std::size_t o) const { return o < data.size() ? data[o] : 0; }
    u16 word(std::size_t o) const { return inRange(data, o, 2) ? rd16(data, o) : 0; }
    u24 longptr(std::size_t o) const { return inRange(data, o, 3) ? rd24(data, o) : 0; }
    bool copyOut(std::size_t o, std::size_t n, std::vector<u8>& out) const;
    bool copyIn(std::size_t o, const std::vector<u8>& src);
    bool copyIn(std::size_t o, const u8* src, std::size_t n);
};

} // namespace dk
