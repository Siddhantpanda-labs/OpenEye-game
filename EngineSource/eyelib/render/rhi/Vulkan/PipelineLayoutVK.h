#pragma once

#include "eyelib/render/rhi/RHI.h"

namespace eyelib {
namespace render {
namespace rhi {

struct PipelineLayoutVK : IPipelineLayout {
    PipelineLayoutDesc m_desc;
    const PipelineLayoutDesc& GetDesc() const override { return m_desc; }
    // VkDescriptorSetLayout + VkPipelineLayout (filled when descriptor system is implemented)
    void* m_descriptorSetLayout = nullptr;
    void* m_pipelineLayout = nullptr;
};

} // namespace rhi
} // namespace render
} // namespace eyelib
