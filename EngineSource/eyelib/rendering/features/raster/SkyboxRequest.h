#pragma once

namespace eyelib {
namespace rendering {

namespace rhi { struct ITexture; }

/**
 * Per-frame sky source for `SkyboxFeature` (set by the public façade from
 * `api::rendering::RenderScene::skybox` before `RenderWorld::RunFrame` records).
 *
 * Field semantics:
 *  - `Kind::None`       — feature does not append a pass; HDR target stays unchanged
 *                         (lighting result alone fills the framebuffer; useful for
 *                         purely UI-style overlays / debug scenes).
 *  - `Kind::Procedural` — analytic gradient pass (no texture input). `cubemap` ignored.
 *  - `Kind::Cubemap`    — samples `cubemap` (must be non-null and GPU-bindable) at the
 *                         far-plane direction. Caller owns the texture; the feature
 *                         only borrows it for the frame and tracks pointer identity
 *                         in its descriptor-set cache.
 *
 * `brightness` is a non-negative HDR multiplier applied in both shader variants. The
 * feature clamps to >= 0 at the input boundary; the shader also clamps for safety.
 *
 * POD; safe to copy. Lifetime spans one frame.
 */
struct SkyboxRequest {
    enum class Kind {
        None        = 0,
        Procedural  = 1,
        Cubemap     = 2,
    };

    Kind            kind       = Kind::None;
    rhi::ITexture*  cubemap    = nullptr;
    float           brightness = 1.0f;
};

}  // namespace rendering
}  // namespace eyelib
