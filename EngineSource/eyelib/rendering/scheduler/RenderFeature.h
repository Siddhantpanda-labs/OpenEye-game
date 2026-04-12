#pragma once

#include "eyelib/rendering/core/FrameContext.h"
#include "eyelib/rendering/core/RenderCapabilities.h"
#include "eyelib/rendering/core/RenderContext.h"
#include "eyelib/rendering/framegraph/BlackboardWriterRegistry.h"

namespace eyelib {
namespace rendering {

class FrameGraph;
class FrameGraphBlackboard;

/**
 * Pluggable render feature contract (doc §2).
 * Default methods are no-ops so features can be introduced incrementally.
 */
class IRenderFeature {
public:
    virtual ~IRenderFeature() = default;

    /** Override for logs, asserts, and tooling (stable per type). */
    virtual const char* FeatureDebugName() const noexcept { return "RenderFeature"; }

    virtual void OnRegister(const RenderCapabilities& caps) { (void)caps; }

    /** Reserve blackboard slots this feature writes to (doc §3). Called after core reserves system slots. */
    virtual void OnRegisterBlackboardWriters(BlackboardWriterRegistry& registry, const RenderCapabilities& caps) {
        (void)registry;
        (void)caps;
    }

    /**
     * Release slots reserved in `OnRegisterBlackboardWriters` (mirror hook). Called on runtime unsubscribe
     * and before `OnShutdown` during full `RenderWorld::Shutdown`.
     */
    virtual void OnUnregisterBlackboardWriters(BlackboardWriterRegistry& registry) { (void)registry; }

    /** CPU/GPU upload work before pass scheduling (doc §8 — staging, scene buffers). */
    virtual void OnUpload(FrameContext& frame, const RenderContext& render) {
        (void)frame;
        (void)render;
    }

    virtual void OnFramePrepare(FrameContext& frame, const RenderContext& render) {
        (void)frame;
        (void)render;
    }

    virtual void OnSchedulePasses(FrameGraph& graph, FrameGraphBlackboard& blackboard, const RenderContext& render) {
        (void)graph;
        (void)blackboard;
        (void)render;
    }

    virtual void OnShutdown() {}
};

}  // namespace rendering
}  // namespace eyelib
