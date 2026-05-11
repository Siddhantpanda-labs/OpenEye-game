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
 * **Skybox / IBL is intentionally NOT in v1.** The new module's `RasterFeature` consumes
 * `IblDefaultProbes` as the only IBL source today; per-scene skybox swap requires an
 * upstream architectural change in `eyelib/rendering/` (filed in `Render_steps_todo.md`
 * Section C — "per-scene skybox / IBL probe swap"). Editor sky panel + player skybox
 * loader remain on the legacy stack until that lands. See M4 audit table.
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
};

} // namespace rendering
} // namespace api
} // namespace eyelib
