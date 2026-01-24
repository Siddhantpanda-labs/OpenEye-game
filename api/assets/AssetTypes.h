#pragma once

#include <string>

namespace eyelib {
namespace api {

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

} // namespace api
} // namespace eyelib
