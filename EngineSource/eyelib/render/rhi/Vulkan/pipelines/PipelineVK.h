#pragma once

#include "eyelib/render/rhi/RHI.h"

namespace eyelib {
namespace render {
namespace rhi {

struct DeviceVK;

/** Build VkGraphicsPipeline from desc (dynamic rendering). Returns VkPipeline or nullptr. Used by DeviceVK. */
void* CreateVkGraphicsPipeline(DeviceVK* device, const PipelineDesc& desc, void* pipelineLayout, void* pipelineCache);
/** Build VkComputePipeline from a shader module. Returns VkPipeline or nullptr. */
void* CreateVkComputePipeline(DeviceVK* device, void* pipelineLayout, void* pipelineCache, void* shaderModule);
IPipeline* CreateGraphicsPipelineVK(DeviceVK* device, const PipelineDesc& desc);
IPipeline* CreateGraphicsPipelineFromShaderAssetVK(DeviceVK* device, const ShaderGraphicsPipelineCreateInfo& info);
IPipeline* CreateComputePipelineVK(DeviceVK* device, void* computeShader);
IPipeline* CreateComputePipelineVK(DeviceVK* device, void* computeShader, PipelineLayoutKind layoutKind);

struct PipelineVK : IPipeline {
    explicit PipelineVK(DeviceVK* device);
    ~PipelineVK() override;

    void GetComputeBlockPresence(ComputeBlockPresence& out) const override;

    DeviceVK* m_device = nullptr;
    VertexLayout m_vertexLayout;
    PrimitiveTopology m_topology = PrimitiveTopology::TriangleList;
    uint32_t m_patchVertexCount = 3;
    BlendState m_blend;
    DepthStencilState m_depthStencil;
    RasterState m_raster;
    ComputeBlockPresence m_computeBlockPresence{};
    bool m_isCompute = false;
    void* m_pipeline = nullptr;           // VkPipeline
    /** Non-zero for graphics pipelines in device cache (shared VkPipeline refcounted by DeviceVK). */
    uint64_t m_graphicsCacheHash = 0;
    /** Layout kind used to create this pipeline. PushOnly = 0 sets, push constants only; Compute may use UpdateComputeSet0; other = caller uses BindDescriptorSets. */
    PipelineLayoutKind m_layoutKind = PipelineLayoutKind::PushOnly;
    /** When m_layoutKind is resource-space (non-PushOnly), this is the VkPipelineLayout. Used for BindDescriptorSets and push constants. */
    void* m_nativePipelineLayout = nullptr;
    /** Number of descriptor sets required when binding from set 0 (0 = PushOnly). */
    uint8_t m_requiredSetCount = 0;
};

} // namespace rhi
} // namespace render
} // namespace eyelib
