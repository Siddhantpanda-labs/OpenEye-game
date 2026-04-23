# Render Fix Track — Invisible Cube (Vulkan forward-opaque)

Tracks every diagnostic/fix made while chasing the "only dark blue clear, cube never shows" bug in `OpenEyeRenderingSandbox`. Revert items marked **[DIAG]** once the root cause is fixed.

## Problem

- `RasterFeature` + `ForwardOpaquePass` reach the draw path each frame (`visible=2`, pipeline/meshResolver/drawList all `ok`).
- No Vulkan validation errors reported; swapchain shows only the HDR clear color.
- Camera/projection math verified correct: world origin → NDC (0, -0.433, 0.988), inside frustum and passes `LESS` depth test against clear=1.0.

## Changes made

### 1. Vulkan debug messenger (FIX — keep)

File: `EngineSource/eyelib/rendering/rhi/Vulkan/core/InstanceVK.{h,cpp}`

- Added `debugMessenger` field on `InstanceVK`.
- `InstanceVK::Create` now creates a `VkDebugUtilsMessengerEXT` (warning+error, all types) routing to `core::Log::Warn/Error/Info` when `OPENEYE_VULKAN_VALIDATION` is on.
- `InstanceVK::Destroy` tears it down.
- Reason: validation layer was enabled but no callback was installed → every validation message was silently dropped.

### 2. Explicit dynamic depth/cull state in forward pass (FIX — keep)

File: `EngineSource/eyelib/rendering/framegraph/passes/ForwardOpaquePass.cpp`

- Before `BindPipeline`, call `SetDepthTest(true) / SetDepthWrite(true) / SetDepthFuncLess(true) / SetCullFace(false)`.
- Reason: the forward pipeline marks `DEPTH_TEST_ENABLE / DEPTH_WRITE_ENABLE / DEPTH_COMPARE_OP / CULL_MODE` as dynamic state. `CommandListVK::BindPipeline` re-issues those from its tracked flags, which the present-resolve blit mutates (`SetDepthTest(false)` etc.). Making the forward pass self-describing avoids any cross-pass leakage.

### 3. Diagnostic logs (**[DIAG]** — revert after fix)

- `EngineSource/eyelib/rendering/rhi/Vulkan/core/InstanceVK.cpp`
  - One-shot info log on messenger install / warn on fallback.
- `EngineSource/eyelib/rendering/shaders/ShaderLibrary.{h,cpp}`
  - `Entry::warmUpFailureLogged` + throttled `WarnF` if `CreateGraphicsPipelineFromShaderAsset` returns null.
- `EngineSource/eyelib/rendering/rhi/Vulkan/commands/CommandListVK.cpp`
  - Throttled `WarnF` in `BindPipeline` when `VkPipeline == VK_NULL_HANDLE`.
- `EngineSource/eyelib/rendering/features/raster/RasterFeature.{h,cpp}`
  - Flags: `m_diagLoggedForwardPipelineNull`, `m_diagLoggedMeshResolverNull`, `m_diagLoggedDrawListNull`, `m_diagScheduleLogCounter`, `m_diagLoggedPerViewOnce`.
  - `OnSchedulePasses`: throttled (`% 120`) info log `visible=… forwardPipeline=… meshResolver=… drawList=…` + one-shot warns when any dependency is null.
  - `OnUpload`: one-shot dump of `viewPos`, output dims, view cols (0,3), projection cols (0,2,3), world-origin clip/NDC, `MainHdrRenderTarget` dims + `hasDepth`.
- `EngineSource/eyelib/rendering/framegraph/passes/ForwardOpaquePass.{h,cpp}`
  - Flags: `m_diagLoggedEarlyExit / NoRt / BuffersMissing / NoDraws / FirstDraw`.
  - One-shot `WarnF` on every early-exit branch + one-shot `InfoF "first draw path reached (visible=%u)"` right before `SetRenderTarget`.
- Also have disabled frustrum culling.

## Observations from the logs

- Messenger works: only `WARNING-Shader-OutputNotConsumed` for vertex attribute locations 3–7 (declared in `Mesh::GetVertexLayoutRHI` but unused by `forward_opaque.vert`) — not fatal.
- No pipeline-null, descriptor-incomplete, or barrier-layout complaints.
- `MainHdrRenderTarget` is 1280x720 with depth; `PerView` matrices are correct.
- `present_resolve shader path (src=0x61 dst=0x2c)` selected every frame.

