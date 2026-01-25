#pragma once

#include "api/fluids/FluidSimulation.h"
#include "api/render/Camera.h"
#include "api/render/Texture.h"

namespace eyelib {
namespace api {

/**
 * @brief API wrapper for GPU fluid volume renderer.
 */
class FluidVolumeRenderer {
public:
    FluidVolumeRenderer();
    ~FluidVolumeRenderer();

    bool Initialize();
    void Shutdown();

    void UpdateFromSimulation(const FluidSimulation& sim, float densityScale = 1.0f);
    void SetBackgroundTexture(const Texture& texture);

    void RenderVolume(const FluidSimulation& sim,
                      const Camera& camera,
                      const math::Mat4& model,
                      const math::Vec3& emissionPos01,
                      const math::Vec3& volumeSize);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace api
} // namespace eyelib
