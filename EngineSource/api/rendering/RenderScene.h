#pragma once

#include "api/rendering/CameraView.h"
#include "api/rendering/Light.h"
#include "eyelib/math/AABB.h"
#include "eyelib/math/Types.h"

#include <cstdint>
#include <vector>

namespace eyelib {
namespace api {
namespace rendering {
class Skybox;  // see api/rendering/Skybox.h
}
}
}

namespace eyelib {
namespace api {
namespace rendering {

/**
 * @brief Draw bucket / pass selection (mirrors `eyelib::rendering::DrawLayer` 1:1).
 *
 * Declared independently in the public API surface so that editor/player code never
 * names the internal type. Conversion happens inside the façade (`RenderPipeline.cpp`).
 */
enum class DrawLayer : uint8_t {
    Opaque = 0,
    AlphaTest = 1,
    Transparent = 2,
    Sky = 3,
};

/** Per-instance flags. Mirrors `eyelib::rendering::draw_item_flags` 1:1 (same bit layout). */
namespace draw_instance_flags {
inline constexpr uint32_t kNone        = 0u;
inline constexpr uint32_t kDoubleSided = 1u << 0;
inline constexpr uint32_t kCastShadow  = 1u << 1;
}  // namespace draw_instance_flags

/**
 * @brief One drawable instance produced by the editor / game thread.
 *
 * Field semantics:
 *  - `meshHandle`   — opaque uint32 returned by the M1.6 mesh façade (`Mesh::GetHandle`).
 *  - `materialId`   — opaque uint32 returned by the M1.6 material façade
 *                     (`MaterialInstance::GetMaterialId`).
 *  - `worldTransform` — model→world matrix (right-handed, column-major; same convention as
 *                       `eyelib::rendering::SceneDrawInstance`).
 *  - `localBounds`    — model-space AABB; the new module's `VisibilitySystem` transforms
 *                       this each frame for frustum culling.
 *  - `layer`          — draw bucket; controls pass routing (Opaque / AlphaTest / Transparent
 *                       / Sky). The façade also re-buckets by the public `RenderScene::opaque`
 *                       vs. `transparent` arrays at adapter time, so this field is currently
 *                       redundant for editor/player but kept for forward compat (alpha-test
 *                       lands when its pass is wired in the new module).
 *  - `flags`          — bitset (see `draw_instance_flags`).
 *
 * POD; safe to memcpy / store in arrays. No GPU-side ownership.
 */
struct DrawableInstance {
    uint32_t       meshHandle   = 0;
    uint32_t       materialId   = 0;
    math::Mat4     worldTransform{1.0f};
    math::AABB     localBounds{};
    DrawLayer      layer        = DrawLayer::Opaque;
    uint32_t       flags        = draw_instance_flags::kNone;
};

/**
 * @brief Per-frame scene snapshot consumed by `RenderPipeline::Render`.
 *
 * Filled on the game thread (or editor UI thread for the editor scene-view); read-only
 * on the render thread. Owns its arrays via `std::vector` for v1; a frame-allocator
 * variant can be added later without changing this struct's shape.
 *
 * Lights (M4.0a):
 *  - One `DirectionalLight` slot (sun / key light).
 *  - `pointLights` and `spotLights` arrays (capped at 8 each by the new module's
 *    `SceneLighting::kMaxScenePointLights`; extras are silently dropped per the legacy
 *    cap).
 *  - The `RenderPipeline::Render` adapter calls `SceneLighting::ClearPointLights /
 *    ClearSpotLights / SetDirectionalLight` then re-populates from these vectors per
 *    frame, so each `Render` is the source of truth for the next frame's lighting.
 *  - `directionalLightActive` distinguishes "no directional in scene" (false) from
 *    "directional zeroed-out" (true with intensity 0). When false, the new module's
 *    default directional state from `SceneLighting` is left untouched (NOT cleared) —
 *    matches legacy behaviour where missing directional kept the previous frame's value.
 *
 * **Skybox** (visual sky overlay) lands here as a borrowed pointer to a caller-owned
 * `Skybox` handle. `nullptr` (the default) means "no sky overlay" — the new module
 * renders against the default `IblDefaultProbes` neutral environment only. When set,
 * the public façade routes the handle into the internal `SkyboxFeature` which appends a
 * fullscreen pass after deferred lighting and before editor debug overlays. The
 * `Skybox` instance MUST outlive any `Render` call that references it; editor / player
 * typically keep one alive for the lifetime of the active scene.
 *
 * **IBL probe swap (re-baking irradiance + prefilter from the user-supplied cubemap and
 * republishing on `IblIrradiance / Prefilter` blackboard slots) is the documented
 * follow-up** in `documentation/Render_Module/Render_steps_todo.md`. Today the IBL
 * source remains the bootstrap default; only the visual sky is per-scene.
 *
 * **Per-instance bones (skinning) is intentionally NOT in v1.** The new module's
 * `SceneDrawInstance` has no bones field, so per-draw skinning matrices cannot cross
 * the contract. Filed in `Render_steps_todo.md` Section C — "per-draw skinning matrices
 * in SceneDrawInstance". Skinned meshes render with bind pose only on the new path.
 */
struct RenderScene {
    CameraView                    camera{};
    std::vector<DrawableInstance> opaque;
    std::vector<DrawableInstance> transparent;

    /** Sun / key light. Active iff `directionalLightActive` is true. */
    DirectionalLight              directionalLight{};
    /** False = adapter does NOT touch the new module's directional slot (preserves previous). */
    bool                          directionalLightActive = false;

    /** Point lights this frame (capped to `SceneLighting::kMaxScenePointLights = 8`; extras dropped). */
    std::vector<PointLight>       pointLights;
    /** Spot lights this frame (capped to 16 by the new module's internal limit). */
    std::vector<SpotLight>        spotLights;

    /** Optional sky source for this frame (borrowed; caller owns the `Skybox`). `nullptr`
     *  = no sky overlay (lighting result alone fills the framebuffer). See class docstring. */
    const Skybox*                 skybox = nullptr;
    /** HDR multiplier for the sky (clamped non-negative inside the feature). 1.0 = unchanged. */
    float                         skyboxBrightness = 1.0f;
};

} // namespace rendering
} // namespace api
} // namespace eyelib
