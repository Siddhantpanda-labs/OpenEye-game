#pragma once

#include "eyelib/rendering/features/editor/DebugDrawPass.h"
#include "eyelib/rendering/features/editor/DebugLine.h"
#include "eyelib/rendering/framegraph/FrameGraphResourceNames.h"
#include "eyelib/rendering/scheduler/RenderFeature.h"
#include "eyelib/rendering/shaders/ShaderLibrary.h"

#include <memory>
#include <vector>

namespace eyelib {
namespace rendering {

namespace rhi {
struct IDevice;
struct IPipeline;
}

/**
 * Editor-only render feature (`features/editor`) hosting the debug-line overlay and, in
 * future iterations, the entity-ID picking pass.
 *
 * Opt-in: the public `api/rendering::RenderPipeline` only registers this feature when a
 * caller activates the debug-line surface (`SetPendingDebugLinesColored` / `RenderDebugLines`).
 * Once registered, the feature has truly zero per-frame cost when its line source is empty —
 * `OnSchedulePasses` skips adding any pass to the frame graph in that case.
 *
 * Architectural placement (P9 "editor as a feature"):
 *  - All feature lifecycle (`OnRegister`, `OnUpload`, `OnSchedulePasses`, `OnShutdown`) lives here.
 *  - The actual GPU work is in `DebugDrawPass` (kept under `features/editor/` per discipline:
 *    feature-specific passes live with their feature, not under `framegraph/passes/`).
 *  - The feature consumes only — it does not own any blackboard slot today. Line vertex
 *    storage is a feature-private host-visible buffer inside the pass.
 *  - Scheduling order +100 (runs after `RasterFeature` at 0) so the overlay is composited on
 *    top of the deferred-lit HDR target rather than before it.
 */
class EditorFeature final : public IRenderFeature {
public:
    EditorFeature() = default;
    ~EditorFeature() override;

    const char* FeatureDebugName() const noexcept override { return writer_names::kEditorFeature; }
    int SchedulingOrder() const noexcept override { return 100; }

    void OnRegister(const RenderCapabilities& caps) override;
    void OnUpload(FrameContext& frame, const RenderContext& render) override;
    void OnSchedulePasses(FrameGraph& graph, FrameGraphBlackboard& blackboard, const RenderContext& render) override;
    void OnShutdown() override;

    /**
     * Replace the feature's per-frame line source with `lines`. Caller's storage is copied
     * into a feature-owned buffer so it may be released before pass execution.
     * Pass an empty vector to disable the overlay for the upcoming frame.
     */
    void SetLines(const std::vector<DebugLine>& lines);

    /**
     * Drop the current line source without releasing the vector capacity. Intended for the
     * api/rendering adapter to call after each `Render` so the legacy single-frame contract
     * holds (callers that stop feeding lines do not see stale geometry).
     */
    void ClearLines() noexcept { m_pendingLines.clear(); }

    /** Returns true while the feature has any lines to draw on the current frame. */
    bool HasPendingLines() const noexcept { return !m_pendingLines.empty(); }

private:
    static rhi::ShaderGraphicsPipelineCreateInfo MakeDebugLinesSpec();

    shaders::ShaderLibrary m_shaderLibrary;
    rhi::IDevice* m_pipelineDevice = nullptr;
    std::unique_ptr<DebugDrawPass> m_debugDrawPass;
    std::vector<DebugLine> m_pendingLines;
};

}  // namespace rendering
}  // namespace eyelib
