#pragma once

#include <string>
#include "eyelib/math/Types.h"

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

    void SetInt(const std::string& name, int value) const;
    void SetFloat(const std::string& name, float value) const;
    void SetVec3(const std::string& name, const math::Vec3& value) const;
    void SetVec4(const std::string& name, const math::Vec4& value) const;
    void SetMat4(const std::string& name, const math::Mat4& value) const;

    bool IsValid() const { return m_program != 0; }
    GLuint Program() const { return m_program; }

private:
    GLuint m_program = 0;

    static GLuint CompileStage(unsigned int stageType, const std::string& src);
};

} // namespace render
} // namespace eyelib

