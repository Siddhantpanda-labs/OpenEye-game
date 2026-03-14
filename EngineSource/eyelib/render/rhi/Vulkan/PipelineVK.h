#pragma once

#include "eyelib/render/rhi/RHI.h"

namespace eyelib {
namespace render {
namespace rhi {

struct DeviceVK;

/** Build VkGraphicsPipeline from desc (dynamic rendering). Returns VkPipeline or nullptr. Used by DeviceVK. */
void* CreateVkGraphicsPipeline(DeviceVK* device, const PipelineDesc& desc, void* pipelineLayout, void* pipelineCache);

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
    // VkPipeline (filled when pipeline system is implemented)
    void* m_pipeline = nullptr;
};

} // namespace rhi
} // namespace render
} // namespace eyelib
