#pragma once

#include "eyelib/math/Types.h"
#include "api/asset/AssetId.h"
#include "eyelib/rendering/shaders/ShaderKey.h"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace eyelib {
namespace rendering {

namespace rhi { struct IDevice; struct ITexture; struct IBuffer; struct ICommandList; struct IDescriptorSet; }
class Shader;
class Texture2D;
class MaterialInstance;

/** Refractor Phase 4: surface shading model for pass template branching. */
enum class ShadingModel : uint8_t {
    Lit,    /** PBR (metallic-roughness). */
    Unlit,  /** Albedo + emission only; no lighting. */
    Custom  /** Per-material snippet drives output. */
};

/** Refractor 7.2 (Limitless-aligned): how surface color is combined with background. */
enum class Blending : uint8_t {
    Opaque,     /** No blending; depth write on. */
    Translucent,/** color.rgb * a + bg * (1-a); depth write off. */
    Additive,   /** color + bg. */
    Modulate,   /** color * bg. */
};

/**
 * @brief Material: shader, texture slots, PBR properties, custom params, keywords.
 * Bind() binds shader, textures, and sets uniforms.
 */
class Material {
public:
    Material();
    ~Material() = default;

    /** MaterialShader id (e.g. PBR=0). Used by ShaderCache to compile pipeline variants. */
    MaterialTypeId GetMaterialShaderId() const { return m_shaderIndex; }
    void SetMaterialShaderId(MaterialTypeId id) { m_shaderIndex = id; m_hashDirty = true; }

    void SetTexture(const std::string& name, std::shared_ptr<Texture2D> texture);
    void RemoveTexture(const std::string& name);
    std::shared_ptr<Texture2D> GetTexture(const std::string& name) const;
    /** C2: RHI texture for bindless or external binding. Returns nullptr if no texture or invalid. */
    rhi::ITexture* GetTextureRHI(const std::string& name) const;
    /** C2: Bind texture to the given unit; returns true if bound. Use for non-bindless path. */
    bool BindTextureToSlot(const std::string& name, int unit) const;

    void SetAlbedo(const math::Vec3& color);
    void SetMetallic(float value);
    void SetRoughness(float value);
    void SetEmission(const math::Vec3& color);
    void SetAO(float value);
    void SetAlpha(float value);

    math::Vec3 GetAlbedo() const { return m_albedo; }
    float GetMetallic() const { return m_metallic; }
    float GetRoughness() const { return m_roughness; }
    math::Vec3 GetEmission() const { return m_emission; }
    float GetAO() const { return m_ao; }
    float GetAlpha() const { return m_alpha; }

    void SetAlbedoTexture(std::shared_ptr<Texture2D> t) { SetTexture("u_AlbedoTexture", t); }
    void SetNormalTexture(std::shared_ptr<Texture2D> t) { SetTexture("u_NormalTexture", t); }
    void SetMetallicTexture(std::shared_ptr<Texture2D> t) { SetTexture("u_MetallicTexture", t); }
    void SetRoughnessTexture(std::shared_ptr<Texture2D> t) { SetTexture("u_RoughnessTexture", t); }
    void SetAOTexture(std::shared_ptr<Texture2D> t) { SetTexture("u_AOTexture", t); }
    void SetEmissiveTexture(std::shared_ptr<Texture2D> t) { SetTexture("u_EmissiveTexture", t); }
    void SetMetallicRoughnessTexture(std::shared_ptr<Texture2D> t) { SetTexture("u_MetallicRoughnessTexture", t); }

    void SetFloat(const std::string& name, float value);
    void SetVec3(const std::string& name, const math::Vec3& value);
    void SetVec4(const std::string& name, const math::Vec4& value);
    void SetMat4Array(const std::string& name, const std::vector<math::Mat4>& values);

    float GetFloat(const std::string& name, float defaultValue = 0.0f) const;
    math::Vec3 GetVec3(const std::string& name, const math::Vec3& defaultValue = math::Vec3(0.0f)) const;
    math::Vec4 GetVec4(const std::string& name, const math::Vec4& defaultValue = math::Vec4(0.0f)) const;

    void SetKeyword(const std::string& keyword, bool enabled);
    bool IsKeywordEnabled(const std::string& keyword) const;

    /** Apply material parameters and textures to an already-bound shader. Call after shader->Bind(device). If cmd is non-null, binds MaterialBlock UBO at binding 2. */
    void ApplyToShader(rhi::IDevice* device, Shader* shader, rhi::ICommandList* cmd = nullptr) const;
    /** Update resource-space Material descriptor set (set 2): binding 0 = UBO, 1 = albedo, 2 = normal, 3 = metallic/roughness. No-op if device or set is null. */
    void UpdateForwardResourceSet(rhi::IDevice* device, rhi::IDescriptorSet* set) const;
    size_t GetHash() const;

