#pragma once

#include "api/rendering/RenderDevice.h"

#include <array>
#include <cstdint>
#include <memory>
#include <string>

namespace eyelib {
namespace api {
namespace rendering {

/**
 * @brief Public PIMPL handle to a per-scene sky source for `RenderPipeline::Render`.
 *
 * Three kinds:
 *  - `Kind::None`       — sky is disabled; the new module renders against the default
 *                         IBL probes only (no sky overlay drawn).
 *  - `Kind::Procedural` — analytic gradient sky pass (no texture input). Cheap; useful
 *                         as a sensible default before any asset is loaded.
 *  - `Kind::Cubemap`    — samples a 6-face GPU cubemap. Authored via:
 *                          - `LoadCrossLayout(...)` — single PNG / JPG with horizontal-
 *                            cross face packing (engine asset convention; see
 *                            `eyelib::world::skybox::kDefaultEveningCrossPath`).
 *                          - `LoadSixFaces(...)`    — six discrete face images
 *                            (`+X, -X, +Y, -Y, +Z, -Z`), matching the legacy directory
 *                            convention (`posx.jpg`, `negx.jpg`, …).
 *
 * Ownership: this class **owns** the GPU cubemap. The pipeline only borrows a raw pointer
 * for one frame at a time. Keep the `Skybox` alive at least until the next
 * `RenderPipeline::Render(scene)` call has completed. Editor / player typically hold one
 * per scene (paired with the world's `skyboxAssetId`).
 *
 * Thread safety: not thread-safe. Construct + load on the render thread (same thread that
 * owns `RenderDevice`). Read-only access from `RenderScene::skybox` is safe per frame.
 *
 * **IBL probe coverage:** `Kind::Cubemap` drives both the visual sky **and** the lighting
 * pass's IBL term. The internal `SkyboxFeature::OnUpload` invokes
 * `IBL::GenerateIrradianceAndPrefilter` once per unique source cubemap pointer (re-baked
 * automatically when the asset id changes), then publishes the baked textures onto the
 * `IblIrradianceCubemap` / `IblPrefilterCubemap` blackboard slots. `Kind::Procedural` /
 * `Kind::None` fall back to `RasterFeature::GetDefaultEnvCubemap()` (the bootstrap neutral
 * env) so deferred lighting always has a valid sample. The BRDF LUT slot is invariant per
 * scene and continues to be published by `RasterFeature` (`IblDefaultProbes`).
 */
class Skybox {
public:
    enum class Kind : uint8_t {
        None       = 0,
        Procedural = 1,
        Cubemap    = 2,
    };

    Skybox();
    ~Skybox();

    Skybox(const Skybox&) = delete;
    Skybox& operator=(const Skybox&) = delete;

    Skybox(Skybox&& other) noexcept;
    Skybox& operator=(Skybox&& other) noexcept;

    /** Load a 6-face cubemap from a single horizontal-cross image.
     *  Returns false on disk-load or GPU-upload failure (caller can inspect `GetKind()` —
     *  unchanged on failure). `faceSize` is the per-face pixel size of the cross layout
     *  (e.g. 512 for a 2048×1536 cross). `isSRGB` enables sRGB → linear at sample time. */
    bool LoadCrossLayout(RenderDevice device, const std::string& path,
                         uint32_t faceSize = 512, bool isSRGB = true, bool generateMipmaps = true);

    /** Load a 6-face cubemap from six discrete images. Face order: `+X, -X, +Y, -Y, +Z, -Z`. */
    bool LoadSixFaces(RenderDevice device, const std::array<std::string, 6>& faces,
                      bool isSRGB = true, bool generateMipmaps = true);

    /** Switch to procedural sky. Frees any cubemap GPU resources previously held. */
    void SetProcedural() noexcept;

    /** Switch to no sky. Frees any cubemap GPU resources previously held. */
    void Reset() noexcept;

    Kind GetKind() const noexcept;
    /** Per-face pixel size of the cubemap (0 when no cubemap is loaded). */
    int  GetSize() const noexcept;
    /** True iff `kind == Cubemap` and the GPU cubemap was successfully uploaded. */
    bool IsCubemapValid() const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;

    friend class SkyboxAccess;  // façade-private accessor (api/rendering/internal/*).
};

} // namespace rendering
} // namespace api
} // namespace eyelib
