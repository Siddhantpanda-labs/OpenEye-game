#include "AssetGUID.h"
#include "core/log/Log.h"
#include <random>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <algorithm>

namespace OpenEye {
namespace Assets {

AssetGUID::AssetGUID() : m_high(0), m_low(0) {
}

AssetGUID::AssetGUID(const std::string& guidString) : m_high(0), m_low(0) {
    *this = FromString(guidString);
}

AssetGUID::AssetGUID(uint64_t high, uint64_t low) : m_high(high), m_low(low) {
}

AssetGUID AssetGUID::Generate() {
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<uint64_t> dis;
    
    return AssetGUID(dis(gen), dis(gen));
}

AssetGUID AssetGUID::FromString(const std::string& guidString) {
    AssetGUID guid;
    
    // Expected format: "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"
    // Total length: 36 characters (32 hex digits + 4 hyphens)
    if (guidString.length() != 36) {
        return Invalid();
    }
    
    // Remove hyphens and parse as 32 hex digits
    std::string hexString = guidString;
    hexString.erase(std::remove(hexString.begin(), hexString.end(), '-'), hexString.end());
    
    if (hexString.length() != 32) {
        return Invalid();
    }
    
    // Parse high 64 bits (first 16 hex digits)
    uint64_t high = 0;
    for (size_t i = 0; i < 16; ++i) {
        uint8_t value;
        if (!ParseHexChar(hexString[i], value)) {
            return Invalid();
        }
        high = (high << 4) | value;
    }
    
    // Parse low 64 bits (last 16 hex digits)
    uint64_t low = 0;
    for (size_t i = 16; i < 32; ++i) {
        uint8_t value;
        if (!ParseHexChar(hexString[i], value)) {
            return Invalid();
        }
        low = (low << 4) | value;
    }
    
    guid.m_high = high;
    guid.m_low = low;
    return guid;
}

std::string AssetGUID::ToString() const {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    
    // Format: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
    oss << std::setw(8) << ((m_high >> 32) & 0xFFFFFFFF) << "-"
        << std::setw(4) << ((m_high >> 16) & 0xFFFF) << "-"
        << std::setw(4) << (m_high & 0xFFFF) << "-"
        << std::setw(4) << ((m_low >> 48) & 0xFFFF) << "-"
        << std::setw(12) << (m_low & 0xFFFFFFFFFFFF);
    
    return oss.str();
}

bool AssetGUID::IsValid() const {
    return m_high != 0 || m_low != 0;
}

AssetGUID AssetGUID::Invalid() {
    return AssetGUID(0, 0);
}

bool AssetGUID::operator==(const AssetGUID& other) const {
    return m_high == other.m_high && m_low == other.m_low;
}

bool AssetGUID::operator!=(const AssetGUID& other) const {
    return !(*this == other);
}

bool AssetGUID::operator<(const AssetGUID& other) const {
    if (m_high < other.m_high) return true;
    if (m_high > other.m_high) return false;
    return m_low < other.m_low;
}

size_t AssetGUID::Hash::operator()(const AssetGUID& guid) const {
    return std::hash<uint64_t>()(guid.m_high) ^ (std::hash<uint64_t>()(guid.m_low) << 1);
}

bool AssetGUID::ParseHexChar(char c, uint8_t& value) {
    if (c >= '0' && c <= '9') {
        value = static_cast<uint8_t>(c - '0');
        return true;
    }
    if (c >= 'a' && c <= 'f') {
        value = static_cast<uint8_t>(c - 'a' + 10);
        return true;
    }
    if (c >= 'A' && c <= 'F') {
        value = static_cast<uint8_t>(c - 'A' + 10);
        return true;
    }
    return false;
}

} // namespace Assets
} // namespace OpenEye

