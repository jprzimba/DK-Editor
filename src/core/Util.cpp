#include "core/Util.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace dk {

std::string trim(const std::string& s) {
    std::size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

std::string hex(u32 value, int digits) {
    static const char* table = "0123456789ABCDEF";
    std::string out;
    out.resize(digits);
    for (int i = digits - 1; i >= 0; --i) {
        out[i] = table[value & 0xF];
        value >>= 4;
    }
    return out;
}

std::string formatSize(std::size_t bytes) {
    std::ostringstream os;
    if (bytes >= 1024 * 1024) {
        os << (bytes / (1024.0 * 1024.0)) << " MiB";
    } else if (bytes >= 1024) {
        os << (bytes / 1024.0) << " KiB";
    } else {
        os << bytes << " B";
    }
    return os.str();
}

} // namespace dk
