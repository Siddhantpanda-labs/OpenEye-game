#pragma once

#include "eyelib/rendering/rhi/RHI.h"

namespace eyelib {
namespace rendering {
namespace rhi {

/** Portable **`IPipelineLayout`** wrapper: **`m_pipelineLayout`** is **`VkPipelineLayout`**; **`m_descriptorSetLayout`** is one set layout
 *  when the RHI builds a minimal layout, or a handle owned elsewhere (**`m_ownsSetLayout` == false**, e.g. resource-space layouts from **`VulkanResourceSpaces`**).
 *  Pass / push constant conventions are defined per layout kind in **`vkres`** and binding docs. */
struct PipelineLayoutVK : IPipelineLayout {
    PipelineLayoutDesc m_desc;
    const PipelineLayoutDesc& GetDesc() const override { return m_desc; }
    void* m_descriptorSetLayout = nullptr;
    void* m_pipelineLayout = nullptr;
    /** If false, set layout is owned elsewhere (e.g. resource-space); destroy only pipeline layout and wrapper. */
    bool m_ownsSetLayout = true;
};

} // namespace rhi
} // namespace rendering
} // namespace eyelib
