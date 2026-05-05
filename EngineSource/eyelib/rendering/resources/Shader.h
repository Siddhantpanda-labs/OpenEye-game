#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include "eyelib/math/Types.h"
#include "eyelib/rendering/rhi/RHI.h"
#include "eyelib/rendering/shaders/ShaderDescriptor.h"
#include "eyelib/rendering/shaders/ShaderReflection.h"
#include <vector>

namespace eyelib {
namespace rendering {

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

    /** Load by logical shader asset path; backend resolves SPIR-V/GLSL. layoutKind: PushOnly or resource-space (Post, Forward, etc.). */
    bool Load(rhi::IDevice* device, const std::string& assetPath, const rhi::VertexLayout& layout,
             rhi::PrimitiveTopology topology = rhi::PrimitiveTopology::TriangleList,
             rhi::PipelineLayoutKind layoutKind = rhi::PipelineLayoutKind::PushOnly,
             const std::optional<rhi::PipelineAttachmentFormats>& attachmentFormats = std::nullopt);
    /** Load with custom depth/stencil state (e.g. skybox with DepthCompare::LessEqual). */
    bool Load(rhi::IDevice* device, const std::string& assetPath, const rhi::VertexLayout& layout,
             rhi::PrimitiveTopology topology, const rhi::DepthStencilState* depthStencilOverride,
             rhi::PipelineLayoutKind layoutKind = rhi::PipelineLayoutKind::PushOnly,
             const std::optional<rhi::PipelineAttachmentFormats>& attachmentFormats = std::nullopt);
    /** E5.2: Load with keyword defines (e.g. {"SKINNED","SHADOWS"}). Source is loaded and #define KEY 1 prepended per keyword, then compiled. */
    bool Load(rhi::IDevice* device, const std::string& assetPath, const rhi::VertexLayout& layout,
             rhi::PrimitiveTopology topology, const std::vector<std::string>& keywords,
             const rhi::DepthStencilState* depthStencilOverride = nullptr,
             rhi::PipelineLayoutKind layoutKind = rhi::PipelineLayoutKind::PushOnly,
             const std::optional<rhi::PipelineAttachmentFormats>& attachmentFormats = std::nullopt);
    /** E5.4: Re-run last Load(asset path, layout, topology, keywords). Returns true if reload succeeded. No-op if never loaded by asset path. */
    bool Reload();
    /** CPU descriptor for last Load/LoadFromFile (for serialization or creating another instance). Empty if never loaded. */
    ShaderDescriptor GetDescriptor() const;
    /** Create GPU instance from CPU descriptor. Replaces current pipeline if any. */
    bool LoadFromDescriptor(rhi::IDevice* device, const ShaderDescriptor& desc);
    /** RHI path: build pipeline from source with given vertex layout. */
    bool BuildFromSource(rhi::IDevice* device, const std::string& vertexSrc, const std::string& fragmentSrc, const rhi::VertexLayout& layout);
    bool LoadFromFile(rhi::IDevice* device, const std::string& vertexPath, const std::string& fragmentPath,
                      const rhi::VertexLayout& layout,
                      rhi::PrimitiveTopology topology = rhi::PrimitiveTopology::TriangleList,
                      const std::optional<rhi::PipelineAttachmentFormats>& attachmentFormats = std::nullopt);
    /** RHI path: build pipeline with layout + topology. layoutKind for resource-space pipelines (e.g. Post). */
    bool BuildFromSource(rhi::IDevice* device, const std::string& vertexSrc, const std::string& fragmentSrc,
                         const rhi::VertexLayout& layout, rhi::PrimitiveTopology topology,
                         rhi::PipelineLayoutKind layoutKind,
                         const std::optional<rhi::PipelineAttachmentFormats>& attachmentFormats = std::nullopt);
    /** Build from source with explicit depth/stencil state override. */
    bool BuildFromSource(rhi::IDevice* device, const std::string& vertexSrc, const std::string& fragmentSrc,
                         const rhi::VertexLayout& layout, rhi::PrimitiveTopology topology,
                         rhi::PipelineLayoutKind layoutKind, const rhi::DepthStencilState* depthStencilOverride,
                         const std::optional<rhi::PipelineAttachmentFormats>& attachmentFormats = std::nullopt);
    /** Build pipeline from pre-created shaders (e.g. vertex from .spv, fragment from compile). Destroys vs/fs after pipeline creation. */
    bool BuildFromShaders(rhi::IDevice* device, rhi::IShader* vs, rhi::IShader* fs, const rhi::VertexLayout& layout,
                         rhi::PrimitiveTopology topology, rhi::PipelineLayoutKind layoutKind,
                         const std::optional<rhi::PipelineAttachmentFormats>& attachmentFormats = std::nullopt);
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

} // namespace rendering
} // namespace eyelib

