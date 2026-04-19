#pragma once

#include "eyelib/rendering/rhi/RHI.h"
#include <cstdint>
#include <unordered_set>
#include <vector>

namespace eyelib {
namespace rendering {
namespace rhi {

/** Metadata for graphics→compute queue-family release/acquire of a `VkImage` (see `DeviceVK::SubmitCommandList`). */
struct CrossQueueImageMeta {
    void* vkImage = nullptr;
    uint32_t aspectMask = 0;
    uint32_t baseMipLevel = 0;
    uint32_t levelCount = 1;
    uint32_t baseArrayLayer = 0;
    uint32_t arrayLayerCount = 1;
    /** Layout used for both sides of the ownership transfer (must match pre-transition layout in the user command buffer). */
    ResourceState ownershipLayout = ResourceState::Undefined;
};

struct DeviceVK;
struct RenderTargetVK;
struct PipelineVK;
struct BufferVK;
struct TextureVK;
struct RenderTargetVK;

/** Vulkan `ICommandList`: records into a **primary** `VkCommandBuffer`.
 *
 * **Immediate list** (per device): buffer is rebound each `DeviceVK::BeginFrame` after **fence wait** and
 * `vkResetCommandPool`; `Begin()`/`End()` wrap `vkBeginCommandBuffer`/`vkEndCommandBuffer` for the frame workload.
 * **Deferred lists** are constructed with buffer + pool; submitted via `DeviceVK::SubmitCommandList` (optional
 * timeline signal), then freed with `DeviceVK::FreeCommandBuffer`.
 *
 * **Rendering:** HDR/offscreen/swapchain-resolve paths use **`vkCmdBeginRendering`** / `vkCmdEndRendering` (dynamic
 * rendering). Active pass extent is tracked in `m_activeDynamicRenderWidth/Height` for draws (including fullscreen).
 * Swapchain **classic** `VkRenderPass` usage lives in presentation code, not here.
 *
 * **Queries:** `ResetQueryPool` / `BeginQuery` / `EndQuery` interact with dynamic rendering and optional host query reset;
 * see `query_pool_dynamic_rendering.md` (same `Vulkan/` tree).
 */
struct CommandListVK : ICommandList {
    /** For immediate list: device creates and sets buffer via SetCommandBuffer(buf, true). */
    explicit CommandListVK(DeviceVK* device);
    /** For deferred list: device allocates from frame pool and passes buffer + pool (pool used at free). */
    CommandListVK(DeviceVK* device, void* commandBuffer, void* commandPool);
    ~CommandListVK() override;

    /** Set command buffer (used by device for immediate list after pool allocation). */
    void SetCommandBuffer(void* commandBuffer, bool isImmediate);
    bool IsImmediate() const { return m_isImmediate; }
    bool IsRecording() const override { return m_isRecording; }

    void Begin() override;
    void End() override;

    void SetRenderTarget(IRenderTarget* rt) override;
    void SetRenderTarget(IRenderTarget* rt, uint32_t colorAttachmentIndex) override;
    void ClearRenderTarget(IRenderTarget* rt, const ClearValue& clear) override;
    void ClearRenderTargetColorOnly(IRenderTarget* rt, const ClearValue& clear) override;
    void ClearDefaultFramebuffer(const ClearValue& clear) override;
    void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) override;
    void SetViewportIndexed(uint32_t index, uint32_t x, uint32_t y, uint32_t width, uint32_t height) override;
    void SetDepthTest(bool enable) override;
    void SetDepthWrite(bool enable) override;
    void SetDepthFuncLess(bool less) override;
    void SetCullFace(bool enable) override;
    void SetCullFrontFace(bool cullFront) override;
    void SetBlend(bool enable) override;
    void SetBlendFunc(BlendFactor src, BlendFactor dst) override;
    void SetPolygonOffsetLine(bool enable, float factor, float units) override;

    void BarrierBuffer(IBuffer* buffer, ResourceState fromState, ResourceState toState) override;
    void BarrierTexture(ITexture* texture, ResourceState fromState, ResourceState toState) override;
    void MemoryBarrierStorage() override;
    void BarrierBufferTransferReadToHostWrite(IBuffer* buffer) override;
    void BarrierHostVisibleBufferToTransferRead(IBuffer* buffer) override;
    void BindPixelPackBufferAsSSBO(void* pboHandle, uint32_t slot, size_t size) override;

