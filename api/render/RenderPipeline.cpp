#include "api/render/RenderPipeline.h"
#include "api/render/RenderTarget.h"
#include "eyelib/render/RenderPipeline.h"
#include "eyelib/render/rhi/OpenGL/DeviceGL.h"

namespace eyelib {
namespace api {

class RenderPipeline::Impl {
public:
    std::unique_ptr<::eyelib::render::RenderPipeline> pipeline = std::make_unique<::eyelib::render::RenderPipeline>();
};

RenderPipeline::RenderPipeline() : m_impl(std::make_unique<Impl>()) {}

RenderPipeline::~RenderPipeline() = default;

RenderPipeline::RenderPipeline(RenderPipeline&& other) noexcept = default;

RenderPipeline& RenderPipeline::operator=(RenderPipeline&& other) noexcept = default;

bool RenderPipeline::Initialize(uint32_t width, uint32_t height) {
    auto device = std::make_unique<::eyelib::render::rhi::DeviceGL>();
    return m_impl->pipeline->Initialize(std::move(device), width, height);
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
    m_impl->pipeline->Render(scene);
}

void RenderPipeline::BlitToScreen() {
    m_impl->pipeline->BlitToScreen();
}

bool RenderPipeline::HasValidOutput() const {
    return m_impl->pipeline->HasValidOutput();
}

} // namespace api
} // namespace eyelib
