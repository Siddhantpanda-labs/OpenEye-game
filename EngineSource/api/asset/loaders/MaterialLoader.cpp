#include "MaterialLoader.h"
#include "api/asset/AssetRegistry.h"
#include "asset/readers/material_reader.h"
#include "eyelib/render/materials/MaterialDescriptor.h"

#include <algorithm>
#include <vector>

namespace eyelib {
namespace api {

namespace {

static std::string TexturePathOrEmpty(const eyelib::render::MaterialDescriptor& src, const char* slot) {
    auto it = src.texturePaths.find(slot);
    return it != src.texturePaths.end() ? it->second : std::string();
}

template <typename Map>
static std::vector<std::string> SortedKeys(const Map& m) {
    std::vector<std::string> keys;
    keys.reserve(m.size());
    for (const auto& kv : m) keys.push_back(kv.first);
    std::sort(keys.begin(), keys.end());
    return keys;
}

static asset::LoadedMaterial DescriptorToLoadedMaterial(const eyelib::render::MaterialDescriptor& src) {
    asset::LoadedMaterial dst;
    dst.albedoPath = TexturePathOrEmpty(src, "u_AlbedoTexture");
    dst.normalPath = TexturePathOrEmpty(src, "u_NormalTexture");
    dst.metallicPath = TexturePathOrEmpty(src, "u_MetallicTexture");
    dst.roughnessPath = TexturePathOrEmpty(src, "u_RoughnessTexture");
    dst.aoPath = TexturePathOrEmpty(src, "u_AOTexture");
    dst.emissivePath = TexturePathOrEmpty(src, "u_EmissiveTexture");
    dst.metallicRoughnessPath = TexturePathOrEmpty(src, "u_MetallicRoughnessTexture");
    dst.metallicValue = src.metallic;
    dst.roughnessValue = src.roughness;
    dst.albedoColorR = src.albedo.x;
    dst.albedoColorG = src.albedo.y;
    dst.albedoColorB = src.albedo.z;
    dst.emissionR = src.emission.x;
    dst.emissionG = src.emission.y;
    dst.emissionB = src.emission.z;
    dst.aoValue = src.ao;
    dst.alpha = src.alpha;
    dst.shadingModel = static_cast<uint8_t>(src.shadingModel);
    dst.blending = static_cast<uint8_t>(src.blending);
    dst.flags = static_cast<uint8_t>((src.twoSided ? 1u : 0u) | (src.refraction ? 2u : 0u));
    dst.reflectiveStrength = src.reflectiveStrength;
    dst.materialShaderId = static_cast<uint64_t>(src.materialShaderId);
    dst.fragmentSnippet = src.fragmentSnippet;
    dst.shadingSnippet = src.shadingSnippet;
    dst.shaderAssetPath = src.shaderAssetPath;
    for (const std::string& k : SortedKeys(src.keywords)) {
        auto it = src.keywords.find(k);
        if (it != src.keywords.end()) dst.keywordPairs.push_back({k, it->second});
    }
    for (const std::string& k : SortedKeys(src.floats)) {
        auto it = src.floats.find(k);
        if (it != src.floats.end()) dst.floatPairs.push_back({k, it->second});
    }
    for (const std::string& k : SortedKeys(src.vec3s)) {
        auto it = src.vec3s.find(k);
        if (it != src.vec3s.end()) {
            const auto& v = it->second;
            dst.vec3Pairs.push_back({k, {v.x, v.y, v.z}});
        }
    }
    for (const std::string& k : SortedKeys(src.vec4s)) {
        auto it = src.vec4s.find(k);
        if (it != src.vec4s.end()) {
            const auto& v = it->second;
            dst.vec4Pairs.push_back({k, {v.x, v.y, v.z, v.w}});
        }
    }
    return dst;
}

}  // namespace

AssetId MaterialLoader::LoadMaterial(AssetRegistry* registry, const std::string& path) {
    if (!registry) return AssetId::Invalid();
    registry->LoadAsset(path, AssetType::Material);
    return registry->GetId(path);
}

AssetId MaterialLoader::LoadMaterial(const std::string& path) {
    (void)path;
    return AssetId::Invalid();
}

bool MaterialLoader::SaveMaterial(const std::string& absolutePath, const eyelib::render::MaterialDescriptor& desc) {
    if (absolutePath.empty()) return false;
    return asset::MaterialReader::Write(absolutePath, DescriptorToLoadedMaterial(desc));
}

} // namespace api
} // namespace eyelib