    void BindPipeline(IPipeline* pipeline) override;
    void BindVertexBuffer(IBuffer* buffer, uint32_t binding, uint32_t offset) override;
    void BindIndexBuffer(IBuffer* buffer, IndexType indexType, uint32_t offset) override;
    void DrawFullscreenQuad(IPipeline* pipeline) override;
    void BindUniformBuffer(IBuffer* buffer, uint32_t binding, uint32_t offset, uint32_t size) override;
    void BindStorageBuffer(IBuffer* buffer, uint32_t binding, uint32_t offset, uint32_t size) override;
    void BindAtomicCounterBuffer(uint32_t slot, IBuffer* buffer, size_t offset, size_t size) override;
    void BindTexture(ITexture* texture, uint32_t binding) override;
    void UnbindTexture(uint32_t binding) override;
    void BindDescriptorSet(IDescriptorSet* set) override;
    void BindDescriptorSets(uint32_t firstSet, uint32_t count, IDescriptorSet* const* sets, const uint32_t* dynamicOffsets = nullptr, uint32_t dynamicOffsetCount = 0) override;
    void BindBindlessHeap(IBindlessHeap* heap) override;

    void SetPushConstants(uint32_t offset, uint32_t size, const void* data) override;

    void SetUniformInt(const char* name, int value) override;
    void SetUniformUint(const char* name, uint32_t value) override;
    void SetUniformFloat(const char* name, float value) override;
    void SetUniformVec2(const char* name, const float* data) override;
    void SetUniformVec3(const char* name, const float* data) override;
    void SetUniformVec4(const char* name, const float* data) override;
    void SetUniformMat4(const char* name, const float* data) override;
    void SetUniformMat4Array(const char* name, const float* data, size_t count) override;
    void SetUniformHandle64(const char* name, uint64_t handle) override;
    void SetSubroutineIndices(ShaderStage stage, uint32_t count, const uint32_t* indices) override;

    void Draw(uint32_t vertexCount, uint32_t firstVertex) override;
    void DrawIndexed(uint32_t indexCount, uint32_t firstIndex, int32_t vertexOffset) override;
    void DrawIndexedSubmesh(uint32_t indexCount, uint32_t firstIndex, int32_t vertexOffset) override;
    void DrawIndexedInstanced(uint32_t indexCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t instanceCount) override;
    void DrawArraysIndirect(IBuffer* indirectBuffer, size_t offset) override;
    void DrawIndexedIndirect(IBuffer* indirectBuffer, size_t offset) override;
    void MultiDrawArraysIndirect(IBuffer* indirectBuffer, size_t offset, uint32_t drawCount, uint32_t stride) override;
    void MultiDrawIndexedIndirect(IBuffer* indirectBuffer, size_t offset, uint32_t drawCount, uint32_t stride) override;
    void MultiDrawArraysIndirectCount(IBuffer* indirectBuffer, size_t indirectOffset, IBuffer* countBuffer, size_t countBufferOffset, uint32_t maxDrawCount, uint32_t stride) override;
    void MultiDrawIndexedIndirectCount(IBuffer* indirectBuffer, size_t indirectOffset, IBuffer* countBuffer, size_t countBufferOffset, uint32_t maxDrawCount, uint32_t stride) override;
    void Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) override;
    void DispatchIndirect(IBuffer* indirectBuffer, size_t offset) override;
    void DrawMeshTasks(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) override;
    void DrawMeshTasksIndirect(IBuffer* indirectBuffer, size_t offset, uint32_t drawCount, uint32_t stride) override;
    void SetFragmentShadingRate(FragmentShadingRate rate,
                                FragmentShadingRateCombinerOp pipelinePrimitiveOp,
                                FragmentShadingRateCombinerOp primitiveAttachmentOp) override;

    void ResetQueryPool(IQueryPool* pool, uint32_t firstQuery, uint32_t queryCount) override;
    void BeginQuery(IQueryPool* pool, uint32_t index) override;
    void EndQuery(IQueryPool* pool, uint32_t index) override;

