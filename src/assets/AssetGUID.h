#pragma once

#include <string>
#include <cstdint>
#include <functional>
#include <unordered_map>

namespace OpenEye {
namespace Assets {

/**
 * @brief Asset GUID (Globally Unique Identifier) for asset identification.
 * 
 * GUIDs are 128-bit identifiers that uniquely identify assets in the engine.
 * They are serialized as strings in the format: "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"
 */
class AssetGUID {
public:
    AssetGUID();
    AssetGUID(const std::string& guidString);
    AssetGUID(uint64_t high, uint64_t low);
    
    /**
     * @brief Generate a new random GUID.
     */
    static AssetGUID Generate();
    
    /**
     * @brief Create a GUID from a string representation.
     * @param guidString String in format "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"
     * @return GUID object, or invalid GUID if string is malformed
     */
    static AssetGUID FromString(const std::string& guidString);
    
    /**
     * @brief Convert GUID to string representation.
     * @return String in format "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"
     */
    std::string ToString() const;
    
    /**
     * @brief Check if GUID is valid (not all zeros).
     */
    bool IsValid() const;
    
    /**
     * @brief Get invalid/null GUID.
     */
    static AssetGUID Invalid();
    
    // Comparison operators
    bool operator==(const AssetGUID& other) const;
    bool operator!=(const AssetGUID& other) const;
    bool operator<(const AssetGUID& other) const;
    
    // Hash support
    struct Hash {
        size_t operator()(const AssetGUID& guid) const;
    };
    
    // Get raw values
    uint64_t GetHigh() const { return m_high; }
    uint64_t GetLow() const { return m_low; }

private:
    uint64_t m_high; // High 64 bits
    uint64_t m_low;  // Low 64 bits
    
    static bool ParseHexChar(char c, uint8_t& value);
};

} // namespace Assets
} // namespace OpenEye

