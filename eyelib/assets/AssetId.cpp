#include "eyelib/assets/AssetId.h"

#include <algorithm>
#include <iomanip>
#include <random>
#include <sstream>

namespace eyelib {
namespace assets {

AssetId::AssetId() : m_high(0), m_low(0) {}

AssetId::AssetId(const std::string& guidString) : m_high(0), m_low(0) {
    *this = FromString(guidString);
}

AssetId::AssetId(uint64_t high, uint64_t low) : m_high(high), m_low(low) {}

AssetId AssetId::Generate() {
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<uint64_t> dis;
    return AssetId(dis(gen), dis(gen));
}

AssetId AssetId::FromString(const std::string& guidString) {
    // Expected length: 36 (32 hex + 4 hyphens)
    if (guidString.size() != 36) {
        return Invalid();
    }

    std::string hex = guidString;
    hex.erase(std::remove(hex.begin(), hex.end(), '-'), hex.end());
    if (hex.size() != 32) {
        return Invalid();
    }

    uint64_t high = 0;
    for (size_t i = 0; i < 16; ++i) {
        uint8_t v;
        if (!ParseHexChar(hex[i], v)) return Invalid();
        high = (high << 4) | v;
    }

    uint64_t low = 0;
    for (size_t i = 16; i < 32; ++i) {
        uint8_t v;
        if (!ParseHexChar(hex[i], v)) return Invalid();
        low = (low << 4) | v;
    }

    return AssetId(high, low);
}

std::string AssetId::ToString() const {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');

    // xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
    oss << std::setw(8) << ((m_high >> 32) & 0xFFFFFFFFu) << "-"
        << std::setw(4) << ((m_high >> 16) & 0xFFFFu) << "-"
        << std::setw(4) << (m_high & 0xFFFFu) << "-"
        << std::setw(4) << ((m_low >> 48) & 0xFFFFu) << "-"
        << std::setw(12) << (m_low & 0xFFFFFFFFFFFFull);

    return oss.str();
}

bool AssetId::IsValid() const {
    return m_high != 0 || m_low != 0;
}

AssetId AssetId::Invalid() {
    return AssetId(0, 0);
}

bool AssetId::operator==(const AssetId& other) const {
    return m_high == other.m_high && m_low == other.m_low;
}

bool AssetId::operator!=(const AssetId& other) const {
    return !(*this == other);
}

bool AssetId::operator<(const AssetId& other) const {
    if (m_high < other.m_high) return true;
    if (m_high > other.m_high) return false;
    return m_low < other.m_low;
}

size_t AssetId::Hash::operator()(const AssetId& id) const {
    return std::hash<uint64_t>()(id.m_high) ^ (std::hash<uint64_t>()(id.m_low) << 1);
}

bool AssetId::ParseHexChar(char c, uint8_t& value) {
    if (c >= '0' && c <= '9') { value = static_cast<uint8_t>(c - '0'); return true; }
    if (c >= 'a' && c <= 'f') { value = static_cast<uint8_t>(c - 'a' + 10); return true; }
    if (c >= 'A' && c <= 'F') { value = static_cast<uint8_t>(c - 'A' + 10); return true; }
    return false;
}

} // namespace assets
} // namespace eyelib

