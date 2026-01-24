#include "eyelib/render/resources/Shader.h"
#include "eyelib/core/Log.h"

#include <glad/glad.h>

namespace eyelib {
namespace render {

Shader::Shader() = default;

Shader::~Shader() {
    Destroy();
}

Shader::Shader(Shader&& other) noexcept {
    m_program = other.m_program;
    other.m_program = 0;
}

Shader& Shader::operator=(Shader&& other) noexcept {
    if (this == &other) return *this;
    Destroy();
    m_program = other.m_program;
    other.m_program = 0;
    return *this;
}

GLuint Shader::CompileStage(unsigned int stageType, const std::string& src) {
    const char* csrc = src.c_str();
    GLuint stage = glCreateShader(stageType);
    glShaderSource(stage, 1, &csrc, nullptr);
    glCompileShader(stage);

    GLint ok = 0;
    glGetShaderiv(stage, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint logLen = 0;
        glGetShaderiv(stage, GL_INFO_LOG_LENGTH, &logLen);
        std::string log;
        log.resize(static_cast<size_t>(logLen > 0 ? logLen : 1));
        glGetShaderInfoLog(stage, logLen, nullptr, log.data());
        core::Log::ErrorF("Shader compile failed: %s", log.c_str());
        glDeleteShader(stage);
        return 0;
    }
    return stage;
}

bool Shader::BuildFromSource(const std::string& vertexSrc, const std::string& fragmentSrc) {
    Destroy();

    GLuint vs = CompileStage(GL_VERTEX_SHADER, vertexSrc);
    if (!vs) return false;
    GLuint fs = CompileStage(GL_FRAGMENT_SHADER, fragmentSrc);
    if (!fs) { glDeleteShader(vs); return false; }

    m_program = glCreateProgram();
    glAttachShader(m_program, vs);
    glAttachShader(m_program, fs);
    glLinkProgram(m_program);

    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok = 0;
    glGetProgramiv(m_program, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint logLen = 0;
        glGetProgramiv(m_program, GL_INFO_LOG_LENGTH, &logLen);
        std::string log;
        log.resize(static_cast<size_t>(logLen > 0 ? logLen : 1));
        glGetProgramInfoLog(m_program, logLen, nullptr, log.data());
        core::Log::ErrorF("Shader link failed: %s", log.c_str());
        Destroy();
        return false;
    }

    return true;
}

void Shader::Destroy() {
    if (m_program) {
        glDeleteProgram(m_program);
        m_program = 0;
    }
}

void Shader::Bind() const {
    if (m_program) glUseProgram(m_program);
}

void Shader::Unbind() const {
    glUseProgram(0);
}

void Shader::SetInt(const std::string& name, int value) const {
    if (!m_program) return;
    GLint loc = glGetUniformLocation(m_program, name.c_str());
    if (loc >= 0) glUniform1i(loc, value);
}

void Shader::SetFloat(const std::string& name, float value) const {
    if (!m_program) return;
    GLint loc = glGetUniformLocation(m_program, name.c_str());
    if (loc >= 0) glUniform1f(loc, value);
}

void Shader::SetVec3(const std::string& name, const math::Vec3& value) const {
    if (!m_program) return;
    GLint loc = glGetUniformLocation(m_program, name.c_str());
    if (loc >= 0) glUniform3fv(loc, 1, &value.x);
}

void Shader::SetVec4(const std::string& name, const math::Vec4& value) const {
    if (!m_program) return;
    GLint loc = glGetUniformLocation(m_program, name.c_str());
    if (loc >= 0) glUniform4fv(loc, 1, &value.x);
}

void Shader::SetMat4(const std::string& name, const math::Mat4& value) const {
    if (!m_program) return;
    GLint loc = glGetUniformLocation(m_program, name.c_str());
    if (loc >= 0) glUniformMatrix4fv(loc, 1, GL_FALSE, &value[0][0]);
}

} // namespace render
} // namespace eyelib

