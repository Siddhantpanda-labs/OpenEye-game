#pragma once

#include "eyelib/math/Types.h"
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

namespace eyelib {
namespace render {

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

    void ClearOverrides();

    void Bind() const;

private:
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
};

} // namespace render
} // namespace eyelib
