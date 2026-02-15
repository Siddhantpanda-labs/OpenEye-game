#pragma once

#include "eyelib/render/rhi/RHITypes.h"
#include <vector>

namespace eyelib {
namespace render {
namespace rhi {

struct PipelineLayoutGL : IPipelineLayout {
    PipelineLayoutDesc desc;
    const PipelineLayoutDesc& GetDesc() const override { return desc; }
};

} // namespace rhi
} // namespace render
} // namespace eyelib
