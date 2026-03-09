#include "api/fluids/FluidFoamRenderer.h"

#include "eyelib/fluids/FluidFoamRenderer.h"
#include "eyelib/render/RenderTypes.h"

namespace eyelib {
namespace api {

class FluidFoamRenderer::Impl {
public:
    fluids::FluidFoamRenderer renderer{};
};

static fluids::FluidFoamSettings ToInternal(const FluidFoamSettings& s) {
    fluids::FluidFoamSettings out;
    out.enabled = s.enabled;
    out.isoValue = s.isoValue;
    out.surfaceBand = s.surfaceBand;
    out.densityThreshold = s.densityThreshold;
    out.velocityThreshold = s.velocityThreshold;
    out.splashVelocityThreshold = s.splashVelocityThreshold;
    out.spawnProbability = s.spawnProbability;
    out.maxSpawnPerFrame = s.maxSpawnPerFrame;
    out.sampleStride = s.sampleStride;
    out.particleLifetime = s.particleLifetime;
    out.splashLifetime = s.splashLifetime;
    out.particleSize = s.particleSize;
    out.splashSize = s.splashSize;
    out.damping = s.damping;
    out.foamColor = s.foamColor;
    out.splashColor = s.splashColor;
    return out;
}

static FluidFoamSettings FromInternal(const fluids::FluidFoamSettings& s) {
    FluidFoamSettings out;
    out.enabled = s.enabled;
    out.isoValue = s.isoValue;
    out.surfaceBand = s.surfaceBand;
    out.densityThreshold = s.densityThreshold;
    out.velocityThreshold = s.velocityThreshold;
    out.splashVelocityThreshold = s.splashVelocityThreshold;
    out.spawnProbability = s.spawnProbability;
    out.maxSpawnPerFrame = s.maxSpawnPerFrame;
    out.sampleStride = s.sampleStride;
    out.particleLifetime = s.particleLifetime;
    out.splashLifetime = s.splashLifetime;
    out.particleSize = s.particleSize;
    out.splashSize = s.splashSize;
    out.damping = s.damping;
    out.foamColor = s.foamColor;
    out.splashColor = s.splashColor;
    return out;
}

FluidFoamRenderer::FluidFoamRenderer() : m_impl(new Impl()) {}
FluidFoamRenderer::~FluidFoamRenderer() = default;

void FluidFoamRenderer::SetSettings(const FluidFoamSettings& settings) {
    m_impl->renderer.SetSettings(ToInternal(settings));
}

FluidFoamSettings FluidFoamRenderer::GetSettings() const {
    return FromInternal(m_impl->renderer.GetSettings());
}

void FluidFoamRenderer::UpdateFromSimulation(const FluidSimulation& sim, float dt) {
    m_impl->renderer.UpdateFromSimulation(*sim.GetInternal(), dt);
}

std::shared_ptr<Mesh> FluidFoamRenderer::BuildMesh(RenderDevice device, const render::Camera& camera) {
    ::eyelib::render::CameraView view{};
    view.view = camera.GetViewMatrix();
    view.projection = camera.GetProjectionMatrix();
    view.viewPos = camera.GetPosition();
    view.nearPlane = 0.1f;
    view.farPlane = 1000.0f;
    auto* rhiDevice = static_cast<::eyelib::render::rhi::IDevice*>(device.Get());
    auto internalMesh = m_impl->renderer.BuildMesh(rhiDevice, view);
    return Mesh::WrapInternal(internalMesh);
}

} // namespace api
} // namespace eyelib
