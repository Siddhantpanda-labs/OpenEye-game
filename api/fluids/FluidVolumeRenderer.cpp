#include "api/fluids/FluidVolumeRenderer.h"

#include "eyelib/fluids/FluidVolumeRenderer.h"
#include "eyelib/render/RenderTypes.h"

namespace eyelib {
namespace api {

class FluidVolumeRenderer::Impl {
public:
    eyelib::fluids::FluidVolumeRenderer renderer{};
};

FluidVolumeRenderer::FluidVolumeRenderer() : m_impl(new Impl()) {}
FluidVolumeRenderer::~FluidVolumeRenderer() = default;

bool FluidVolumeRenderer::Initialize() {
    return m_impl->renderer.Initialize();
}

void FluidVolumeRenderer::Shutdown() {
    m_impl->renderer.Shutdown();
}

void FluidVolumeRenderer::UpdateFromSimulation(const FluidSimulation& sim, float densityScale) {
    m_impl->renderer.UpdateFromSimulation(*sim.GetInternal(), densityScale);
}

void FluidVolumeRenderer::SetBackgroundTexture(const Texture& texture) {
    m_impl->renderer.SetBackgroundTexture(texture.GetInternal());
}

void FluidVolumeRenderer::RenderVolume(const FluidSimulation& sim,
                                       const render::Camera& camera,
                                       const math::Mat4& model,
                                       const math::Vec3& emissionPos01,
                                       const math::Vec3& volumeSize) {
    ::eyelib::render::CameraView view{};
    view.view = camera.GetViewMatrix();
    view.projection = camera.GetProjectionMatrix();
    view.viewPos = camera.GetPosition();
    view.nearPlane = 0.1f;
    view.farPlane = 1000.0f;
    m_impl->renderer.RenderVolume(*sim.GetInternal(), view, model, emissionPos01, volumeSize);
}

} // namespace api
} // namespace eyelib
