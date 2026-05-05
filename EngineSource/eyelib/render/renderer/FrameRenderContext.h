#pragma once

#include "eyelib/render/rhi/RHI.h"
#include <cstdint>

namespace eyelib {
namespace render {

struct RenderFrameData;

/**
 * Per-frame immutable inputs for pass systems and graph executors (see documentation/refractor/Render_refractor.md).
 * Populated by RenderPipeline each frame before FrameGraph::Execute.
 */
struct FrameRenderContext {
    rhi::IDevice* device = nullptr;
    rhi::ICommandList* cmd = nullptr;
    const RenderFrameData* frameData = nullptr;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t frameIndex = 0;
};

/**
 * Mutable resource binding state for the current frame (descriptor rings, scratch).
 * Owned by RenderPipeline until dedicated PassResourceBuilders are introduced.
 */
struct PassResourceContext {
    // Intentionally empty; pipeline member rings remain the source of truth for now.
};

}  // namespace render
}  // namespace eyelib
