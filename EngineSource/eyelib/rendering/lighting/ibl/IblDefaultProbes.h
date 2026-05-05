#pragma once

#include "eyelib/rendering/lighting/ibl/IBL.h"

#include <memory>

namespace eyelib {
namespace rendering {

namespace rhi {
struct IDevice;
struct ITexture;
}

/**
 * Owns bootstrap IBL GPU textures (CPU-generated neutral env + BRDF LUT).
 * Separate from `RenderResources` (frame RTs/UBOs) and from the GPU bake path in `IBL`.
 * `RasterFeature` publishes these on `Ibl*` blackboard slots under `writer_names::kLightingIbl`.
 * BRDF LUT policy: try asset-backed raw RGBA16F LUT first, then GPU one-shot fallback (`IBL::Initialize`),
 * then bounded CPU fallback as last resort.
 */
class IblDefaultProbes {
public:
    IblDefaultProbes() = default;
    ~IblDefaultProbes();

    void Ensure(rhi::IDevice* device);
    void Shutdown(rhi::IDevice* device) noexcept;

    /** Shared cubemap for irradiance + single-mip prefilter until a real prefiltered env exists. */
    rhi::ITexture* EnvCubemap() noexcept { return m_envCubemap; }
    const rhi::ITexture* EnvCubemap() const noexcept { return m_envCubemap; }
    rhi::ITexture* BrdfLut() noexcept { return m_brdfLut; }
    const rhi::ITexture* BrdfLut() const noexcept { return m_brdfLut; }

private:
    rhi::ITexture* m_envCubemap = nullptr;
    rhi::ITexture* m_brdfLut = nullptr;
    bool m_brdfOwnedByIbl = false;
    std::unique_ptr<IBL> m_gpuFallbackIbl;
};

} // namespace rendering
} // namespace eyelib
