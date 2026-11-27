#pragma once

#include "core/Util.h"

#include <string>
#include <vector>

namespace dk {

enum class DkFormat {
    BigData,   // DKC2/DKC3 "Big Data" (tilesets, tilemaps, map chips)
    SmallData, // DKC3 tilemaps
    DkcChr,    // DKC1 tilesets
};

// Decompress bytes from src[offset..].  Returns false and fills error on failure.
bool decompress(DkFormat format, const std::vector<u8>& src, std::size_t offset,
                std::vector<u8>& out, std::string& error);

bool decompress(DkFormat format, const u8* src, std::size_t size,
                std::vector<u8>& out, std::string& error);

bool compress(DkFormat format, const std::vector<u8>& input, std::vector<u8>& out,
              std::string& error);

// Size of the compressed stream starting at src[0], or 0 if it cannot be parsed.
std::size_t compressedSize(DkFormat format, const std::vector<u8>& src,
                           std::size_t offset);

// DKC3's dedicated tilemap compression.  Decodes to 16-bit tilemap words.
// Returns false if the header or stream is invalid.
bool decompressTilemapDkc3(const std::vector<u8>& src, std::size_t offset,
                           std::vector<u16>& out, std::string& error);

} // namespace dk
