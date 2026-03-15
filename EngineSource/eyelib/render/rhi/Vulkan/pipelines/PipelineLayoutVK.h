#pragma once

#include "eyelib/render/rhi/RHI.h"

namespace eyelib {
namespace render {
namespace rhi {

struct PipelineLayoutVK : IPipelineLayout {
    PipelineLayoutDesc m_desc;
    const PipelineLayoutDesc& GetDesc() const override { return m_desc; }
    void* m_descriptorSetLayout = nullptr;
    void* m_pipelineLayout = nullptr;
    /** If false, set layout is owned elsewhere (e.g. resource-space); destroy only pipeline layout and wrapper. */
    bool m_ownsSetLayout = true;
};

} // namespace rhi
} // namespace render
} // namespace eyelib
