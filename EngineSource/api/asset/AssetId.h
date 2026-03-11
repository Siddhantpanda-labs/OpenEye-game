#pragma once

#include <cstdint>
#include <string>

namespace eyelib {
namespace api {

/**
 * @brief Client-facing asset identifier.
 *
 * Mirrors the engine id (two 64-bit parts) but does not include engine headers.
 */
struct AssetId {
    uint64_t high = 0;
    uint64_t low = 0;

    bool IsValid() const { return high != 0 || low != 0; }
    static AssetId Invalid() { return AssetId{}; }

    bool operator==(const AssetId& other) const { return high == other.high && low == other.low; }
    bool operator!=(const AssetId& other) const { return !(*this == other); }

    std::string ToString() const;
    static AssetId FromString(const std::string& s);
};

} // namespace api
} // namespace eyelib

