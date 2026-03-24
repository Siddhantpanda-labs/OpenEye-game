#pragma once

#include "eyelib/render/rhi/RHITypes.h"
#include <optional>
#include <vector>

namespace eyelib {
namespace render {
namespace rhi {

struct PipelineLayoutGL : IPipelineLayout {
    PipelineLayoutDesc desc;
    /** When set, BindDescriptorSet maps logical bindings to global GL slots (UBO/SSBO/tex units). */
    std::optional<IDevice::ResourceSetType> resourceSetType;
    const PipelineLayoutDesc& GetDesc() const override { return desc; }
};

} // namespace rhi
} // namespace render
} // namespace eyelib
