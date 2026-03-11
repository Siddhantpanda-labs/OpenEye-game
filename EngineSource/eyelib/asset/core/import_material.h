#pragma once

#include <string>

namespace asset {

/** Reference to a texture (path or name; resolved at write time). */
struct TextureRef {
    std::string path;  // e.g. "car_albedo.texture" or source path
    bool empty() const { return path.empty(); }
};

struct ImportMaterial {
    std::string name;

    TextureRef albedo;
    TextureRef normal;
    TextureRef metallic;
    TextureRef roughness;

    float metallicValue = 0.f;
    float roughnessValue = 0.5f;

    float albedoColorR = 1.f;
    float albedoColorG = 1.f;
    float albedoColorB = 1.f;
};

} // namespace asset
