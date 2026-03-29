#include "eyelib/render/renderer/graph/RenderGraphOrchestrator.h"
#include "eyelib/render/framegraph/FrameGraph.h"

namespace eyelib {
namespace render {

RenderGraphOrchestrator::RenderGraphOrchestrator() {
    m_frameGraph = std::make_unique<FrameGraph>();
}

RenderGraphOrchestrator::~RenderGraphOrchestrator() = default;

FrameGraph* RenderGraphOrchestrator::GetFrameGraph() {
    return m_frameGraph.get();
}

const FrameGraph* RenderGraphOrchestrator::GetFrameGraph() const {
    return m_frameGraph.get();
}

void RenderGraphOrchestrator::SetFrameSize(uint32_t width, uint32_t height) {
    if (m_frameGraph)
        m_frameGraph->SetFrameSize(width, height);
}

void RenderGraphOrchestrator::MarkNeedsCompile() {
    m_needsCompile = true;
}

bool RenderGraphOrchestrator::CompileIfNeeded(PipelineMode mode, bool stereo, uint32_t width, uint32_t height,
                                              rhi::IDevice* device, const std::function<void(FrameGraph&)>& buildGraph) {
    if (!m_frameGraph || !device)
        return false;
    const bool needsCompile = m_needsCompile || m_lastWidth != width || m_lastHeight != height || m_lastMode != mode
        || m_lastStereo != stereo;
    if (!needsCompile)
        return false;

    m_frameGraph->Clear();
    m_frameGraph->SetFrameSize(width, height);
    buildGraph(*m_frameGraph);
    m_frameGraph->Compile(device);
    m_lastWidth = width;
    m_lastHeight = height;
    m_lastMode = mode;
    m_lastStereo = stereo;
    m_needsCompile = false;
    return true;
}

void RenderGraphOrchestrator::Execute(const RenderContext& ctx) {
    if (m_frameGraph)
        m_frameGraph->Execute(ctx);
}

}  // namespace render
}  // namespace eyelib
