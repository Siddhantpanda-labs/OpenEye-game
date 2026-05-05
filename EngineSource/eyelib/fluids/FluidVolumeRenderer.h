#pragma once

#include "eyelib/fluids/FluidSimulation.h"
#include "eyelib/render/RenderTypes.h"
#include "eyelib/render/rhi/RHI.h"
#include "eyelib/render/resources/Shader.h"
#include "eyelib/render/resources/Texture2D.h"
#include "eyelib/render/resources/Texture3D.h"

#include <memory>

namespace eyelib {
namespace fluids {

/**
 * @brief GPU volume renderer for fluid density/temperature. Uses RHI only.
 */
class FluidVolumeRenderer {
public:
    FluidVolumeRenderer();
    ~FluidVolumeRenderer();

    /** Set device before Initialize. Call once at app/render startup. */
    void SetDevice(render::rhi::IDevice* device) { m_device = device; }
    bool Initialize();
    void Shutdown();

    /** Set viewport size for ray data targets (call before RenderVolume when size changes). */
    void SetViewport(unsigned int width, unsigned int height);

    void UpdateFromSimulation(const FluidSimulation& sim, float densityScale = 1.0f);

    void SetBackgroundTexture(const std::shared_ptr<render::Texture2D>& background);

    /** Render to the given RHI render target. */
    void RenderVolume(const FluidSimulation& sim,
                      const render::CameraView& camera,
                      const math::Mat4& model,
                      const math::Vec3& emissionPos01,
                      const math::Vec3& volumeSize,
                      render::rhi::IRenderTarget* target = nullptr);

private:
    void EnsureRayDataTargets();
    void EnsureShaders();
    void CreateCube();

    render::rhi::IDevice* m_device = nullptr;
    render::rhi::IBuffer* m_cubeVB = nullptr;
    render::rhi::IRenderTarget* m_rayDataFrontRT = nullptr;
    render::rhi::IRenderTarget* m_rayDataBackRT = nullptr;

    unsigned int m_viewportW = 0;
    unsigned int m_viewportH = 0;

    std::shared_ptr<render::Shader> m_shRayDataBack;
    std::shared_ptr<render::Shader> m_shRayDataFront;
    std::shared_ptr<render::Shader> m_shRaymarch;

    std::shared_ptr<render::Texture3D> m_densityTexture;
    std::shared_ptr<render::Texture3D> m_obstacleTexture;
    std::shared_ptr<render::Texture2D> m_backgroundTexture;

    math::IVec3 m_cachedResolution{0, 0, 0};
};

} // namespace fluids
} // namespace eyelib
