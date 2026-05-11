#pragma once

namespace eyelib {
namespace api {
namespace rendering {

/**
 * @brief Opaque offscreen render target (color attachments + optional depth).
 *
 * Forward-declared placeholder for now. Full implementation lands when the editor/player
 * surfaces that need it migrate (XR eye buffers — M4.6; streaming RTs — M4.5; material
 * preview offscreen target if it stops sharing the scene-view pipeline — M3.1). Keeping
 * the type nameable here lets façade headers (`RenderPipeline.h` etc.) declare
 * `class RenderTarget;` and accept references without dragging in implementation details.
 *
 * Ownership (when implemented): the façade creates targets via the device's RHI;
 * no change inside `eyelib/rendering/` is required for a typical offscreen color/depth pair.
 */
class RenderTarget;

} // namespace rendering
} // namespace api
} // namespace eyelib
