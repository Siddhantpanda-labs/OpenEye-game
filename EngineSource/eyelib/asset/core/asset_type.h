#pragma once

#include <string>

namespace asset {

enum class AssetType {
    Unknown = 0,
    Mesh,
    Material,
    Texture,
    Animation,
    Package,
};

inline std::string ToString(AssetType type) {
    switch (type) {
        case AssetType::Mesh: return "Mesh";
        case AssetType::Material: return "Material";
        case AssetType::Texture: return "Texture";
        case AssetType::Animation: return "Animation";
        case AssetType::Package: return "Package";
        default: return "Unknown";
    }
}

inline AssetType AssetTypeFromString(const std::string& value) {
    if (value == "Mesh") return AssetType::Mesh;
    if (value == "Material") return AssetType::Material;
    if (value == "Texture") return AssetType::Texture;
    if (value == "Animation") return AssetType::Animation;
    if (value == "Package") return AssetType::Package;
    return AssetType::Unknown;
}

} // namespace asset
