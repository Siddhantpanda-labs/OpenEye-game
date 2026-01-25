#include "Shader.h"
#include "core/log/Log.h"
#include <fstream>
#include <sstream>
#include <regex>
#include <vector>

namespace OpenEye {
namespace Rendering {

Shader::Shader() = default;

Shader::~Shader() {
    if (m_programID != 0) {
        glDeleteProgram(m_programID);
        m_programID = 0;
    }
}

Shader::Shader(Shader&& other) noexcept
    : m_programID(other.m_programID)
    , m_vertexPath(std::move(other.m_vertexPath))
    , m_geometryPath(std::move(other.m_geometryPath))
    , m_fragmentPath(std::move(other.m_fragmentPath))
    , m_uniformLocationCache(std::move(other.m_uniformLocationCache))
    , m_uniformInfo(std::move(other.m_uniformInfo))
{
    other.m_programID = 0;
}

Shader& Shader::operator=(Shader&& other) noexcept {
    if (this != &other) {
        if (m_programID != 0) {
            glDeleteProgram(m_programID);
        }
        
        m_programID = other.m_programID;
        m_vertexPath = std::move(other.m_vertexPath);
        m_geometryPath = std::move(other.m_geometryPath);
        m_fragmentPath = std::move(other.m_fragmentPath);
        m_uniformLocationCache = std::move(other.m_uniformLocationCache);
        m_uniformInfo = std::move(other.m_uniformInfo);
        
        other.m_programID = 0;
    }
    return *this;
}

bool Shader::Compile(const std::string& vertexSource, const std::string& fragmentSource) {
    return Compile(vertexSource, "", fragmentSource);
}

bool Shader::Compile(const std::string& vertexSource, const std::string& geometrySource, const std::string& fragmentSource) {
    if (m_programID != 0) {
        glDeleteProgram(m_programID);
        m_programID = 0;
    }
    m_uniformLocationCache.clear();
    m_uniformInfo.clear();
    
    GLuint vertexID = CompileShader(ShaderType::Vertex, vertexSource);
    if (vertexID == 0) {
        return false;
    }
    
    GLuint geometryID = 0;
    if (!geometrySource.empty()) {
        geometryID = CompileShader(ShaderType::Geometry, geometrySource);
        if (geometryID == 0) {
            glDeleteShader(vertexID);
            return false;
        }
    }
    
    GLuint fragmentID = CompileShader(ShaderType::Fragment, fragmentSource);
    if (fragmentID == 0) {
        glDeleteShader(vertexID);
        if (geometryID != 0) {
            glDeleteShader(geometryID);
        }
        return false;
    }
    
    bool success = LinkProgram(vertexID, geometryID, fragmentID);
    
    glDeleteShader(vertexID);
    if (geometryID != 0) {
        glDeleteShader(geometryID);
    }
    glDeleteShader(fragmentID);
    
    if (success) {
        ReflectUniforms();
    }
    
    return success;
}

bool Shader::LoadFromFile(const std::string& vertexPath, const std::string& fragmentPath) {
    return LoadFromFile(vertexPath, "", fragmentPath);
}

bool Shader::LoadFromFile(const std::string& vertexPath, const std::string& geometryPath, const std::string& fragmentPath) {
    m_vertexPath = vertexPath;
    m_geometryPath = geometryPath;
    m_fragmentPath = fragmentPath;
    
    std::string vertexSource = LoadFile(vertexPath);
    if (vertexSource.empty()) {
        OE_ERROR_F("Failed to load vertex shader: %s", vertexPath.c_str());
        return false;
    }
    
    std::string geometrySource;
    if (!geometryPath.empty()) {
        geometrySource = LoadFile(geometryPath);
        if (geometrySource.empty()) {
            OE_ERROR_F("Failed to load geometry shader: %s", geometryPath.c_str());
            return false;
        }
    }
    
    std::string fragmentSource = LoadFile(fragmentPath);
    if (fragmentSource.empty()) {
        OE_ERROR_F("Failed to load fragment shader: %s", fragmentPath.c_str());
        return false;
    }
    
    // Preprocess shaders (handle #include directives)
    std::string basePath = vertexPath.substr(0, vertexPath.find_last_of("/\\") + 1);
    if (!PreprocessShader(vertexSource, basePath)) {
        return false;
    }
    if (!geometrySource.empty() && !PreprocessShader(geometrySource, basePath)) {
        return false;
    }
    if (!PreprocessShader(fragmentSource, basePath)) {
        return false;
    }
    
    return Compile(vertexSource, geometrySource, fragmentSource);
}

void Shader::Bind() const {
    if (m_programID != 0) {
        glUseProgram(m_programID);
    }
}

void Shader::Unbind() const {
    glUseProgram(0);
}

void Shader::SetBool(const std::string& name, bool value) {
    glUniform1i(GetUniformLocation(name), static_cast<int>(value));
}

void Shader::SetInt(const std::string& name, int value) {
    glUniform1i(GetUniformLocation(name), value);
}

void Shader::SetFloat(const std::string& name, float value) {
    glUniform1f(GetUniformLocation(name), value);
}

void Shader::SetVec2(const std::string& name, const glm::vec2& value) {
    glUniform2fv(GetUniformLocation(name), 1, glm::value_ptr(value));
}

void Shader::SetVec3(const std::string& name, const glm::vec3& value) {
    glUniform3fv(GetUniformLocation(name), 1, glm::value_ptr(value));
}

void Shader::SetVec4(const std::string& name, const glm::vec4& value) {
    glUniform4fv(GetUniformLocation(name), 1, glm::value_ptr(value));
}

void Shader::SetMat3(const std::string& name, const glm::mat3& value) {
    glUniformMatrix3fv(GetUniformLocation(name), 1, GL_FALSE, glm::value_ptr(value));
}

void Shader::SetMat4(const std::string& name, const glm::mat4& value) {
    glUniformMatrix4fv(GetUniformLocation(name), 1, GL_FALSE, glm::value_ptr(value));
}

void Shader::SetIntArray(const std::string& name, const int* values, int count) {
    glUniform1iv(GetUniformLocation(name), count, values);
}

void Shader::SetFloatArray(const std::string& name, const float* values, int count) {
    glUniform1fv(GetUniformLocation(name), count, values);
}

const UniformInfo* Shader::GetUniformInfo(const std::string& name) const {
    auto it = m_uniformInfo.find(name);
    return (it != m_uniformInfo.end()) ? &it->second : nullptr;
}

std::vector<std::string> Shader::GetUniformNames() const {
    std::vector<std::string> names;
    names.reserve(m_uniformInfo.size());
    for (const auto& pair : m_uniformInfo) {
        names.push_back(pair.first);
    }
    return names;
}

bool Shader::Reload() {
    if (m_vertexPath.empty() || m_fragmentPath.empty()) {
        OE_WARN("Cannot reload shader: file paths not set");
        return false;
    }
    
    OE_INFO_F("Reloading shader: %s", m_vertexPath.c_str());
    return LoadFromFile(m_vertexPath, m_geometryPath, m_fragmentPath);
}

GLuint Shader::CompileShader(ShaderType type, const std::string& source) {
    GLuint shaderID = glCreateShader(static_cast<GLenum>(type));
    const char* sourceCStr = source.c_str();
    glShaderSource(shaderID, 1, &sourceCStr, nullptr);
    glCompileShader(shaderID);
    
    // Check compilation status
    GLint success;
    glGetShaderiv(shaderID, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLchar infoLog[1024];
        glGetShaderInfoLog(shaderID, 1024, nullptr, infoLog);
        
        const char* typeName = (type == ShaderType::Vertex) ? "Vertex" :
                              (type == ShaderType::Geometry) ? "Geometry" :
                              (type == ShaderType::Fragment) ? "Fragment" : "Compute";
        
        OE_ERROR_F("%s shader compilation failed:\n%s", typeName, infoLog);
        glDeleteShader(shaderID);
        return 0;
    }
    
    return shaderID;
}

bool Shader::LinkProgram(GLuint vertexID, GLuint geometryID, GLuint fragmentID) {
    m_programID = glCreateProgram();
    glAttachShader(m_programID, vertexID);
    if (geometryID != 0) {
        glAttachShader(m_programID, geometryID);
    }
    glAttachShader(m_programID, fragmentID);
    glLinkProgram(m_programID);
    
    GLint success;
    glGetProgramiv(m_programID, GL_LINK_STATUS, &success);
    if (!success) {
        GLchar infoLog[1024];
        glGetProgramInfoLog(m_programID, 1024, nullptr, infoLog);
        OE_ERROR_F("Shader program linking failed:\n%s", infoLog);
        glDeleteProgram(m_programID);
        m_programID = 0;
        return false;
    }
    
    return true;
}

void Shader::ReflectUniforms() {
    if (m_programID == 0) return;
    
    GLint uniformCount = 0;
    glGetProgramiv(m_programID, GL_ACTIVE_UNIFORMS, &uniformCount);
    
    GLint maxNameLength = 0;
    glGetProgramiv(m_programID, GL_ACTIVE_UNIFORM_MAX_LENGTH, &maxNameLength);
    
    for (GLint i = 0; i < uniformCount; ++i) {
        std::vector<GLchar> nameBuffer(maxNameLength);
        GLint size;
        GLenum type;
        
        glGetActiveUniform(m_programID, i, maxNameLength, nullptr, &size, &type, nameBuffer.data());
        
        std::string name(nameBuffer.data());
        
        // Remove array brackets for arrays
        size_t bracketPos = name.find('[');
        if (bracketPos != std::string::npos) {
            name = name.substr(0, bracketPos);
        }
        
        int location = glGetUniformLocation(m_programID, name.c_str());
        
        UniformInfo info;
        info.name = name;
        info.type = type;
        info.location = location;
        info.size = size;
        
        m_uniformInfo[name] = info;
    }
}

int Shader::GetUniformLocation(const std::string& name) const {
    auto it = m_uniformLocationCache.find(name);
    if (it != m_uniformLocationCache.end()) {
        return it->second;
    }
    
    int location = glGetUniformLocation(m_programID, name.c_str());
    if (location == -1) {
        // Only warn once per uniform
        static std::unordered_map<std::string, bool> warnedUniforms;
        if (warnedUniforms.find(name) == warnedUniforms.end()) {
            OE_WARN_F("Uniform '%s' not found in shader", name.c_str());
            warnedUniforms[name] = true;
        }
    }
    
    m_uniformLocationCache[name] = location;
    return location;
}

std::string Shader::LoadFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        OE_ERROR_F("Failed to open shader file: %s", path.c_str());
        return "";
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

bool Shader::PreprocessShader(std::string& source, const std::string& basePath) {
    // Simple #include preprocessor
    std::regex includeRegex(R"(#include\s+["<](.+?)[">])");
    std::smatch match;
    
    std::string::const_iterator searchStart = source.cbegin();
    while (std::regex_search(searchStart, source.cend(), match, includeRegex)) {
        std::string includeFile = match[1].str();
        std::string includePath = basePath + includeFile;
        
        std::string includedContent = LoadFile(includePath);
        if (includedContent.empty()) {
            OE_ERROR_F("Failed to include shader file: %s", includePath.c_str());
            return false;
        }
        
        // Recursively preprocess included file
        if (!PreprocessShader(includedContent, basePath)) {
            return false;
        }
        
        // Replace #include directive with file content
        std::string fullMatch = match[0].str();
        source.replace(match[0].first, match[0].second, includedContent);
        
        searchStart = source.cbegin();
    }
    
    return true;
}

} // namespace Rendering
} // namespace OpenEye

