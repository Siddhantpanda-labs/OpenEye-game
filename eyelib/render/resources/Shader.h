#pragma once

#include <cstddef>
#include <string>
#include "eyelib/math/Types.h"
#include "eyelib/render/rhi/RHI.h"
#include "eyelib/render/ShaderReflection.h"
#include <vector>

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
    /** Load by logical name; backend resolves paths/format (Option 2: backend-owned shader loading). */
    bool Load(rhi::IDevice* device, const std::string& shaderName, const rhi::VertexLayout& layout,
             rhi::PrimitiveTopology topology = rhi::PrimitiveTopology::TriangleList);
    /** E5.2: Load with keyword defines (e.g. {"SKINNED","SHADOWS"}). Source is loaded and #define KEY 1 prepended per keyword, then compiled. */
    bool Load(rhi::IDevice* device, const std::string& shaderName, const rhi::VertexLayout& layout,
             rhi::PrimitiveTopology topology, const std::vector<std::string>& keywords);
    /** E5.4: Re-run last Load(name, layout, topology, keywords). Returns true if reload succeeded. No-op if never loaded by name. */
    bool Reload();
    /** RHI path: build pipeline from source with given vertex layout. */
    bool BuildFromSource(rhi::IDevice* device, const std::string& vertexSrc, const std::string& fragmentSrc, const rhi::VertexLayout& layout);
    bool LoadFromFile(rhi::IDevice* device, const std::string& vertexPath, const std::string& fragmentPath, const rhi::VertexLayout& layout);
    /** RHI path: build pipeline with layout + topology. */
    bool BuildFromSource(rhi::IDevice* device, const std::string& vertexSrc, const std::string& fragmentSrc,
                         const rhi::VertexLayout& layout, rhi::PrimitiveTopology topology);
    bool LoadFromFile(rhi::IDevice* device, const std::string& vertexPath, const std::string& fragmentPath,
                      const rhi::VertexLayout& layout, rhi::PrimitiveTopology topology);
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
    /** Set 64-bit bindless sampler handle (E9). No-op if bindless not supported. */
    void SetHandle64(const std::string& name, uint64_t value) const;

    bool IsValid() const { return m_pipelineRHI != nullptr; }
    rhi::IPipeline* GetPipelineRHI() const { return m_pipelineRHI; }

    /** E5.1: Reflected uniforms/samplers from last BuildFromSource (empty if loaded by name only). */
    const ShaderReflection& GetReflection() const { return m_reflection; }

    /** Set device to use when calling LoadFromFile/BuildFromSource without device. Also used in Bind/Set* if set. */
    void SetDevice(rhi::IDevice* device) { m_injectedDevice = device; }

private:
    rhi::IDevice* GetDevice() const;
    rhi::IDevice* m_injectedDevice = nullptr;
    rhi::IDevice* m_deviceRHI = nullptr;
    rhi::IPipeline* m_pipelineRHI = nullptr;
    ShaderReflection m_reflection;
    /** E5.4: Last load params for Reload(). */
    std::string m_lastShaderName;
    rhi::VertexLayout m_lastLayout{};
    rhi::PrimitiveTopology m_lastTopology = rhi::PrimitiveTopology::TriangleList;
    std::vector<std::string> m_lastKeywords;
    bool m_lastLoadByName = false;
};

} // namespace render
} // namespace eyelib

