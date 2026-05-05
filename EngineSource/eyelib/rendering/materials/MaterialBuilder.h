#pragma once

#include "eyelib/math/Types.h"
#include "eyelib/rendering/shaders/ShaderKey.h"
#include "eyelib/rendering/materials/Material.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace eyelib {
namespace rendering {

class Material;
class Shader;
class Texture2D;

/**
 * Fluent builder for Material (Limitless-inspired, Refractor 7.5).
 * Set properties and textures then build() to get a shared_ptr<Material>.
 * To build from canonical .mat data, load via MaterialReader then set Albedo/Metallic/Roughness from LoadedMaterial and call build() (e.g. in api/asset or app).
 */
class MaterialBuilder {
public:
    MaterialBuilder() = default;

    MaterialBuilder& Albedo(const math::Vec3& color);
    MaterialBuilder& Metallic(float value);
    MaterialBuilder& Roughness(float value);
    MaterialBuilder& Emission(const math::Vec3& color);
    MaterialBuilder& AO(float value);
    MaterialBuilder& Alpha(float value);

    MaterialBuilder& SetTexture(const std::string& name, std::shared_ptr<Texture2D> texture);
    MaterialBuilder& SetShader(std::shared_ptr<Shader> shader);

    MaterialBuilder& SetFloat(const std::string& name, float value);
    MaterialBuilder& SetVec3(const std::string& name, const math::Vec3& value);
    MaterialBuilder& SetVec4(const std::string& name, const math::Vec4& value);
    MaterialBuilder& SetMat4Array(const std::string& name, std::vector<math::Mat4> values);
    MaterialBuilder& SetKeyword(const std::string& keyword, bool enabled);

    MaterialBuilder& SetShadingModel(ShadingModel m);
    MaterialBuilder& ShaderIndex(MaterialTypeId id);
    MaterialBuilder& FragmentSnippet(std::string s);
    MaterialBuilder& ShadingSnippet(std::string s);

    MaterialBuilder& SetBlending(Blending b);
    MaterialBuilder& TwoSided(bool v);
    MaterialBuilder& Refraction(bool v);
    MaterialBuilder& Reflective(bool v);
    /** SSR: reflection amount 0..1 (default 0). */
    MaterialBuilder& ReflectiveStrength(float v);

    MaterialBuilder& Name(std::string name);

    /** Build and return a new Material with all set state. Caller owns the result. */
    std::shared_ptr<Material> build() const;

private:
    math::Vec3 m_albedo{0.8f};
    float m_metallic = 0.0f;
    float m_roughness = 0.5f;
    math::Vec3 m_emission{0.0f};
    float m_ao = 1.0f;
    float m_alpha = 1.0f;

    std::shared_ptr<Shader> m_shader;
    std::unordered_map<std::string, std::shared_ptr<Texture2D>> m_textures;
    std::unordered_map<std::string, float> m_floats;
    std::unordered_map<std::string, math::Vec3> m_vec3s;
    std::unordered_map<std::string, math::Vec4> m_vec4s;
    std::unordered_map<std::string, std::vector<math::Mat4>> m_mat4Arrays;
    std::unordered_map<std::string, bool> m_keywords;

    ::eyelib::rendering::ShadingModel m_shadingModel = ShadingModel::Lit;
    MaterialTypeId m_shaderIndex = kDefaultMaterialTypeId;
    std::string m_fragmentSnippet;
    std::string m_shadingSnippet;

    ::eyelib::rendering::Blending m_blending = Blending::Opaque;
    bool m_twoSided = false;
    bool m_refraction = false;
    float m_reflectiveStrength = 0.0f;

    std::string m_name;
};

} // namespace rendering
} // namespace eyelib