    void BindTransformFeedbackBuffer(uint32_t slot, IBuffer* buffer, size_t offset, size_t size) override;
    void BeginTransformFeedback(PrimitiveTopology primitiveMode) override;
    void EndTransformFeedback() override;

    void BeginConditionalRender(IQueryPool* pool, uint32_t index) override;
    void EndConditionalRender() override;

    IFence* InsertFence() override;

    void CopyBuffer(IBuffer* src, size_t srcOffset, IBuffer* dst, size_t dstOffset, size_t size) override;
    void BuildAccelerationStructures(uint32_t infoCount, const void* pBuildGeometryInfos, const void* const* ppBuildRangeInfos) override;
    void MemoryBarrierAccelerationStructureAccess() override;

    /** Backend: VkCommandBuffer (opaque to avoid including vulkan.h in header). */
    void* GetCommandBuffer() const { return m_commandBuffer; }

    /** Vulkan-only: nested **debug regions** for RenderDoc / GPU captures (**`VK_EXT_debug_utils`**). No-op if unsupported. */
    void PushDebugUtilsLabel(const char* name, const float* colorRgba = nullptr) override;
    void PopDebugUtilsLabel() override;
    /** For compute UpdateComputeSet0: current bound texture at binding (null if unbound). */
    rhi::ITexture* GetBoundTexture(uint32_t binding) const;
    /** For compute UpdateComputeSet0: current bound storage buffer at binding. */
    void GetBoundStorageBuffer(uint32_t binding, rhi::IBuffer** outBuffer, uint32_t* outOffset, uint32_t* outSize) const;
    /** For DeviceVK::UpdateComputeSet0: current bound uniform buffer at binding. */
    void GetBoundUniformBuffer(uint32_t binding, rhi::IBuffer** outBuffer, uint32_t* outOffset, uint32_t* outSize) const;
    /** ParamsBlock data for compute binding 1 (light_cull). Device reads and uploads to UBO before dispatch. */
    const uint8_t* GetComputeParamsData() const { return m_computeParamsData; }
    /** Timeline value to signal on next submit (0 = none). Set by InsertFence; read/cleared by DeviceVK::SubmitCommandList. */
    uint64_t GetPendingTimelineValue() const { return m_pendingTimelineValue; }
    void ClearPendingTimelineValue() { m_pendingTimelineValue = 0; }
    bool HasGraphicsWork() const { return m_hasGraphicsWork; }
    bool HasComputeWork() const { return m_hasComputeWork; }
    /** True when this list recorded compute only and no path disabled async routing; see `DeviceVK::SubmitCommandList` + `documentation/vulkan/steps_todo.md` Phase C. */
    bool IsComputeQueueEligible() const { return m_computeQueueEligible && m_hasComputeWork && !m_hasGraphicsWork; }
    bool RequiresQueueOwnershipTransfers() const { return m_requiresQueueOwnershipTransfers; }
    /** True when cross-queue submit cannot use the buffer/image release+acquire bridge (e.g. host barriers, global memory barrier, texture with unknown prior layout). */
    bool CrossQueueOwnershipUnsafe() const { return m_crossQueueOwnershipUnsafe; }
    /** VkBuffer handles (as void*) touched by copies/barriers that need a graphics→compute ownership handoff. */
    const std::vector<void*>& GetCrossQueueBuffers() const { return m_crossQueueBuffers; }
    /** Images (and pre-transition layout) for the same bridge; empty if only buffers or unsafe paths. */
    const std::vector<CrossQueueImageMeta>& GetCrossQueueImages() const { return m_crossQueueImages; }
    bool GetDepthTestEnabled() const { return m_depthTestEnabled; }
    bool GetDepthWriteEnabled() const { return m_depthWriteEnabled; }
    bool GetDepthFuncLess() const { return m_depthFuncLess; }
    bool GetCullFaceEnabled() const { return m_cullFaceEnabled; }
    bool GetCullFrontFace() const { return m_cullFrontFace; }

