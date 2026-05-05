#pragma once

namespace eyelib {
namespace rendering {

class FrameGraph;
class FrameGraphBlackboard;
class RenderFeatureRegistry;
struct FrameContext;
struct RenderContext;
struct RenderCapabilities;

/** Runs feature prepare + pass scheduling into the frame graph (doc scheduler role). Does not clear the blackboard. */
class FrameScheduler {
public:
    explicit FrameScheduler(RenderFeatureRegistry& registry) noexcept;

    void Schedule(
        FrameContext& frame,
        RenderContext& render,
        const RenderCapabilities& caps,
        FrameGraph& graph,
        FrameGraphBlackboard& blackboard);

private:
    RenderFeatureRegistry* m_registry = nullptr;
};

}  // namespace rendering
}  // namespace eyelib
