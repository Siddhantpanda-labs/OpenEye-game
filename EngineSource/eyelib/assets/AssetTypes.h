#pragma once

#include <string>

namespace eyelib {
namespace assets {

enum class AssetType {
    Unknown = 0,
    Mesh,
    Texture,
    Material,
    Shader,
    Audio,
    Animation,
    Prefab,
    Scene
};

inline std::string ToString(AssetType type) {
    switch (type) {
        case AssetType::Mesh: return "Mesh";
        case AssetType::Texture: return "Texture";
        case AssetType::Material: return "Material";
        case AssetType::Shader: return "Shader";
        case AssetType::Audio: return "Audio";
        case AssetType::Animation: return "Animation";
        case AssetType::Prefab: return "Prefab";
        case AssetType::Scene: return "Scene";
        default: return "Unknown";
    }
}

inline AssetType AssetTypeFromString(const std::string& value) {
    if (value == "Mesh") return AssetType::Mesh;
    if (value == "Texture") return AssetType::Texture;
    if (value == "Material") return AssetType::Material;
    if (value == "Shader") return AssetType::Shader;
    if (value == "Audio") return AssetType::Audio;
    if (value == "Animation") return AssetType::Animation;
    if (value == "Prefab") return AssetType::Prefab;
    if (value == "Scene") return AssetType::Scene;
    return AssetType::Unknown;
}

} // namespace assets
} // namespace eyelib