    void EndRenderingIfActive();
    /** Begin dynamic rendering into a swapchain color image view (image must already be in COLOR_ATTACHMENT_OPTIMAL). */
    void BeginRenderingSwapchainColor(void* colorImageView, uint32_t width, uint32_t height, bool clear, const float clearColor[4]);
    /** Begin dynamic rendering with given RT. */
    void BeginRenderingWithTarget(RenderTargetVK* rtVK, bool doTransitions);
    /** Push current graphics constants for the currently bound pipeline layout (if available). */
    void PushGraphicsConstantsIfPossible();
    /** Called by DeviceVK::BeginFrame() after reset+begin so the list is marked recording. Do not call from application code. */
    void SetRecordingState(bool recording) {
        m_isRecording = recording;
        if (!recording) {
            m_isRenderingActive = false;
            m_currentRenderTarget = nullptr;
            m_activeDynamicRenderWidth = 0;
            m_activeDynamicRenderHeight = 0;
        }
    }
    /** After vkResetCommandPool + vkBeginCommandBuffer: GPU CB has no RT/pipeline/bind state; CPU must match. */
    void ResetCpuRecordingStateAfterPoolReset();
    void NoteCrossQueueBuffer(void* vkBuffer);
    void NoteCrossQueueImage(void* vkImage, uint32_t aspectMask, uint32_t baseMipLevel, uint32_t levelCount, uint32_t baseArrayLayer,
                             uint32_t arrayLayerCount, ResourceState ownershipLayout);

private:
    DeviceVK* m_device = nullptr;
    void* m_commandBuffer = nullptr;   // VkCommandBuffer
    void* m_commandPool = nullptr;     // VkCommandPool (for deferred list free only)
    bool m_isImmediate = false;
    bool m_isRecording = false;
    bool m_isRenderingActive = false;
    RenderTargetVK* m_currentRenderTarget = nullptr;  // active dynamic rendering RT (null for swapchain-only vkCmdBeginRendering)
    /** Extent of the active vkCmdBeginRendering pass (matches renderArea); valid while m_isRenderingActive. */
    uint32_t m_activeDynamicRenderWidth = 0;
    uint32_t m_activeDynamicRenderHeight = 0;
    uint64_t m_pendingTimelineValue = 0;  // signal this value on next submit (0 = none)
    void* m_currentVertexBuffer = nullptr;  // VkBuffer
    uint32_t m_currentVertexOffset = 0;
    void* m_currentIndexBuffer = nullptr;  // VkBuffer
    uint32_t m_currentIndexOffset = 0;
    IndexType m_currentIndexType = IndexType::UInt16;
    rhi::IPipeline* m_currentPipeline = nullptr;
    uint8_t m_pushConstants[256] = {};  // vertex + fragment push constants (e.g. u_UseIBL, u_ViewProj, stereo)
    static constexpr uint32_t kMaxLegacyBindings = 16;
    rhi::ITexture* m_boundTextures[kMaxLegacyBindings] = {};
    rhi::IBuffer* m_boundStorageBuffers[kMaxLegacyBindings] = {};
    uint32_t m_boundStorageOffsets[kMaxLegacyBindings] = {};
    uint32_t m_boundStorageSizes[kMaxLegacyBindings] = {};
    rhi::IBuffer* m_boundUniformBuffers[kMaxLegacyBindings] = {};
    uint32_t m_boundUniformOffsets[kMaxLegacyBindings] = {};
    uint32_t m_boundUniformSizes[kMaxLegacyBindings] = {};
    /** Staging for PushOnly compute legacy UBO upload (see DeviceVK::UpdateComputeSet0); not used by resource-space passes. */
    uint8_t m_computeParamsData[256] = {};
    bool m_depthTestEnabled = true;
    bool m_depthWriteEnabled = true;
    bool m_depthFuncLess = true;
    bool m_cullFaceEnabled = false;
    bool m_cullFrontFace = false;
    bool m_pushModelSet = false;
    bool m_hasGraphicsWork = false;
    bool m_hasComputeWork = false;
    bool m_computeQueueEligible = true;
    bool m_requiresQueueOwnershipTransfers = false;
    bool m_crossQueueOwnershipUnsafe = false;
    std::vector<void*> m_crossQueueBuffers;
    std::unordered_set<uint64_t> m_crossQueueBufferSet;
    std::vector<CrossQueueImageMeta> m_crossQueueImages;
    std::unordered_set<uint64_t> m_activeQueryKeys;
};

} // namespace rhi
} // namespace rendering
} // namespace eyelib
