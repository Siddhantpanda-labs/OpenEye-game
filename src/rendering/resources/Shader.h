#pragma once

#include <string>
#include <unordered_map>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace OpenEye {
namespace Rendering {

enum class ShaderType {
    Vertex = GL_VERTEX_SHADER,
    Fragment = GL_FRAGMENT_SHADER,
    Geometry = GL_GEOMETRY_SHADER,
    Compute = GL_COMPUTE_SHADER
};

struct UniformInfo {
    std::string name;
    GLenum type;
    int location;
    int size;
};

class Shader {
public:
    Shader();
    ~Shader();
    
    // Non-copyable
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    
    // Movable
    Shader(Shader&& other) noexcept;
    Shader& operator=(Shader&& other) noexcept;
    
    // Compile shader from source
    bool Compile(const std::string& vertexSource, const std::string& fragmentSource);
    bool Compile(const std::string& vertexSource, const std::string& geometrySource, const std::string& fragmentSource);
    
    // Compile shader from file
    bool LoadFromFile(const std::string& vertexPath, const std::string& fragmentPath);
    bool LoadFromFile(const std::string& vertexPath, const std::string& geometryPath, const std::string& fragmentPath);
    
    // Use shader program
    void Bind() const;
    void Unbind() const;
    
    // Get OpenGL program ID
    GLuint GetID() const { return m_programID; }
    
    // Check if shader is valid
    bool IsValid() const { return m_programID != 0; }
    
    // Uniform setters
    void SetBool(const std::string& name, bool value);
    void SetInt(const std::string& name, int value);
    void SetFloat(const std::string& name, float value);
    void SetVec2(const std::string& name, const glm::vec2& value);
    void SetVec3(const std::string& name, const glm::vec3& value);
    void SetVec4(const std::string& name, const glm::vec4& value);
    void SetMat3(const std::string& name, const glm::mat3& value);
    void SetMat4(const std::string& name, const glm::mat4& value);
    
    // Uniform array setters
    void SetIntArray(const std::string& name, const int* values, int count);
    void SetFloatArray(const std::string& name, const float* values, int count);
    
    // Reflection: Get uniform info
    const UniformInfo* GetUniformInfo(const std::string& name) const;
    std::vector<std::string> GetUniformNames() const;
    
    // Reload shader (hot-reload support)
    bool Reload();

private:
    GLuint m_programID = 0;
    std::string m_vertexPath;
    std::string m_geometryPath;
    std::string m_fragmentPath;
    
    mutable std::unordered_map<std::string, int> m_uniformLocationCache;
    std::unordered_map<std::string, UniformInfo> m_uniformInfo;
    
    // Internal compilation
    GLuint CompileShader(ShaderType type, const std::string& source);
    bool LinkProgram(GLuint vertexID, GLuint geometryID, GLuint fragmentID);
    void ReflectUniforms();
    int GetUniformLocation(const std::string& name) const;
    
    // File loading
    std::string LoadFile(const std::string& path);
    bool PreprocessShader(std::string& source, const std::string& basePath);
};

} // namespace Rendering
} // namespace OpenEye

