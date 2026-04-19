#pragma once

#include <cstdint>

namespace eyelib {
namespace rendering {

/** Typed blackboard slots (extend as the module grows). */
enum class BlackboardSlot : uint8_t {
    /** Main HDR color render target owned by `RenderResources`; uses `MainDepth` as its shared depth attachment. */
    MainHdrRenderTarget = 0,
    /** Standalone depth texture shared by `MainHdrRenderTarget` and any depth-only pass (e.g. depth prepass). */
    MainDepth,
    /** Per-frame uniform buffer (time, frame index, global constants). Null until created (A.5). */
    PerFrameUniformBuffer,
    /** Per-view uniform buffer (camera matrices, viewport, jitter). Null until created (A.5). */
    PerViewUniformBuffer,
    /** Per-draw instance SSBO (transforms + material ids), std430. Filled in upload phase (B.4). */
    PerObjectStorageBuffer,
    /** Single directional light UBO (std140, 256 bytes). Filled in upload phase (B.6). */
    DirectionalLightUniformBuffer,
    SlotCount
};

}  // namespace rendering
}  // namespace eyelib
