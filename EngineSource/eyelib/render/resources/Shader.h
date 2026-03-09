#pragma once

#include <cstddef>
#include <string>
#include "eyelib/math/Types.h"
#include "eyelib/render/rhi/RHI.h"
#include "eyelib/render/ShaderDescriptor.h"
#include "eyelib/render/ShaderReflection.h"
#include <vector>

namespace eyelib {
namespace render {

/** GPU shader instance (device-bound). CPU representation is ShaderDescriptor (Phase 3 asset vs instance).
 *  Device is passed explicitly to Load/BuildFromSource/Bind; no SetDevice/GetDevice. */
class Shader {
public:
    Shader();
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    Shader(Shader&& other) noexcept;
    Shader& operator=(Shader&& other) noexcept;

    /** Load by logical name; backend resolves paths/format (Option 2: backend-owned shader loading). */
    bool Load(rhi::IDevice* device, const std::string& shaderName, const rhi::VertexLayout& layout,
             rhi::PrimitiveTopology topology = rhi::PrimitiveTopology::TriangleList);
    /** Load with custom depth/stencil state (e.g. skybox with DepthCompare::LessEqual). */
    bool Load(rhi::IDevice* device, const std::string& shaderName, const rhi::VertexLayout& layout,
             rhi::PrimitiveTopology topology, const rhi::DepthStencilState* depthStencilOverride);
    /** E5.2: Load with keyword defines (e.g. {"SKINNED","SHADOWS"}). Source is loaded and #define KEY 1 prepended per keyword, then compiled. */
    bool Load(rhi::IDevice* device, const std::string& shaderName, const rhi::VertexLayout& layout,
             rhi::PrimitiveTopology topology, const std::vector<std::string>& keywords,
             const rhi::DepthStencilState* depthStencilOverride = nullptr);
    /** E5.4: Re-run last Load(name, layout, topology, keywords). Returns true if reload succeeded. No-op if never loaded by name. */
    bool Reload();
    /** CPU descriptor for last Load/LoadFromFile (for serialization or creating another instance). Empty if never loaded. */
    ShaderDescriptor GetDescriptor() const;
    /** Create GPU instance from CPU descriptor. Replaces current pipeline if any. */
    bool LoadFromDescriptor(rhi::IDevice* device, const ShaderDescriptor& desc);
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

    /** Bind pipeline. Device must be non-null. */
    void Bind(rhi::IDevice* device) const;
    void Unbind() const;

    void SetBool(const std::string& name, bool value) const;
    void SetInt(const std::string& name, int value) const;
    void SetUint(const std::string& name, uint32_t value) const;
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

private:
    /** Used internally by Set* and Reload(); returns device that created the pipeline. */
    rhi::IDevice* GetDevice() const { return m_deviceRHI; }
    rhi::IDevice* m_deviceRHI = nullptr;
    rhi::IPipeline* m_pipelineRHI = nullptr;
    ShaderReflection m_reflection;
    /** CPU reload state (Phase 2.5). Reload() uses this; GetDescriptor() returns it. */
    ShaderDescriptor m_descriptor;
};

} // namespace render
} // namespace eyelib