## Next candidates if cube still hidden

1. Render directly to swapchain (bypass `BlitMainHdrToDefaultPass`) to confirm the draw reached the HDR color attachment.
2. Log `VkPipeline` / `VkPipelineLayout` handle values from `ForwardOpaquePass` and cross-check with the handles used by `vkCmdBindPipeline`.
3. Temporarily disable depth test (pipeline + dynamic) and check if visibility changes.
4. Hard-code `outColor = vec4(1,0,1,1)` in `forward_opaque.frag` to rule out shader-logic issues.





saw that 

in short whatever i do in forward_opaque in outColor had no effect

but as i changed in fragcolor in present_resolve then that color was shown,

and before hardcoding it showed the clear bluish grey color when the fragcolor was as FragColor = vec4(LinearToSRGB(linear), 1.0);



so can you decipher this situation

so diagnosed as  
I found one **strong code-level suspect** that can exactly cause “draw runs but no pixels show”:

- `ForwardOpaquePass` uses **depth test = LESS**.
- Dynamic rendering uses depth attachment with `**loadOp = LOAD`** (not clear).
- So visibility depends on `ClearBlackboardRenderTargetPass` successfully clearing depth to `1.0` each frame.

But in Vulkan command code:

```339:392:EngineSource/eyelib/rendering/rhi/Vulkan/commands/CommandListVK.cpp
// BeginRenderingWithTarget:
depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
renderingInfo.pDepthAttachment = &depthAttachment;
renderingInfo.pStencilAttachment = nullptr;
```

and clear does:

```414:447:EngineSource/eyelib/rendering/rhi/Vulkan/commands/CommandListVK.cpp
att.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
att.clearValue.depthStencil.depth = clear.depth;
att.clearValue.depthStencil.stencil = clear.stencil;
vkCmdClearAttachments(...)
```

### Why this is risky

If that depth clear path is not effectively applied, depth remains undefined/old, and with `LESS` all forward fragments can fail -> exactly your symptom (forward outColor changes do nothing, present_resolve hardcode works).

### In short

The likely issue is **depth path**, not color resolve:

- depth attachment is loaded (not auto-cleared),
- clear is done manually via `vkCmdClearAttachments`,
- any mismatch/ineffective clear there means **all forward fragments get depth-rejected**.

That is the most plausible code root cause from what you observed.

Correct architectural fix is to stop doing “manual clear as a separate pass” for render targets that are immediately rendered, and make **pass load/store ops explicit in pass begin**.

### Recommended architecture

- Use **`LOAD_OP_CLEAR`** for first writer of `MainHdr` color+depth each frame.
- Use **`LOAD_OP_LOAD`** only for subsequent passes that genuinely preserve prior content.
- Keep barriers/state transitions in frame graph, but move clear intent into render-pass begin info (dynamic rendering attachment load ops), not `vkCmdClearAttachments` bootstrap.

---

### Concretely for your flow

Current flow:
- `ClearBlackboardRenderTargetPass` (manual clear)
- `ForwardOpaquePass` (load/load)

Better flow:
- Remove standalone clear pass for this path.
- In `ForwardOpaquePass` begin rendering:
  - color attachment: `LOAD_OP_CLEAR` + clear color
  - depth attachment: `LOAD_OP_CLEAR` + depth=1.0
- Later passes that read/compose use `LOAD_OP_LOAD` or sampled reads as needed.

---

### Why this is the right fix

- Eliminates fragile dependency on “manual clear happened correctly before load”.
- Makes first-writer semantics deterministic and local to the pass.
- Aligns with frame-graph architecture: each pass declares resource usage + load/store policy.
- Scales better (deferred, post, overlays, multi-target) without hidden clear assumptions.

---

### Design improvement (long-term) - Prefering the longterm Solution

Add a small pass-level API contract, e.g.:

- `RenderAttachmentOps { colorLoadOp, colorStoreOp, depthLoadOp, depthStoreOp, clearValues }`
- Frame graph computes first-writer and validates conflicting ops.
- CommandList/DeviceVK builds `VkRenderingAttachmentInfo` from this contract.

That is the clean architectural direction and will prevent this class of bug.

## Revert checklist (once fixed)

- Remove all **[DIAG]** logs/flags listed above.
- Keep: debug messenger install/destroy (item 1).
- Keep: explicit depth/cull state setup in `ForwardOpaquePass::Execute` (item 2), or fold into a shared helper.

