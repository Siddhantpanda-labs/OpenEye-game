#pragma once

#include <cstddef>
#include <string>
#include "eyelib/math/Types.h"
#include "eyelib/render/rhi/RHI.h"

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
    bool LoadFromFile(const std::string& vertexPath, const std::string& fragmentPath);
    /** RHI path: build pipeline from source with given vertex layout. */
    bool BuildFromSource(rhi::IDevice* device, const std::string& vertexSrc, const std::string& fragmentSrc, const rhi::VertexLayout& layout);
    bool LoadFromFile(rhi::IDevice* device, const std::string& vertexPath, const std::string& fragmentPath, const rhi::VertexLayout& layout);
    void Destroy();
    void Destroy(rhi::IDevice* device);

    void Bind() const;
    void Unbind() const;

    void SetBool(const std::string& name, bool value) const;
    void SetInt(const std::string& name, int value) const;
    void SetFloat(const std::string& name, float value) const;
    void SetVec2(const std::string& name, const math::Vec2& value) const;
    void SetVec3(const std::string& name, const math::Vec3& value) const;
    void SetVec4(const std::string& name, const math::Vec4& value) const;
    void SetMat4(const std::string& name, const math::Mat4& value) const;
    void SetMat4Array(const std::string& name, const math::Mat4* values, size_t count) const;

    bool IsValid() const { return m_program != 0 || m_pipelineRHI != nullptr; }
    GLuint Program() const { return m_program; }
    rhi::IPipeline* GetPipelineRHI() const { return m_pipelineRHI; }

private:
    GLuint m_program = 0;
    rhi::IDevice* m_deviceRHI = nullptr;
    rhi::IPipeline* m_pipelineRHI = nullptr;
    mutable GLuint m_bonesUBO = 0;
    mutable size_t m_bonesUBOSize = 0;

    static GLuint CompileStage(unsigned int stageType, const std::string& src);
};

} // namespace render
} // namespace eyelib

