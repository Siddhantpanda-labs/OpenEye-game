#pragma once

#include "eyelib/rendering/rhi/RHITypes.h"

namespace eyelib {
namespace rendering {

/**
 * Snapshot of device capabilities for feature registration (doc §5).
 * Filled once at startup from `rhi::IDevice::GetCapabilities()`; features must not re-query the device for caps in hot paths.
 */
struct RenderCapabilities {
    rhi::RHICapabilities gpu{};
};

}  // namespace rendering
}  // namespace eyelib
