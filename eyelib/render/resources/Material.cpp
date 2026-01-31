#include "eyelib/render/resources/Material.h"
#include "eyelib/render/resources/Shader.h"
#include "eyelib/render/resources/Texture2D.h"

#include <functional>

namespace eyelib {
namespace render {

Material::Material() = default;

Material::Material(std::shared_ptr<Shader> shader) : m_shader(std::move(shader)) {}

void Material::SetShader(std::shared_ptr<Shader> shader) {
    m_shader = std::move(shader);
    m_hashDirty = true;
}

void Material::SetTexture(const std::string& name, std::shared_ptr<Texture2D> texture) {
    m_textures[name] = std::move(texture);
    m_hashDirty = true;
}

std::shared_ptr<Texture2D> Material::GetTexture(const std::string& name) const {
    auto it = m_textures.find(name);
    return it != m_textures.end() ? it->second : nullptr;
}

void Material::SetAlbedo(const math::Vec3& color) { m_albedo = color; m_hashDirty = true; }
void Material::SetMetallic(float v) { m_metallic = v; m_hashDirty = true; }
void Material::SetRoughness(float v) { m_roughness = v; m_hashDirty = true; }
void Material::SetEmission(const math::Vec3& color) { m_emission = color; m_hashDirty = true; }
void Material::SetAO(float v) { m_ao = v; m_hashDirty = true; }
void Material::SetAlpha(float v) { m_alpha = v; m_hashDirty = true; }

void Material::SetFloat(const std::string& name, float value) {
    m_floats[name] = value;
    m_hashDirty = true;
}

void Material::SetVec3(const std::string& name, const math::Vec3& value) {
    m_vec3s[name] = value;
    m_hashDirty = true;
}

void Material::SetVec4(const std::string& name, const math::Vec4& value) {
    m_vec4s[name] = value;
    m_hashDirty = true;
}

void Material::SetMat4Array(const std::string& name, const std::vector<math::Mat4>& values) {
    m_mat4Arrays[name] = values;
    m_hashDirty = true;
}

float Material::GetFloat(const std::string& name, float defaultValue) const {
    auto it = m_floats.find(name);
    return it != m_floats.end() ? it->second : defaultValue;
}

math::Vec3 Material::GetVec3(const std::string& name, const math::Vec3& defaultValue) const {
    auto it = m_vec3s.find(name);
    return it != m_vec3s.end() ? it->second : defaultValue;
}

math::Vec4 Material::GetVec4(const std::string& name, const math::Vec4& defaultValue) const {
    auto it = m_vec4s.find(name);
    return it != m_vec4s.end() ? it->second : defaultValue;
}

void Material::SetKeyword(const std::string& keyword, bool enabled) {
    m_keywords[keyword] = enabled;
    m_hashDirty = true;
}

bool Material::IsKeywordEnabled(const std::string& keyword) const {
    auto it = m_keywords.find(keyword);
    return it != m_keywords.end() && it->second;
}

void Material::Bind() const {
    if (!m_shader || !m_shader->IsValid()) return;

    m_shader->Bind();

    int unit = 0;
    for (const auto& p : m_textures) {
        if (p.second && p.second->IsValid()) {
            p.second->Bind(unit);
            m_shader->SetInt(p.first, unit);
            ++unit;
        }
    }

    m_shader->SetVec3("u_Albedo", m_albedo);
    m_shader->SetVec3("u_Emission", m_emission);
    m_shader->SetFloat("u_Metallic", m_metallic);
    m_shader->SetFloat("u_Roughness", m_roughness);
    m_shader->SetFloat("u_AO", m_ao);
    m_shader->SetFloat("u_Alpha", m_alpha);

    for (const auto& p : m_floats) m_shader->SetFloat(p.first, p.second);
    for (const auto& p : m_vec3s) m_shader->SetVec3(p.first, p.second);
    for (const auto& p : m_vec4s) m_shader->SetVec4(p.first, p.second);
    for (const auto& p : m_mat4Arrays) {
        if (!p.second.empty()) {
            m_shader->SetMat4Array(p.first, p.second.data(), p.second.size());
        }
    }
}

size_t Material::GetHash() const {
    if (m_hashDirty) {
        UpdateHash();
        m_hashDirty = false;
    }
    return m_cachedHash;
}

void Material::UpdateHash() const {
    std::hash<std::string> sh;
    std::hash<float> fh;
    std::hash<void*> ph;

    size_t h = 0;
    h ^= ph(m_shader.get()) + 0x9e3779b9 + (h << 6) + (h >> 2);
    for (const auto& p : m_textures) {
        h ^= sh(p.first) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= ph(p.second.get()) + 0x9e3779b9 + (h << 6) + (h >> 2);
    }
    h ^= fh(m_albedo.x) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= fh(m_albedo.y) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= fh(m_albedo.z) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= fh(m_metallic) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= fh(m_roughness) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= fh(m_ao) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= fh(m_alpha) + 0x9e3779b9 + (h << 6) + (h >> 2);

    m_cachedHash = h;
}

void Material::ForEachTexture(std::function<void(const std::string&, const std::shared_ptr<Texture2D>&)> fn) const {
    for (const auto& p : m_textures) fn(p.first, p.second);
}

void Material::ForEachFloat(std::function<void(const std::string&, float)> fn) const {
    for (const auto& p : m_floats) fn(p.first, p.second);
}

void Material::ForEachVec3(std::function<void(const std::string&, const math::Vec3&)> fn) const {
    for (const auto& p : m_vec3s) fn(p.first, p.second);
}

void Material::ForEachVec4(std::function<void(const std::string&, const math::Vec4&)> fn) const {
    for (const auto& p : m_vec4s) fn(p.first, p.second);
}

void Material::ForEachMat4Array(std::function<void(const std::string&, const math::Mat4*, size_t)> fn) const {
    for (const auto& p : m_mat4Arrays) {
        if (!p.second.empty())
            fn(p.first, p.second.data(), p.second.size());
    }
}

} // namespace render
} // namespace eyelib
