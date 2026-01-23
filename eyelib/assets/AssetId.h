#pragma once

#include <cstdint>
#include <string>

namespace eyelib {
namespace assets {

/**
 * @brief 128-bit asset identifier (two 64-bit parts).
 *
 * String format matches the legacy GUID:
 * "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"
 */
class AssetId {
public:
    AssetId();
    explicit AssetId(const std::string& guidString);
    AssetId(uint64_t high, uint64_t low);

    static AssetId Generate();
    static AssetId FromString(const std::string& guidString);
    std::string ToString() const;

    bool IsValid() const;
    static AssetId Invalid();

    uint64_t High() const { return m_high; }
    uint64_t Low() const { return m_low; }

    bool operator==(const AssetId& other) const;
    bool operator!=(const AssetId& other) const;
    bool operator<(const AssetId& other) const;

    struct Hash {
        size_t operator()(const AssetId& id) const;
    };

private:
    uint64_t m_high;
    uint64_t m_low;

    static bool ParseHexChar(char c, uint8_t& value);
};

} // namespace assets
} // namespace eyelib

