#include "EditorRenderFrameCoordinator.h"

#include "EditorContext.h"
#include "EditorPersistentSceneCache.h"
#include "EditorSceneDirtyTracker.h"
#include "EditorVisibilitySystem.h"

namespace openeye::editor {

void EditorRenderFrameCoordinator::BeginFrame(EditorContext& ctx) {
    ctx.world.FlushTransforms();
    ctx.world.CaptureRenderSnapshot(ctx.frameSnapshot);
    ctx.frameSnapshotValid = true;
    ++ctx.frameSnapshotSerial;
    if (ctx.sceneDirtyTracker) {
        ctx.sceneDirtyTracker->BeginFrame(ctx.frameSnapshot);
    }
    if (ctx.visibilitySystem) {
        ctx.visibilitySystem->BeginFrame(ctx.frameSnapshot);
    }
    if (ctx.sceneCacheNew)
        ctx.sceneCacheNew->BeginFrame(ctx.frameSnapshotSerial);
    if (ctx.sceneCacheNew && ctx.sceneDirtyTracker) {
        for (const auto& entity : ctx.sceneDirtyTracker->GetCacheInvalidationEntities())
            ctx.sceneCacheNew->InvalidateEntity(entity);
    }
}

} // namespace openeye::editor
