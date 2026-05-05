#pragma once

#include <string>

namespace asset {

/** Resolved texture reference: dashed asset GUID (matches sibling .png.meta etc.). Empty = no texture. */
struct TextureRef {
    std::string textureAssetId;
    bool empty() const { return textureAssetId.empty(); }
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
