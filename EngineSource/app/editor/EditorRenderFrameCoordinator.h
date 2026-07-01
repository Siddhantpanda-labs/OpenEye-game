#pragma once

namespace openeye::editor {

struct EditorContext;

/**
 * Editor-wide render-frame coordinator.
 * Phase 1 contract: capture one immutable world snapshot per frame and expose
 * it through EditorContext for all panels.
 */
class EditorRenderFrameCoordinator {
public:
    void BeginFrame(EditorContext& ctx);
};

} // namespace openeye::editor

