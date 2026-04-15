#pragma once

#include "eyelib/rendering/core/FrameContext.h"
#include "eyelib/rendering/core/RenderCapabilities.h"
#include "eyelib/rendering/core/RenderContext.h"
#include "eyelib/rendering/core/GpuProfiler.h"
#include "eyelib/rendering/core/RenderResources.h"
#include "eyelib/rendering/debug/RenderStats.h"
#include "eyelib/rendering/framegraph/BlackboardWriterRegistry.h"
#include "eyelib/rendering/framegraph/FrameGraph.h"
#include "eyelib/rendering/framegraph/FrameGraphBlackboard.h"
#include "eyelib/rendering/scheduler/FrameScheduler.h"
#include "eyelib/rendering/scheduler/RenderFeatureRegistry.h"
#include "eyelib/rendering/scene/DrawList.h"
#include "eyelib/rendering/visibility/VisibilitySystem.h"

namespace eyelib {
namespace rendering {

class IRenderSceneView;

namespace rhi {
struct IDevice;
struct ICommandList;
}

/**
 * Owns frame-level orchestration: capabilities, feature registry, frame graph, scheduler (doc core/).
 * Does not own the RHI device or the scene view.
 */
class RenderWorld {
public:
    explicit RenderWorld(rhi::IDevice* device) noexcept;

    /** Set output resolution used for `RenderResources` allocation (call before `Initialize` or anytime before `RunFrame`). */
    void SetOutputSize(uint32_t width, uint32_t height) noexcept;

    /** Optional read-only scene; forwarded on `RenderContext` each `RunFrame`. */
    void SetSceneView(const IRenderSceneView* sceneView) noexcept { m_sceneView = sceneView; }

    bool IsInitialized() const noexcept { return m_initialized; }

    /**
     * Subscribe a feature (non-owning). If `Initialize` was already called, runs `OnRegister` and
     * `OnRegisterBlackboardWriters` immediately for this feature. If not initialized, only queues it;
     * `Initialize` will invoke registration for all features in order.
     * @return false if null or duplicate pointer.
     */
    bool TrySubscribeFeature(IRenderFeature* feature);

    /**
     * Unsubscribe a feature. Before `Initialize`: removes from queue only (no callbacks).
     * After `Initialize`: calls `OnUnregisterBlackboardWriters`, `OnShutdown`, then removes. Features must
     * `TryRelease` every slot they successfully `TryReserve`'d.
     * @return false if null or not subscribed.
     */
    bool TryUnsubscribeFeature(IRenderFeature* feature);

    void Initialize();
    void Shutdown();

    /**
     * @param recordingCommandList Optional; if null, graph compile still runs but Execute is a no-op.
     */
    void RunFrame(float deltaSeconds, rhi::ICommandList* recordingCommandList);

    FrameGraph& Graph() noexcept { return m_graph; }
    FrameGraphBlackboard& Blackboard() noexcept { return m_blackboard; }
    RenderFeatureRegistry& Features() noexcept { return m_features; }
    const RenderCapabilities& Caps() const noexcept { return m_caps; }
    RenderResources& Resources() noexcept { return m_resources; }
    const GpuProfiler& Profiler() const noexcept { return m_gpuProfiler; }
    const RenderStats& Stats() const noexcept { return m_stats; }

private:
    void UploadPhase(FrameContext& frame, RenderContext& render, rhi::ICommandList* recordingCommandList);

    rhi::IDevice* m_device = nullptr;
    const IRenderSceneView* m_sceneView = nullptr;
    bool m_initialized = false;
    RenderCapabilities m_caps{};
    FrameContext m_frame{};
    RenderContext m_render{};
    BlackboardWriterRegistry m_blackboardWriters;
    DrawList m_drawList;
    VisibilitySystem m_visibility;
    RenderResources m_resources;
    GpuProfiler m_gpuProfiler;
    RenderStats m_stats;
    RenderFeatureRegistry m_features;
    FrameGraph m_graph;
    FrameGraphBlackboard m_blackboard;
    FrameScheduler m_scheduler;
};

}  // namespace rendering
}  // namespace eyelib
