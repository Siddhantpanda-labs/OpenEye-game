#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <glm/glm.hpp>
#include "Shader.h"
#include "Texture.h"

namespace OpenEye {
namespace Rendering {

class Material {
public:
    Material();
    explicit Material(std::shared_ptr<Shader> shader);
    ~Material() = default;
    
    // Shader
    void SetShader(std::shared_ptr<Shader> shader);
    std::shared_ptr<Shader> GetShader() const { return m_shader; }
    
    // Textures
    void SetTexture(const std::string& name, std::shared_ptr<Texture> texture);
    std::shared_ptr<Texture> GetTexture(const std::string& name) const;
    
    // Material properties (for PBR)
    void SetAlbedo(const glm::vec3& color);
    void SetMetallic(float value);
    void SetRoughness(float value);
    void SetEmission(const glm::vec3& color);
    
    glm::vec3 GetAlbedo() const { return m_albedo; }
    float GetMetallic() const { return m_metallic; }
    float GetRoughness() const { return m_roughness; }
    glm::vec3 GetEmission() const { return m_emission; }
    
    // Custom float parameters
    void SetFloat(const std::string& name, float value);
    void SetVec3(const std::string& name, const glm::vec3& value);
    void SetVec4(const std::string& name, const glm::vec4& value);
    
    float GetFloat(const std::string& name, float defaultValue = 0.0f) const;
    glm::vec3 GetVec3(const std::string& name, const glm::vec3& defaultValue = glm::vec3(0.0f)) const;
    glm::vec4 GetVec4(const std::string& name, const glm::vec4& defaultValue = glm::vec4(0.0f)) const;
    
    // Bind material for rendering
    void Bind() const;
    
    // Get hash for batching (same material = same hash)
    size_t GetHash() const;
    
    // Shader keywords/variants (for future use)
    void SetKeyword(const std::string& keyword, bool enabled);
    bool IsKeywordEnabled(const std::string& keyword) const;

private:
    std::shared_ptr<Shader> m_shader;
    std::unordered_map<std::string, std::shared_ptr<Texture>> m_textures;
    std::unordered_map<std::string, float> m_floatParams;
    std::unordered_map<std::string, glm::vec3> m_vec3Params;
    std::unordered_map<std::string, glm::vec4> m_vec4Params;
    std::unordered_map<std::string, bool> m_keywords;
    
    // PBR properties
    glm::vec3 m_albedo = glm::vec3(0.8f);
    float m_metallic = 0.0f;
    float m_roughness = 0.5f;
    glm::vec3 m_emission = glm::vec3(0.0f);
    
    mutable size_t m_cachedHash = 0;
    mutable bool m_hashDirty = true;
    
    void UpdateHash() const;
};

} // namespace Rendering
} // namespace OpenEye

