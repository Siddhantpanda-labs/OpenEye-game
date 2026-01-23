#pragma once

#include <string>

// OpenGL types are implementation-only; keep them out of headers where possible.
// Here we keep GLuint as an unsigned int without including glad in the header.
using GLuint = unsigned int;

namespace eyelib {
namespace render {

class Shader {
public:
    Shader();
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    Shader(Shader&& other) noexcept;
    Shader& operator=(Shader&& other) noexcept;

    bool BuildFromSource(const std::string& vertexSrc, const std::string& fragmentSrc);
    void Destroy();

    void Bind() const;
    void Unbind() const;

    bool IsValid() const { return m_program != 0; }
    GLuint Program() const { return m_program; }

private:
    GLuint m_program = 0;

    static GLuint CompileStage(unsigned int stageType, const std::string& src);
};

} // namespace render
} // namespace eyelib

