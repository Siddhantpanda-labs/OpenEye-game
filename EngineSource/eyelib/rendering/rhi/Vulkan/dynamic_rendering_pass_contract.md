# Dynamic rendering — pass contract (Vulkan RHI)

**Scope:** `vkCmdBeginRendering` / `vkCmdEndRendering` in `CommandListVK`, graphics pipelines built with `VkPipelineRenderingCreateInfo` in `CreateVkGraphicsPipeline` (`PipelineVK.cpp`). There is **no** classic `VkRenderPass` graph in this path.

## What the frame graph / renderer owns

- **Attachment count and formats** of each pass’s `IRenderTarget` (or swapchain image) must match the **`PipelineDesc::attachmentFormats`** (or inferred-by-**`layoutKind`**) path used to create the **`IPipeline`**. Mismatch yields undefined behavior at draw time even if pipeline creation succeeded.
- **Load/store and clears:** today **`CommandListVK::BeginRenderingWithTarget`** uses **load** for color and depth and **store** for all attachments unless a narrower path is added later. Callers that need **clear-first** must either transition/clear elsewhere or extend the RHI with explicit load ops (see matrix row **Load/store optimization**).
- **MSAA sample count:** pipelines are created with **`rasterizationSamples = 1`**. If MSAA RTs are introduced, both **`VkPipelineMultisampleStateCreateInfo`** and **`VkPipelineRenderingCreateInfo`** / target samples must be aligned in a dedicated change.

## What `CreateVkGraphicsPipeline` enforces (Phase 6.1)

- Color count ≤ **`maxColorAttachments`** and ≤ **`maxFragmentOutputAttachments`**.
- No **`VK_FORMAT_UNDEFINED`** color slot; explicit **`PipelineAttachmentFormats::color`** must not contain **`None`** or depth/stencil **`RHITextureFormat`** values.
- Stencil-only **`stencilAttachmentFormat`** without depth is rejected (not produced by current format mapping).

### Depth/stencil state vs attachments

Many call sites leave **`DepthStencilState`** at defaults (**depth test/write on**) while using layouts with **no** depth attachment in **`VkPipelineRenderingCreateInfo`** (e.g. Post, IBL, PushOnly color-only). That combination is invalid for Vulkan. **`CreateVkGraphicsPipeline`** **clamps** depth test/write off when there is no **`depthAttachmentFormat`**, and stencil test off when there is no **`stencilAttachmentFormat`**, with **capped `Warn` logs** — prefer fixing **`PipelineDesc`** (or explicit **`attachmentFormats`**) so the declared state matches the pass.

## References

- `documentation/vulkan/steps_todo.md` — Phase 6 checklist.
- `documentation/vulkan/vulkan_check.md` — matrix rows **Render pass abstraction**, **Load/store optimization**, **Dynamic rendering**.
