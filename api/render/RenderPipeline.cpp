#include "api/render/RenderPipeline.h"
#include "api/render/RenderTarget.h"
#include "eyelib/render/RenderPipeline.h"
#include "eyelib/render/RenderTypes.h"
#include "eyelib/render/framegraph/FrameGraph.h"

namespace eyelib {
namespace api {

namespace {
::eyelib::render::Drawable ToInternal(const Drawable& d) {
    ::eyelib::render::Drawable out;
    out.mesh = d.mesh ? d.mesh->GetInternal() : nullptr;
    out.material = d.material ? d.material->GetInternal() : nullptr;
    out.materialInstance = d.materialInstance ? d.materialInstance->GetInternal() : nullptr;
    out.transform = d.transform;
    return out;
}

::eyelib::render::RenderScene ToInternal(const RenderScene& scene) {
    ::eyelib::render::RenderScene out;
    out.opaque.reserve(scene.opaque.size());
    for (const auto& d : scene.opaque) {
        out.opaque.push_back(ToInternal(d));
    }
    out.transparent.reserve(scene.transparent.size());
    for (const auto& d : scene.transparent) {
        out.transparent.push_back(ToInternal(d));
    }

    out.camera.view = scene.camera.view;
    out.camera.projection = scene.camera.projection;
    out.camera.viewPos = scene.camera.viewPos;
    out.camera.nearPlane = scene.camera.nearPlane;
    out.camera.farPlane = scene.camera.farPlane;

    out.light.direction = scene.light.direction;
    out.light.color = scene.light.color;
    out.light.intensity = scene.light.intensity;
    out.light.ambient = scene.light.ambient;
    out.light.exposure = scene.light.exposure;
    out.light.castShadows = scene.light.castShadows;
    out.skybox = scene.skybox ? scene.skybox->GetInternal() : nullptr;

    out.pointLights.reserve(scene.pointLights.size());
    for (const auto& p : scene.pointLights) {
        ::eyelib::render::PointLight lp;
        lp.position = p.position;
        lp.color = p.color;
        lp.intensity = p.intensity;
        lp.radius = p.radius;
        out.pointLights.push_back(lp);
    }

    out.spotLights.reserve(scene.spotLights.size());
    for (const auto& s : scene.spotLights) {
        ::eyelib::render::SpotLight ls;
        ls.position = s.position;
        ls.direction = s.direction;
        ls.color = s.color;
        ls.intensity = s.intensity;
        ls.range = s.range;
        ls.innerConeCos = s.innerConeCos;
        ls.outerConeCos = s.outerConeCos;
        out.spotLights.push_back(ls);
    }

    return out;
}
} // namespace

class RenderPipeline::Impl {
public:
    std::unique_ptr<::eyelib::render::RenderPipeline> pipeline = std::make_unique<::eyelib::render::RenderPipeline>();
};

RenderPipeline::RenderPipeline() : m_impl(std::make_unique<Impl>()) {}

RenderPipeline::~RenderPipeline() = default;

RenderPipeline::RenderPipeline(RenderPipeline&& other) noexcept = default;

RenderPipeline& RenderPipeline::operator=(RenderPipeline&& other) noexcept = default;

bool RenderPipeline::Initialize(uint32_t width, uint32_t height) {
    return m_impl->pipeline->Initialize(width, height);
}

bool RenderPipeline::Initialize(const RenderTarget& target) {
    return Initialize(target.GetWidth(), target.GetHeight());
}

void RenderPipeline::Resize(uint32_t width, uint32_t height) {
    m_impl->pipeline->Resize(width, height);
}

void RenderPipeline::Resize(const RenderTarget& target) {
    Resize(target.GetWidth(), target.GetHeight());
}

void RenderPipeline::Shutdown() {
    m_impl->pipeline->Shutdown();
}

void RenderPipeline::SetMode(Mode mode) {
    ::eyelib::render::PipelineMode m = (mode == Mode::Forward) ? ::eyelib::render::PipelineMode::Forward :
                             (mode == Mode::Deferred) ? ::eyelib::render::PipelineMode::Deferred :
                             ::eyelib::render::PipelineMode::ForwardPlus;
    m_impl->pipeline->SetMode(m);
}

RenderPipeline::Mode RenderPipeline::GetMode() const {
    ::eyelib::render::PipelineMode m = m_impl->pipeline->GetMode();
    return (m == ::eyelib::render::PipelineMode::Forward) ? Mode::Forward :
           (m == ::eyelib::render::PipelineMode::Deferred) ? Mode::Deferred :
           Mode::ForwardPlus;
}

void RenderPipeline::Render(const RenderScene& scene) {
    m_impl->pipeline->Render(ToInternal(scene));
}

void RenderPipeline::BlitToScreen() {
    m_impl->pipeline->BlitToScreen();
}

} // namespace api
} // namespace eyelib
