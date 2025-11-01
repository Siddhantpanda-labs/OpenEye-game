#include "Material.h"
#include "core/log/Log.h"
#include <functional>

namespace OpenEye {
namespace Rendering {

Material::Material() = default;

Material::Material(std::shared_ptr<Shader> shader) : m_shader(shader) {}

void Material::SetShader(std::shared_ptr<Shader> shader) {
    m_shader = shader;
    m_hashDirty = true;
}

void Material::SetTexture(const std::string& name, std::shared_ptr<Texture> texture) {
    m_textures[name] = texture;
    m_hashDirty = true;
}

std::shared_ptr<Texture> Material::GetTexture(const std::string& name) const {
    auto it = m_textures.find(name);
    return (it != m_textures.end()) ? it->second : nullptr;
}

void Material::SetAlbedo(const glm::vec3& color) {
    m_albedo = color;
    m_hashDirty = true;
}

void Material::SetMetallic(float value) {
    m_metallic = value;
    m_hashDirty = true;
}

void Material::SetRoughness(float value) {
    m_roughness = value;
    m_hashDirty = true;
}

void Material::SetEmission(const glm::vec3& color) {
    m_emission = color;
    m_hashDirty = true;
}

void Material::SetFloat(const std::string& name, float value) {
    m_floatParams[name] = value;
    m_hashDirty = true;
}

void Material::SetVec3(const std::string& name, const glm::vec3& value) {
    m_vec3Params[name] = value;
    m_hashDirty = true;
}

void Material::SetVec4(const std::string& name, const glm::vec4& value) {
    m_vec4Params[name] = value;
    m_hashDirty = true;
}

float Material::GetFloat(const std::string& name, float defaultValue) const {
    auto it = m_floatParams.find(name);
    return (it != m_floatParams.end()) ? it->second : defaultValue;
}

glm::vec3 Material::GetVec3(const std::string& name, const glm::vec3& defaultValue) const {
    auto it = m_vec3Params.find(name);
    return (it != m_vec3Params.end()) ? it->second : defaultValue;
}

glm::vec4 Material::GetVec4(const std::string& name, const glm::vec4& defaultValue) const {
    auto it = m_vec4Params.find(name);
    return (it != m_vec4Params.end()) ? it->second : defaultValue;
}

void Material::Bind() const {
    if (!m_shader) {
        return;
    }
    
    m_shader->Bind();
    
    // Bind textures
    int textureUnit = 0;
    for (const auto& pair : m_textures) {
        if (pair.second) {
            pair.second->Bind(textureUnit);
            m_shader->SetInt(pair.first, textureUnit);
            textureUnit++;
        }
    }
    
    // Set PBR properties (only set uniforms that exist in shader)
    m_shader->SetVec3("u_Albedo", m_albedo);
    m_shader->SetVec3("u_Emission", m_emission);
    
    // Note: u_Metallic and u_Roughness may be optimized out if not used in shader
    // This is fine - they'll be set when shader actually uses them
    if (m_shader->GetUniformInfo("u_Metallic")) {
        m_shader->SetFloat("u_Metallic", m_metallic);
    }
    if (m_shader->GetUniformInfo("u_Roughness")) {
        m_shader->SetFloat("u_Roughness", m_roughness);
    }
    
    // Set custom parameters
    for (const auto& pair : m_floatParams) {
        m_shader->SetFloat(pair.first, pair.second);
    }
    
    for (const auto& pair : m_vec3Params) {
        m_shader->SetVec3(pair.first, pair.second);
    }
    
    for (const auto& pair : m_vec4Params) {
        m_shader->SetVec4(pair.first, pair.second);
    }
}

void Material::SetKeyword(const std::string& keyword, bool enabled) {
    m_keywords[keyword] = enabled;
    m_hashDirty = true;
}

bool Material::IsKeywordEnabled(const std::string& keyword) const {
    auto it = m_keywords.find(keyword);
    return (it != m_keywords.end()) ? it->second : false;
}

size_t Material::GetHash() const {
    if (m_hashDirty) {
        UpdateHash();
        m_hashDirty = false;
    }
    return m_cachedHash;
}

void Material::UpdateHash() const {
    std::hash<std::string> stringHash;
    std::hash<float> floatHash;
    std::hash<void*> pointerHash;
    
    size_t hash = 0;
    
    // Hash shader pointer
    hash ^= pointerHash(m_shader.get()) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    
    // Hash textures
    for (const auto& pair : m_textures) {
        hash ^= stringHash(pair.first) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        hash ^= pointerHash(pair.second.get()) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    }
    
    // Hash PBR properties
    hash ^= floatHash(m_albedo.x) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    hash ^= floatHash(m_albedo.y) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    hash ^= floatHash(m_albedo.z) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    hash ^= floatHash(m_metallic) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    hash ^= floatHash(m_roughness) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    
    m_cachedHash = hash;
}

} // namespace Rendering
} // namespace OpenEye

