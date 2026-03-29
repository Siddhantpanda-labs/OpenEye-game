#pragma once

#include "eyelib/render/RenderContext.h"
#include "eyelib/render/renderer/RenderPipelineMode.h"
#include "eyelib/render/rhi/RHI.h"
#include <cstdint>
#include <functional>
#include <memory>

namespace eyelib {
namespace render {

class FrameGraph;

/**
 * Owns the FrameGraph instance and compile scheduling (size / mode / stereo).
 * Pass wiring (recipes, executors) is supplied by the host via CompileIfNeeded's build callback.
 */
class RenderGraphOrchestrator {
public:
    RenderGraphOrchestrator();
    ~RenderGraphOrchestrator();

    RenderGraphOrchestrator(const RenderGraphOrchestrator&) = delete;
    RenderGraphOrchestrator& operator=(const RenderGraphOrchestrator&) = delete;

    FrameGraph* GetFrameGraph();
    const FrameGraph* GetFrameGraph() const;

    /** Resize target dimensions for the graph (e.g. after window resize); does not imply compile until MarkNeedsCompile + CompileIfNeeded. */
    void SetFrameSize(uint32_t width, uint32_t height);

    /** Invalidate compiled graph (initial state, resize, or mode change from host). */
    void MarkNeedsCompile();

    /**
     * Rebuilds and compiles the graph when dimensions, mode, stereo, or MarkNeedsCompile demand it.
     * @param buildGraph  Fills passes (ImportTexture, AddPass, AddPassesFromRecipe, etc.).
     * @return true if a compile occurred this call.
     */
    bool CompileIfNeeded(PipelineMode mode, bool stereo, uint32_t width, uint32_t height, rhi::IDevice* device,
                           const std::function<void(FrameGraph&)>& buildGraph);

    void Execute(const RenderContext& ctx);

private:
    std::unique_ptr<FrameGraph> m_frameGraph;
    bool m_needsCompile = true;
    uint32_t m_lastWidth = 0;
    uint32_t m_lastHeight = 0;
    PipelineMode m_lastMode = PipelineMode::Deferred;
    bool m_lastStereo = false;
};

}  // namespace render
}  // namespace eyelib
