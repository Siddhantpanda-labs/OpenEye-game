#pragma once

#include "eyelib/math/Types.h"
#include "eyelib/assets/AssetId.h"
#include <cstddef>
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
 * @brief Material: shader, texture slots, PBR properties, custom params, keywords.
 * Bind() binds shader, textures, and sets uniforms.
 */
class Material {
public:
    Material();
    explicit Material(std::shared_ptr<Shader> shader);
    ~Material() = default;

    void SetShader(std::shared_ptr<Shader> shader);
    std::shared_ptr<Shader> GetShader() const { return m_shader; }

    void SetTexture(const std::string& name, std::shared_ptr<Texture2D> texture);
    std::shared_ptr<Texture2D> GetTexture(const std::string& name) const;

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

    void Bind() const;
    size_t GetHash() const;

    void SetAssetId(const assets::AssetId& id) { m_assetId = id; }
    const assets::AssetId& GetAssetId() const { return m_assetId; }
    bool HasAssetId() const { return m_assetId.IsValid(); }

    void ForEachTexture(std::function<void(const std::string&, const std::shared_ptr<Texture2D>&)> fn) const;
    void ForEachFloat(std::function<void(const std::string&, float)> fn) const;
    void ForEachVec3(std::function<void(const std::string&, const math::Vec3&)> fn) const;
    void ForEachVec4(std::function<void(const std::string&, const math::Vec4&)> fn) const;

private:
    void UpdateHash() const;

    std::shared_ptr<Shader> m_shader;
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

    assets::AssetId m_assetId = assets::AssetId::Invalid();
};

} // namespace render
} // namespace eyelib
