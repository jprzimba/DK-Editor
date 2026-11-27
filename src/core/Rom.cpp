#include "core/Rom.h"

#include <cstring>
#include <fstream>

namespace dk {

namespace {
bool readFile(const std::string& filename, std::vector<u8>& out) {
    std::ifstream f(filename, std::ios::binary);
    if (!f) return false;
    f.seekg(0, std::ios::end);
    std::streamoff size = f.tellg();
    if (size < 0) return false;
    f.seekg(0, std::ios::beg);
    out.resize(static_cast<std::size_t>(size));
    if (size > 0) f.read(reinterpret_cast<char*>(out.data()), size);
    return f.good() || f.eof();
}

bool writeFile(const std::string& filename, const std::vector<u8>& data) {
    std::ofstream f(filename, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    if (!data.empty()) f.write(reinterpret_cast<const char*>(data.data()), data.size());
    return f.good();
}
} // namespace

bool Rom::load(const std::string& filename, std::string& error) {
    error.clear();
    data.clear();
    path = filename;
    headered = false;
    game = GameKind::Unknown;
    version = 0;
    title.clear();

    std::vector<u8> raw;
    if (!readFile(filename, raw)) {
        error = "Could not read " + filename;
        return false;
    }
    if (raw.size() % 0x8000 == 512) {
        headered = true;
        raw.erase(raw.begin(), raw.begin() + 512);
    }
    if (raw.size() < 0x10000) {
        error = "File is too small to be a SNES ROM.";
        return false;
    }
    if (raw.size() != 0x400000) {
        // Accept odd dumps but warn through the error string on the UI side.
        error = "Warning: ROM is not exactly 4 MiB (" + formatSize(raw.size()) + ").";
    }

    data = std::move(raw);

    char name[22] = {};
    std::memcpy(name, data.data() + 0xFFC0, 21);
    title.assign(name, 21);
    for (char& c : title) {
        if (c == '\0') c = ' ';
    }
    version = data[0xFFDB];

    const std::string t = title;
    if (t.find("DIDDY") != std::string::npos || t.find("KONG QUEST") != std::string::npos) {
        game = GameKind::Dkc2;
    } else if (t.find("COUNTRY 3") != std::string::npos ||
               t.find("DIXIE") != std::string::npos ||
               (t.find("DONKEY KONG COUNTRY") != std::string::npos && t.find('3') != std::string::npos)) {
        game = GameKind::Dkc3;
    } else if (t.find("DONKEY KONG COUNTRY") != std::string::npos) {
        game = GameKind::Dkc1;
    } else {
        error = "Unrecognised ROM title: \"" + trim(title) + "\"";
        return false;
    }

    return true;
}

bool Rom::save(const std::string& filename, std::string& error) {
    error.clear();
    fixChecksum();
    if (!writeFile(filename, data)) {
        error = "Could not write " + filename;
        return false;
    }
    path = filename;
    return true;
}

void Rom::fixChecksum() {
    if (data.size() < 0x10000) return;

    // The checksum covers the whole image with the four checksum bytes acting as
    // FF FF 00 00.  This works for both LoROM and HiROM images.
    const std::size_t header = 0xFFC0;
    const std::size_t field = header + 0x1C; // 0xFFDC
    data[field + 0] = 0xFF;
    data[field + 1] = 0xFF;
    data[field + 2] = 0x00;
    data[field + 3] = 0x00;

    u32 sum = 0;
    for (u8 b : data) sum += b;
    const u16 checksum = static_cast<u16>(sum & 0xFFFF);
    const u16 complement = static_cast<u16>(checksum ^ 0xFFFF);
    data[field + 0] = static_cast<u8>(complement & 0xFF);
    data[field + 1] = static_cast<u8>(complement >> 8);
    data[field + 2] = static_cast<u8>(checksum & 0xFF);
    data[field + 3] = static_cast<u8>(checksum >> 8);
}

u32 Rom::checksum() const {
    if (data.size() < 0x10000) return 0;
    return data[0xFFDE] | (data[0xFFDF] << 8);
}

u32 Rom::checksumComplement() const {
    if (data.size() < 0x10000) return 0;
    return data[0xFFDC] | (data[0xFFDD] << 8);
}

std::string Rom::gameName() const {
    switch (game) {
    case GameKind::Dkc1: return "Donkey Kong Country";
    case GameKind::Dkc2: return "Donkey Kong Country 2";
    case GameKind::Dkc3: return "Donkey Kong Country 3";
    default: return "Unknown";
    }
}

std::string Rom::versionName() const {
    switch (game) {
    case GameKind::Dkc1: return "US v1." + std::to_string(version);
    case GameKind::Dkc2: return "US v1." + std::to_string(version);
    case GameKind::Dkc3: return "US v1." + std::to_string(version);
    default: return "";
    }
}

bool Rom::copyOut(std::size_t o, std::size_t n, std::vector<u8>& out) const {
    if (!inRange(data, o, n)) return false;
    out.assign(data.begin() + static_cast<std::ptrdiff_t>(o),
               data.begin() + static_cast<std::ptrdiff_t>(o + n));
    return true;
}

bool Rom::copyIn(std::size_t o, const std::vector<u8>& src) {
    return copyIn(o, src.data(), src.size());
}

bool Rom::copyIn(std::size_t o, const u8* src, std::size_t n) {
    if (!inRange(data, o, n)) return false;
    std::memcpy(data.data() + o, src, n);
    return true;
}

} // namespace dk
