#include "core/Compression.h"

extern "C" {
#include "dkcomp.h"
}

#include <cstdlib>

namespace dk {

namespace {
enum DK_FORMAT mapFormat(DkFormat f) {
    switch (f) {
    case DkFormat::BigData: return BD_COMP;
    case DkFormat::SmallData: return SD_COMP;
    case DkFormat::DkcChr: return DKCCHR_COMP;
    }
    return BD_COMP;
}
} // namespace

bool decompress(DkFormat format, const u8* src, std::size_t size,
                std::vector<u8>& out, std::string& error) {
    out.clear();
    error.clear();
    if (!src || size == 0) {
        error = "Empty compressed stream.";
        return false;
    }
    unsigned char* result = nullptr;
    std::size_t resultSize = 0;
    const int rc = dk_decompress_mem_to_mem(mapFormat(format), &result, &resultSize,
                                            const_cast<unsigned char*>(src), size);
    if (rc != 0) {
        error = dk_get_error(rc);
        if (error.empty()) error = "Decompression failed.";
        std::free(result);
        return false;
    }
    if (!result) {
        error = "Decompressor returned no data.";
        return false;
    }
    out.assign(result, result + resultSize);
    std::free(result);
    return true;
}

bool decompress(DkFormat format, const std::vector<u8>& src, std::size_t offset,
                std::vector<u8>& out, std::string& error) {
    if (offset >= src.size()) {
        error = "Compressed data offset is outside the ROM.";
        return false;
    }
    return decompress(format, src.data() + offset, src.size() - offset, out, error);
}

bool compress(DkFormat format, const std::vector<u8>& input, std::vector<u8>& out,
              std::string& error) {
    out.clear();
    error.clear();
    if (input.empty()) {
        error = "Nothing to compress.";
        return false;
    }
    unsigned char* result = nullptr;
    std::size_t resultSize = 0;
    const int rc = dk_compress_mem_to_mem(mapFormat(format), &result, &resultSize,
                                          const_cast<unsigned char*>(input.data()),
                                          input.size());
    if (rc != 0) {
        error = dk_get_error(rc);
        if (error.empty()) error = "Compression failed.";
        std::free(result);
        return false;
    }
    if (!result) {
        error = "Compressor returned no data.";
        return false;
    }
    out.assign(result, result + resultSize);
    std::free(result);
    return true;
}

std::size_t compressedSize(DkFormat format, const std::vector<u8>& src,
                           std::size_t offset) {
    if (offset >= src.size()) return 0;
    std::size_t size = 0;
    const int rc = dk_compressed_size_mem(mapFormat(format),
                                          const_cast<unsigned char*>(src.data() + offset),
                                          src.size() - offset, &size);
    return rc == 0 ? size : 0;
}

namespace {
// MSB-first bit reader used by the DKC3 tilemap format.
struct BitReader {
    const u8* data = nullptr;
    std::size_t bits = 0;
    std::size_t limit = 0;

    int read(int count) {
        int v = 0;
        for (int i = 0; i < count; ++i) {
            if (bits >= limit) return -1;
            const std::size_t byte = bits >> 3;
            v = (v << 1) | ((data[byte] >> (7 - (bits & 7))) & 1);
            ++bits;
        }
        return v;
    }
};

bool readField(BitReader& rd, std::vector<u8>& dst, int valueBits, int runBits) {
    std::size_t pos = 0;
    while (true) {
        const int flag = rd.read(1);
        if (flag < 0) return false;
        const int value = rd.read(valueBits);
        if (value < 0) return false;
        if (flag == 0) {
            if (pos < dst.size()) dst[pos] = static_cast<u8>(value);
            ++pos;
            continue;
        }
        const int count = rd.read(runBits);
        if (count < 0) return false;
        if (count == 0) return true;
        for (int i = 0; i < count; ++i) {
            if (pos < dst.size()) dst[pos] = static_cast<u8>(value);
            ++pos;
        }
    }
}
} // namespace

bool decompressTilemapDkc3(const std::vector<u8>& src, std::size_t offset,
                           std::vector<u16>& out, std::string& error) {
    out.clear();
    error.clear();
    if (offset + 3 > src.size()) {
        error = "DKC3 tilemap header is outside the ROM.";
        return false;
    }
    const u8 spec = src[offset];
    const int words = static_cast<int>(src[offset + 1]) |
                      (static_cast<int>(src[offset + 2]) << 8);
    if ((spec & 0xF8) != 0 || words <= 0) {
        error = "Invalid DKC3 tilemap header.";
        return false;
    }

    BitReader rd;
    rd.data = src.data() + offset + 3;
    rd.limit = (src.size() - offset - 3) * 8;

    std::vector<u8> prio(static_cast<std::size_t>(words), 0);
    std::vector<u8> hflip(static_cast<std::size_t>(words), 0);
    std::vector<u8> vflip(static_cast<std::size_t>(words), 0);
    std::vector<u8> pal(static_cast<std::size_t>(words), 0);
    std::vector<u16> tile(static_cast<std::size_t>(words), 0);

    if ((spec & 1) && !readField(rd, prio, 1, 6)) {
        error = "DKC3 tilemap priority field is truncated.";
        return false;
    }
    if ((spec & 2) && !readField(rd, hflip, 1, 6)) {
        error = "DKC3 tilemap horizontal-flip field is truncated.";
        return false;
    }
    if ((spec & 4) && !readField(rd, vflip, 1, 6)) {
        error = "DKC3 tilemap vertical-flip field is truncated.";
        return false;
    }
    if (!readField(rd, pal, 3, 4)) {
        error = "DKC3 tilemap palette field is truncated.";
        return false;
    }

    std::size_t pos = 0;
    while (true) {
        const int cmd = rd.read(2);
        if (cmd < 0) {
            error = "DKC3 tilemap tile field is truncated.";
            return false;
        }
        if (cmd == 0) {
            const int value = rd.read(10);
            if (value < 0) {
                error = "DKC3 tilemap tile field is truncated.";
                return false;
            }
            if (pos < tile.size()) tile[pos] = static_cast<u16>(value);
            ++pos;
            continue;
        }
        const int value = rd.read(10);
        if (value < 0) {
            error = "DKC3 tilemap tile field is truncated.";
            return false;
        }
        if (cmd == 1) {
            const int count = rd.read(6);
            if (count < 0) {
                error = "DKC3 tilemap tile field is truncated.";
                return false;
            }
            if (count == 0) break;
            for (int i = 0; i < count; ++i) {
                if (pos < tile.size()) tile[pos] = static_cast<u16>(value);
                ++pos;
            }
        } else {
            const int count = rd.read(4);
            if (count < 0) {
                error = "DKC3 tilemap tile field is truncated.";
                return false;
            }
            const int step = cmd == 2 ? 1 : -1;
            for (int i = 0; i < count; ++i) {
                if (pos < tile.size()) {
                    tile[pos] = static_cast<u16>((value + step * i) & 0x3FF);
                }
                ++pos;
            }
        }
    }
    if (pos < tile.size()) {
        error = "DKC3 tilemap ended before all words were written.";
        return false;
    }

    out.resize(tile.size());
    for (std::size_t i = 0; i < tile.size(); ++i) {
        out[i] = static_cast<u16>(tile[i] | (pal[i] << 10) | (prio[i] << 13) |
                                  (hflip[i] << 14) | (vflip[i] << 15));
    }
    return true;
}

} // namespace dk
