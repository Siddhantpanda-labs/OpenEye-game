#pragma once

#include "eyelib/math/Types.h"
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

namespace eyelib {
namespace render {

namespace rhi { struct IDevice; struct ITexture; struct IBuffer; }
class Material;
class Shader;
class Texture2D;

/**
 * @brief Material instance: shared material + per-instance overrides.
 * Override properties without modifying the base material.
 */
class MaterialInstance {
public:
    explicit MaterialInstance(std::shared_ptr<Material> material);

    std::shared_ptr<Material> GetMaterial() const { return m_base; }

    void SetTextureOverride(const std::string& name, std::shared_ptr<Texture2D> texture);
    void SetAlbedoOverride(const math::Vec3& color);
    void SetMetallicOverride(float value);
    void SetRoughnessOverride(float value);
    void SetEmissionOverride(const math::Vec3& color);
    void SetAlphaOverride(float value);
    void SetFloatOverride(const std::string& name, float value);
    void SetVec3Override(const std::string& name, const math::Vec3& value);
    void SetVec4Override(const std::string& name, const math::Vec4& value);
    void SetMat4ArrayOverride(const std::string& name, const std::vector<math::Mat4>& values);

    void ClearOverrides();

    std::shared_ptr<Texture2D> GetTexture(const std::string& name) const;
    /** C2: RHI texture for bindless or external binding. Returns nullptr if no texture or invalid. */
    rhi::ITexture* GetTextureRHI(const std::string& name) const;
    /** C2: Bind texture to the given unit; returns true if bound. Use for non-bindless path. */
    bool BindTextureToSlot(const std::string& name, int unit) const;
    math::Vec3 GetAlbedo() const;
    float GetMetallic() const;
    float GetRoughness() const;
    math::Vec3 GetEmission() const;
    float GetAO() const;
    float GetAlpha() const;
    bool GetReflective() const;
    float GetReflectiveStrength() const;
    float GetFloat(const std::string& name, float defaultValue = 0.0f) const;
    math::Vec3 GetVec3(const std::string& name, const math::Vec3& defaultValue = math::Vec3(0.0f)) const;
    math::Vec4 GetVec4(const std::string& name, const math::Vec4& defaultValue = math::Vec4(0.0f)) const;
    void ForEachMat4Array(std::function<void(const std::string&, const math::Mat4*, size_t)> fn) const;

    /** Bind base material shader and textures. Device must be non-null. */
    void Bind(rhi::IDevice* device) const;

    /** Refractor Phase 2: GPU buffer with effective PBR props (base + overrides). Returns null if device is null. */
    rhi::IBuffer* GetMaterialBufferRHI(rhi::IDevice* device) const;

private:
    void EnsureMaterialBuffer(rhi::IDevice* device) const;
    std::shared_ptr<Material> m_base;
    std::unordered_map<std::string, std::shared_ptr<Texture2D>> m_textureOverrides;
    std::optional<math::Vec3> m_albedoOverride;
    std::optional<float> m_metallicOverride;
    std::optional<float> m_roughnessOverride;
    std::optional<math::Vec3> m_emissionOverride;
    std::optional<float> m_alphaOverride;
    std::unordered_map<std::string, float> m_floatOverrides;
    std::unordered_map<std::string, math::Vec3> m_vec3Overrides;
    std::unordered_map<std::string, math::Vec4> m_vec4Overrides;
    std::unordered_map<std::string, std::vector<math::Mat4>> m_mat4ArrayOverrides;
    mutable std::unique_ptr<rhi::IBuffer> m_materialBuffer;
};

} // namespace render
} // namespace eyelib
