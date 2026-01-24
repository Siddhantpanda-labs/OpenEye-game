#include "api/render/RenderPipeline.h"
#include "eyelib/render/RenderPipeline.h"
#include "eyelib/render/RenderTypes.h"
#include "eyelib/render/framegraph/FrameGraph.h"

namespace eyelib {
namespace api {

class RenderPipeline::Impl {
public:
    std::unique_ptr<render::RenderPipeline> pipeline = std::make_unique<render::RenderPipeline>();
};

RenderPipeline::RenderPipeline() : m_impl(std::make_unique<Impl>()) {}

RenderPipeline::~RenderPipeline() = default;

RenderPipeline::RenderPipeline(RenderPipeline&& other) noexcept = default;

RenderPipeline& RenderPipeline::operator=(RenderPipeline&& other) noexcept = default;

bool RenderPipeline::Initialize(uint32_t width, uint32_t height) {
    return m_impl->pipeline->Initialize(width, height);
}

void RenderPipeline::Resize(uint32_t width, uint32_t height) {
    m_impl->pipeline->Resize(width, height);
}

void RenderPipeline::Shutdown() {
    m_impl->pipeline->Shutdown();
}

void RenderPipeline::SetMode(Mode mode) {
    render::PipelineMode m = (mode == Mode::Forward) ? render::PipelineMode::Forward :
                             (mode == Mode::Deferred) ? render::PipelineMode::Deferred :
                             render::PipelineMode::ForwardPlus;
    m_impl->pipeline->SetMode(m);
}

RenderPipeline::Mode RenderPipeline::GetMode() const {
    render::PipelineMode m = m_impl->pipeline->GetMode();
    return (m == render::PipelineMode::Forward) ? Mode::Forward :
           (m == render::PipelineMode::Deferred) ? Mode::Deferred :
           Mode::ForwardPlus;
}

void RenderPipeline::Render(/* RenderScene scene */) {
    // TODO: Convert API RenderScene to internal RenderScene
    // render::RenderScene scene;
    // m_impl->pipeline->Render(scene);
}

} // namespace api
} // namespace eyelib
