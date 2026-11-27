#pragma once

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace dk {

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u24 = std::uint32_t;
using u32 = std::uint32_t;

inline u16 rd16(const std::vector<u8>& d, std::size_t o) {
    return static_cast<u16>(d[o] | (d[o + 1] << 8));
}

inline u24 rd24(const std::vector<u8>& d, std::size_t o) {
    return static_cast<u24>(d[o] | (d[o + 1] << 8) | (d[o + 2] << 16));
}

inline void wr16(std::vector<u8>& d, std::size_t o, u16 v) {
    d[o + 0] = static_cast<u8>(v & 0xFF);
    d[o + 1] = static_cast<u8>(v >> 8);
}

inline void wr24(std::vector<u8>& d, std::size_t o, u24 v) {
    d[o + 0] = static_cast<u8>(v & 0xFF);
    d[o + 1] = static_cast<u8>((v >> 8) & 0xFF);
    d[o + 2] = static_cast<u8>((v >> 16) & 0xFF);
}

// SNES HiROM address (e.g. $D90000) to a file offset inside a 4 MiB image.
inline std::size_t snToFile(u32 addr) {
    return static_cast<std::size_t>(addr & 0x3FFFFF);
}

inline bool inRange(const std::vector<u8>& d, std::size_t o, std::size_t n) {
    return o <= d.size() && n <= d.size() - o;
}

std::string trim(const std::string& s);
std::string lower(std::string s);
std::string hex(u32 value, int digits = 6);
std::string formatSize(std::size_t bytes);

} // namespace dk
