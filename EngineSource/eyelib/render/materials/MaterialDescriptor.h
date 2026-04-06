#pragma once

#include "eyelib/math/Types.h"
#include "eyelib/render/materials/Material.h"
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace eyelib {
namespace render {

class Shader;
class Texture2D;

/**
 * CPU-only material definition (Phase 3: asset vs GPU instance).
 * Shader name, texture paths, PBR and custom params, keywords; no GPU pointers.
 * Safe to store in registry and serialize.
 * Use with Material::CreateFromDescriptor(device, desc, resolveShader, resolveTexture) to build a GPU-bound Material.
 * Texture map values are asset GUIDs; resolveTexture receives that GUID string.
 */
struct MaterialDescriptor {
    /** Non-empty selects built-in PBR material path (e.g. default forward asset). */
    std::string shaderAssetPath;
    /** Uniform name (e.g. u_AlbedoTexture) -> texture asset GUID string (dashed), matching AssetDatabase / .meta. */
    std::unordered_map<std::string, std::string> texturePaths;
    std::unordered_map<std::string, float> floats;
    std::unordered_map<std::string, math::Vec3> vec3s;
    std::unordered_map<std::string, math::Vec4> vec4s;
    std::unordered_map<std::string, std::vector<math::Mat4>> mat4Arrays;
    std::unordered_map<std::string, bool> keywords;

    math::Vec3 albedo{0.8f};
    float metallic = 0.0f;
    float roughness = 0.5f;
    math::Vec3 emission{0.0f};
    float ao = 1.0f;
    float alpha = 1.0f;

    ShadingModel shadingModel = ShadingModel::Lit;
    Blending blending = Blending::Opaque;
    bool twoSided = false;
    bool refraction = false;
    float reflectiveStrength = 0.f;
    MaterialTypeId materialShaderId = kDefaultMaterialTypeId;
    std::string fragmentSnippet;
    std::string shadingSnippet;
};

/**
 * Create a Material from a CPU descriptor. Resolvers provide shader/texture instances for the given device.
 * resolveShader(name) -> shared_ptr<Shader> or nullptr; resolveTexture(textureAssetGuid) -> shared_ptr<Texture2D> or nullptr.
 */
std::shared_ptr<Material> CreateMaterialFromDescriptor(
    const MaterialDescriptor& desc,
    std::function<std::shared_ptr<Shader>(const std::string& name)> resolveShader,
    std::function<std::shared_ptr<Texture2D>(const std::string& textureAssetGuid)> resolveTexture);

/** Updates an existing material from \a desc (PBR, blend, snippets, textures). Empty texture path clears the slot. */
void ApplyMaterialFromDescriptor(
    Material& mat,
    const MaterialDescriptor& desc,
    std::function<std::shared_ptr<Shader>(const std::string& name)> resolveShader,
    std::function<std::shared_ptr<Texture2D>(const std::string& textureAssetGuid)> resolveTexture);

} // namespace render
} // namespace eyelib
