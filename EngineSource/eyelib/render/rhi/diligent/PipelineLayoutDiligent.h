#pragma once

#include "eyelib/render/rhi/RHITypes.h"

namespace eyelib {
namespace render {
namespace rhi {

struct PipelineLayoutDiligent : IPipelineLayout {
    PipelineLayoutDesc desc;
    const PipelineLayoutDesc& GetDesc() const override { return desc; }
};

} // namespace rhi
} // namespace render
} // namespace eyelib