    /** Refractor Phase 2: GPU buffer (std140) for PBR props. Ensures buffer exists and is up to date; returns null if device is null. */
    rhi::IBuffer* GetMaterialBufferRHI(rhi::IDevice* device) const;

    /** Alias for GetMaterialShaderId (cache key). */
    MaterialTypeId GetShaderIndex() const { return m_shaderIndex; }
    void SetShaderIndex(MaterialTypeId id) { SetMaterialShaderId(id); }
    /** Optional GLSL snippet injected into pass fragment template (e.g. custom surface). Empty = use pipeline default. */
    const std::string& GetFragmentSnippet() const { return m_fragmentSnippet; }
    void SetFragmentSnippet(std::string s) { m_fragmentSnippet = std::move(s); }
    const std::string& GetShadingSnippet() const { return m_shadingSnippet; }
    void SetShadingSnippet(std::string s) { m_shadingSnippet = std::move(s); }

    /** Refractor Phase 4: shading model (Lit / Unlit / Custom). Default Lit. */
    ShadingModel GetShadingModel() const { return m_shadingModel; }
    void SetShadingModel(ShadingModel m) { m_shadingModel = m; }

    /** Refractor 7.2: blending mode (Opaque / Translucent / Additive / Modulate). Default Opaque. */
    Blending GetBlending() const { return m_blending; }
    void SetBlending(Blending b) { m_blending = b; }
    /** Refractor 7.2: if true, disable back-face culling. */
    bool GetTwoSided() const { return m_twoSided; }
    void SetTwoSided(bool v) { m_twoSided = v; }
    /** Refractor 7.2: compile-time flag for refraction in shader (HAS_REFRACTION). */
    bool GetRefraction() const { return m_refraction; }
    void SetRefraction(bool v) { m_refraction = v; }
    /** SSR: when true, surface receives screen-space reflections (blended by fresnel). */
    bool GetReflective() const { return m_reflectiveStrength > 0.0f; }
    void SetReflective(bool v) { m_reflectiveStrength = v ? 1.0f : 0.0f; }
    /** SSR: reflection amount 0 (none) to 1 (full). Used in blend: fresnel * strength. */
    float GetReflectiveStrength() const { return m_reflectiveStrength; }
    void SetReflectiveStrength(float v) { m_reflectiveStrength = (v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v); }

    void SetAssetId(const ::eyelib::api::AssetId& id) { m_assetId = id; }
    const ::eyelib::api::AssetId& GetAssetId() const { return m_assetId; }
    bool HasAssetId() const { return m_assetId.IsValid(); }

    void ForEachTexture(std::function<void(const std::string&, const std::shared_ptr<Texture2D>&)> fn) const;
    void ForEachFloat(std::function<void(const std::string&, float)> fn) const;
    void ForEachVec3(std::function<void(const std::string&, const math::Vec3&)> fn) const;
    void ForEachVec4(std::function<void(const std::string&, const math::Vec4&)> fn) const;
    void ForEachMat4Array(std::function<void(const std::string&, const math::Mat4*, size_t)> fn) const;

private:
    void UpdateHash() const;

    std::unordered_map<std::string, std::shared_ptr<Texture2D>> m_textures;
    std::unordered_map<std::string, float> m_floats;
    std::unordered_map<std::string, math::Vec3> m_vec3s;
    std::unordered_map<std::string, math::Vec4> m_vec4s;
    std::unordered_map<std::string, std::vector<math::Mat4>> m_mat4Arrays;
    std::unordered_map<std::string, bool> m_keywords;

    math::Vec3 m_albedo{0.8f};
    float m_metallic = 0.0f;
    float m_roughness = 0.5f;
    math::Vec3 m_emission{0.0f};
    float m_ao = 1.0f;
    float m_alpha = 1.0f;

    mutable size_t m_cachedHash = 0;
    mutable bool m_hashDirty = true;

    ::eyelib::api::AssetId m_assetId = ::eyelib::api::AssetId::Invalid();

    /** Refractor Phase 2: UBO for MaterialBlock (binding 2). Created on first GetMaterialBufferRHI; updated every call. */
    mutable std::unique_ptr<rhi::IBuffer> m_materialBuffer;
    void EnsureMaterialBuffer(rhi::IDevice* device) const;

    MaterialTypeId m_shaderIndex = kDefaultMaterialTypeId;
    std::string m_fragmentSnippet;
    std::string m_shadingSnippet;
    ShadingModel m_shadingModel = ShadingModel::Lit;
    Blending m_blending = Blending::Opaque;
    bool m_twoSided = false;
    bool m_refraction = false;
    float m_reflectiveStrength = 0.0f;
};

/** std140 layout for MaterialBlock UBO (binding 2). Must match GLSL. */
constexpr size_t kMaterialBlockStd140Size = 48u;

/** Vulkan forward fragment push: 1 when `u_AlbedoTexture` has an RHI binding. */
int ComputeForwardAlbedoTextureFlag(const Material* mat, const MaterialInstance* mi = nullptr);

} // namespace rendering
} // namespace eyelib
