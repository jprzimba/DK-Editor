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

} // namespace dk
