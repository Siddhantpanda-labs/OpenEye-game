#include "asset_pipe/core/guid.h"

#include <algorithm>
#include <iomanip>
#include <random>
#include <sstream>

namespace asset_pipe {

namespace {

bool ParseHexChar(char c, uint8_t& out) {
    if (c >= '0' && c <= '9') { out = static_cast<uint8_t>(c - '0'); return true; }
    if (c >= 'a' && c <= 'f') { out = static_cast<uint8_t>(c - 'a' + 10); return true; }
    if (c >= 'A' && c <= 'F') { out = static_cast<uint8_t>(c - 'A' + 10); return true; }
    return false;
}

} // namespace

Guid Guid::Generate() {
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<uint64_t> dis;
    Guid g;
    g.high = dis(gen);
    g.low = dis(gen);
    return g;
}

Guid Guid::FromString(const std::string& s) {
    std::string hex = s;
    hex.erase(std::remove(hex.begin(), hex.end(), '-'), hex.end());
    if (hex.size() != 32) return Invalid();

    uint64_t high = 0, low = 0;
    for (size_t i = 0; i < 16; ++i) {
        uint8_t v;
        if (!ParseHexChar(hex[i], v)) return Invalid();
        high = (high << 4) | v;
    }
    for (size_t i = 16; i < 32; ++i) {
        uint8_t v;
        if (!ParseHexChar(hex[i], v)) return Invalid();
        low = (low << 4) | v;
    }
    return Guid{ high, low };
}

std::string Guid::ToString() const {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0')
        << std::setw(8) << ((high >> 32) & 0xFFFFFFFFu) << "-"
        << std::setw(4) << ((high >> 16) & 0xFFFFu) << "-"
        << std::setw(4) << (high & 0xFFFFu) << "-"
        << std::setw(4) << ((low >> 48) & 0xFFFFu) << "-"
        << std::setw(12) << (low & 0xFFFFFFFFFFFFull);
    return oss.str();
}

bool Guid::IsValid() const {
    return high != 0 || low != 0;
}

Guid Guid::Invalid() {
    return Guid{ 0, 0 };
}

size_t Guid::Hash::operator()(const Guid& g) const {
    return std::hash<uint64_t>{}(g.high) ^ (std::hash<uint64_t>{}(g.low) << 1);
}

} // namespace asset_pipe
