#include "eyelib/rendering/scheduler/FrameScheduler.h"
#include "eyelib/rendering/framegraph/FrameGraph.h"
#include "eyelib/rendering/framegraph/FrameGraphBlackboard.h"
#include "eyelib/rendering/scheduler/RenderFeature.h"
#include "eyelib/rendering/scheduler/RenderFeatureRegistry.h"

namespace eyelib {
namespace rendering {

FrameScheduler::FrameScheduler(RenderFeatureRegistry& registry) noexcept : m_registry(&registry) {}

void FrameScheduler::Schedule(
    FrameContext& frame,
    RenderContext& render,
    const RenderCapabilities& caps,
    FrameGraph& graph,
    FrameGraphBlackboard& blackboard) {
    (void)caps;  // Reserved: future per-frame capability overrides (should stay rare; see doc §5).
    // Blackboard is cleared and populated in `RenderWorld::UploadPhase` for this frame; do not clear here
    // or core publishes (e.g. MainHdr) are lost before `OnSchedulePasses`.
    graph.Clear();

    if (!m_registry)
        return;

    for (IRenderFeature* f : m_registry->OrderedFeatures()) {
        if (f)
            f->OnFramePrepare(frame, render);
    }
    for (IRenderFeature* f : m_registry->OrderedFeatures()) {
        if (f)
            f->OnSchedulePasses(graph, blackboard, render);
    }
}

}  // namespace rendering
}  // namespace eyelib
