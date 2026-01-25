#pragma once

#include "eyelib/fluids/FluidSimulation.h"
#include "eyelib/render/RenderTypes.h"
#include "eyelib/render/resources/Shader.h"
#include "eyelib/render/resources/Texture2D.h"
#include "eyelib/render/resources/Texture3D.h"

#include <memory>

namespace eyelib {
namespace fluids {

/**
 * @brief GPU volume renderer for fluid density/temperature.
 */
class FluidVolumeRenderer {
public:
    FluidVolumeRenderer();
    ~FluidVolumeRenderer();

    bool Initialize();
    void Shutdown();

    void UpdateFromSimulation(const FluidSimulation& sim, float densityScale = 1.0f);

    void SetBackgroundTexture(const std::shared_ptr<render::Texture2D>& background);

    void RenderVolume(const FluidSimulation& sim,
                      const render::CameraView& camera,
                      const math::Mat4& model,
                      const math::Vec3& emissionPos01,
                      const math::Vec3& volumeSize,
                      unsigned int targetFbo = 0);

private:
    void EnsureRayDataTargets();
    void EnsureShaders();
    void CreateCube();

    // GPU resources
    unsigned int m_cubeVAO = 0;
    unsigned int m_cubeVBO = 0;
    unsigned int m_rayDataFrontTex = 0;
    unsigned int m_rayDataBackTex = 0;
    unsigned int m_rayDataFrontFBO = 0;
    unsigned int m_rayDataBackFBO = 0;

    int m_viewportW = 0;
    int m_viewportH = 0;

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
