#pragma once

#include "eyelib/render/rhi/RHI.h"
#include <vulkan/vulkan_core.h>

namespace eyelib {
namespace render {
namespace rhi {

struct DeviceVk;
struct ShaderVk;

struct PipelineVk : IPipeline {
    static PipelineVk* CreateGraphics(DeviceVk* device, const PipelineDesc& desc);
    static PipelineVk* CreateCompute(DeviceVk* device, ShaderVk* computeShader);
    static void Destroy(DeviceVk* device, PipelineVk* pipeline);

    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_layout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VertexLayout m_vertexLayout;
    PrimitiveTopology m_topology = PrimitiveTopology::TriangleList;
    BlendState m_blend;
    DepthStencilState m_depthStencil;
    RasterState m_raster;
    DeviceVk* m_device = nullptr;
};

} // namespace rhi
} // namespace render
} // namespace eyelib
