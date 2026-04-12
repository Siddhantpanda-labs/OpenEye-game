#include "eyelib/rendering/framegraph/FrameGraph.h"
#include "eyelib/rendering/framegraph/FrameGraphBarrierList.h"
#include "eyelib/rendering/framegraph/FrameGraphBlackboard.h"
#include "eyelib/core/Log.h"
#include "eyelib/rendering/rhi/RHI.h"

namespace eyelib {
namespace rendering {

void FrameGraph::Clear() noexcept {
    m_passes.clear();
}

void FrameGraph::AddPass(std::unique_ptr<IFrameGraphPass> pass) {
    if (pass)
        m_passes.push_back(std::move(pass));
}

void FrameGraph::Compile(rhi::IDevice* device) {
    (void)device;
    for (uint32_t i = 0; i < m_passes.size(); ++i) {
        if (m_passes[i])
            m_passes[i]->OnCompiled(i);
    }
#if !defined(NDEBUG)
    if (!m_passes.empty()) {
        core::Log::DebugF("FrameGraph::Compile: %u passes", static_cast<unsigned>(m_passes.size()));
    }
#endif
}

void FrameGraph::Execute(rhi::IDevice* device, rhi::ICommandList* cmd, const FrameGraphBlackboard* blackboard) {
    if (!cmd)
        return;

    for (auto& p : m_passes) {
        if (!p)
            continue;

        FrameGraphBarrierList barriers;
        if (blackboard)
            p->CollectTextureBarriers(*blackboard, barriers);

        if (device && device->GetBarrierTracker()) {
            for (const auto& e : barriers.entries) {
                if (e.texture)
                    device->TransitionTextureToState(e.texture, e.toState);
            }
            device->FlushBarrierTracker(cmd);
        }

        p->Execute(cmd);
    }
}

}  // namespace rendering
}  // namespace eyelib
