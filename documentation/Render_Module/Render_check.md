# Render module — implementation track & checks

**Spec:** `new_render_module.md`  
**Code:** `EngineSource/eyelib/rendering/`  
**Build check:** `EyelibRendering` (Debug), `OpenEyeRenderingSandbox` (Debug)

---

## Principles (ongoing)


| Check                                   | Status                                                                     |
| --------------------------------------- | -------------------------------------------------------------------------- |
| GPU work only through frame graph       | In progress                                                                |
| Inter-feature comms via blackboard only | In progress — registration-time writers + per-frame publish checks (debug) |
| Dependency direction                    | Enforced by includes; review each PR                                       |
| Capabilities queried once at init       | Done                                                                       |
| No `ICommandList`* stored across frames | Passes do not retain `cmd`                                                 |
| Feature subscription                       | Done — `TrySubscribeFeature` (late = immediate `OnRegister`), `TryUnsubscribeFeature` anytime (runtime = `OnUnregisterBlackboardWriters` + `OnShutdown` + `TryRelease` slots), duplicate-safe, `FeatureDebugName` |
| Blackboard survives upload → schedule      | Done — scheduler clears graph only, not blackboard (core publishes kept) |


---

## Milestones

### M0 — Layout scaffold — Done

### M1 — Core orchestration spine — Done

**API note:** `IRenderFeature::OnSchedulePasses(..., const RenderContext& render)` for device access during scheduling.

### M2 — Upload, resources, blackboard, passes, sandbox — Done

### M3 — Scene hook, blackboard registry, compile hooks — Done


| Item                                                                           | Status |
| ------------------------------------------------------------------------------ | ------ |
| `RenderContext::sceneView` + `RenderWorld::SetSceneView`                       | Done   |
| `IRenderSceneView::TryGetCamera` / `RenderSceneView`                           | Done   |
| `BlackboardWriterRegistry` + `OnRegisterBlackboardWriters`                     | Done   |
| Debug: publish must match reserved writer when registry is bound               | Done   |
| `IFrameGraphPass::OnCompiled` + `FrameGraph::Compile(device)` pass-index pass  | Done   |
| `RenderWorld::Initialize` reserves `MainHdrRenderTarget` for `RenderResources` | Done   |


### M4.1 — Raster feature bootstrap — Done

| Item | Status |
|------|--------|
| `RasterFeature` as `IRenderFeature` (HDR clear + blit + upload scratch demo) | Done |
| Sandbox uses `RasterFeature` only (no inline feature class) | Done |

**Files:** `features/raster/RasterFeature.h`, `RasterFeature.cpp`, `app/rendering_sandbox/main.cpp`

### M4.2 — Fullscreen HDR compose (deferred bootstrap) — Done

| Item | Status |
|------|--------|
| `FullscreenHdrComposePass` (barriers + `DrawFullscreenQuad`, push tint) | Done |
| `RasterFeature` lazy pipeline from embedded GLSL (`CreateGraphicsPipelineFromShaderAsset` overrides) | Done |
| `OnShutdown` destroys compose pipeline | Done |

### M4 — Uploads, barriers, visibility → draw list — In progress (partial)


| Item                                                                                      | Status                                                                 |
| ----------------------------------------------------------------------------------------- | ---------------------------------------------------------------------- |
| Staging upload buffer capacity + `EnsureStagingUploadBuffer` / `Shutdown` ordering       | Done                                                                   |
| Real staging uploads in `OnUpload` (buffers, material tables)                             | Partial — `RenderResources::RecordGpuUploadScratchFromStaging` (owned barrier state); sandbox demo; material tables TBD |
| `FrameGraph::Compile` drives global `BarrierTracker` / layout scheduling                  | Not started — execute-time per-pass barriers only                      |
| `FrameGraph::Execute` applies pass `CollectTextureBarriers` → transition → flush        | Done                                                                   |
| `VisibilitySystem::BuildCpuDrawList` + `RenderContext::drawList` + `UploadPhase` wiring   | Partial — stub list + `DrawList::visibleCount` when camera exists   |
| Pass texture barrier hooks (`IFrameGraphPass::CollectTextureBarriers`)                  | Done — clear (RT/depth) + blit (HDR color → `ShaderResource` before `BlitToDefaultFramebuffer`) |


---

## Files touched (M4)

- `framegraph/FrameGraphBarrierList.h` (new)
- `framegraph/FrameGraphPass.h`, `FrameGraph.h`, `FrameGraph.cpp`
- `framegraph/passes/ClearBlackboardRenderTargetPass.h`, `ClearBlackboardRenderTargetPass.cpp`
- `framegraph/passes/BlitMainHdrToDefaultPass.h`, `BlitMainHdrToDefaultPass.cpp`
- `framegraph/passes/FullscreenHdrComposePass.h`, `FullscreenHdrComposePass.cpp`
- `scene/DrawList.h`
- `visibility/VisibilitySystem.h`, `VisibilitySystem.cpp`
- `core/RenderContext.h`, `core/RenderWorld.h`, `core/RenderWorld.cpp`
- `core/RenderResources.h`, `core/RenderResources.cpp`
- `rhi/RHI.h`, `rhi/Vulkan/commands/CommandListVK.h`, `CommandListVK.cpp`
- `app/rendering_sandbox/main.cpp`


---

## Files touched (M3)

- `framegraph/BlackboardWriterRegistry.h` (new)
- `framegraph/FrameGraphBlackboard.h`, `FrameGraphPass.h`, `FrameGraph.h`, `FrameGraph.cpp`
- `scene/IRenderSceneView.h`, `scene/RenderSceneView.h`, `scene/RenderSceneView.cpp`
- `core/RenderContext.h`, `core/RenderWorld.h`, `core/RenderWorld.cpp`
- `scheduler/RenderFeature.h`
- `app/rendering_sandbox/main.cpp`

---

*Update this file whenever a milestone or principle check changes.*