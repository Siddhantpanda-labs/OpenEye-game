#include "eyelib/rendering/materials/MaterialBuilder.h"
#include "eyelib/rendering/materials/Material.h"
#include "eyelib/rendering/resources/Texture2D.h"

namespace eyelib {
namespace rendering {

MaterialBuilder& MaterialBuilder::Albedo(const math::Vec3& color) {
    m_albedo = color;
    return *this;
}
MaterialBuilder& MaterialBuilder::Metallic(float value) {
    m_metallic = value;
    return *this;
}
MaterialBuilder& MaterialBuilder::Roughness(float value) {
    m_roughness = value;
    return *this;
}
MaterialBuilder& MaterialBuilder::Emission(const math::Vec3& color) {
    m_emission = color;
    return *this;
}
MaterialBuilder& MaterialBuilder::AO(float value) {
    m_ao = value;
    return *this;
}
MaterialBuilder& MaterialBuilder::Alpha(float value) {
    m_alpha = value;
    return *this;
}

MaterialBuilder& MaterialBuilder::SetTexture(const std::string& name, std::shared_ptr<Texture2D> texture) {
    m_textures[name] = std::move(texture);
    return *this;
}
MaterialBuilder& MaterialBuilder::SetShader(std::shared_ptr<Shader> shader) {
    m_shader = std::move(shader);
    return *this;
}

MaterialBuilder& MaterialBuilder::SetFloat(const std::string& name, float value) {
    m_floats[name] = value;
    return *this;
}
MaterialBuilder& MaterialBuilder::SetVec3(const std::string& name, const math::Vec3& value) {
    m_vec3s[name] = value;
    return *this;
}
MaterialBuilder& MaterialBuilder::SetVec4(const std::string& name, const math::Vec4& value) {
    m_vec4s[name] = value;
    return *this;
}
MaterialBuilder& MaterialBuilder::SetMat4Array(const std::string& name, std::vector<math::Mat4> values) {
    m_mat4Arrays[name] = std::move(values);
    return *this;
}
MaterialBuilder& MaterialBuilder::SetKeyword(const std::string& keyword, bool enabled) {
    m_keywords[keyword] = enabled;
    return *this;
}

MaterialBuilder& MaterialBuilder::SetShadingModel(ShadingModel m) {
    m_shadingModel = m;
    return *this;
}
MaterialBuilder& MaterialBuilder::ShaderIndex(MaterialTypeId id) {
    m_shaderIndex = id;
    return *this;
}
MaterialBuilder& MaterialBuilder::FragmentSnippet(std::string s) {
    m_fragmentSnippet = std::move(s);
    return *this;
}
MaterialBuilder& MaterialBuilder::ShadingSnippet(std::string s) {
    m_shadingSnippet = std::move(s);
    return *this;
}

MaterialBuilder& MaterialBuilder::SetBlending(Blending b) {
    m_blending = b;
    return *this;
}
MaterialBuilder& MaterialBuilder::TwoSided(bool v) {
    m_twoSided = v;
    return *this;
}
MaterialBuilder& MaterialBuilder::Refraction(bool v) {
    m_refraction = v;
    return *this;
}
MaterialBuilder& MaterialBuilder::Reflective(bool v) {
    m_reflectiveStrength = v ? 1.0f : 0.0f;
    return *this;
}
MaterialBuilder& MaterialBuilder::ReflectiveStrength(float v) {
    m_reflectiveStrength = (v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v);
    return *this;
}

MaterialBuilder& MaterialBuilder::Name(std::string name) {
    m_name = std::move(name);
    return *this;
}

std::shared_ptr<Material> MaterialBuilder::build() const {
    auto mat = std::make_shared<Material>();
    mat->SetMaterialShaderId(m_shaderIndex);
    mat->SetAlbedo(m_albedo);
    mat->SetMetallic(m_metallic);
    mat->SetRoughness(m_roughness);
    mat->SetEmission(m_emission);
    mat->SetAO(m_ao);
    mat->SetAlpha(m_alpha);
    for (const auto& p : m_textures) mat->SetTexture(p.first, p.second);
    for (const auto& p : m_floats) mat->SetFloat(p.first, p.second);
    for (const auto& p : m_vec3s) mat->SetVec3(p.first, p.second);
    for (const auto& p : m_vec4s) mat->SetVec4(p.first, p.second);
    for (const auto& p : m_mat4Arrays) mat->SetMat4Array(p.first, p.second);
    for (const auto& p : m_keywords) mat->SetKeyword(p.first, p.second);
    mat->SetShadingModel(m_shadingModel);
    mat->SetShaderIndex(m_shaderIndex);
    if (!m_fragmentSnippet.empty()) mat->SetFragmentSnippet(m_fragmentSnippet);
    if (!m_shadingSnippet.empty()) mat->SetShadingSnippet(m_shadingSnippet);
    mat->SetBlending(m_blending);
    mat->SetTwoSided(m_twoSided);
    mat->SetRefraction(m_refraction);
    mat->SetReflectiveStrength(m_reflectiveStrength);
    return mat;
}

} // namespace rendering
} // namespace eyelib
