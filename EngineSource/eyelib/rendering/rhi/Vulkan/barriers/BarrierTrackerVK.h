#pragma once

#include "eyelib/rendering/rhi/RHITypes.h"
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace eyelib {
namespace rendering {
namespace rhi {

struct CommandListVK;

/**
 * Tracks **logical** resource state and batches **`vkCmdPipelineBarrier2`** calls. Keys are raw **`VkBuffer` / `VkImage`**
 * handles (`void*`), not `ITexture*` / `IBuffer*` — one entry per GPU resource.
 *
 * **Workflow:** `TransitionBuffer` / `TransitionImage` → `FlushBarriers` (often via `DeviceVK::FlushBarrierTracker`, which
 * ends active dynamic rendering first). Pending barriers are merged into **one** `vkCmdPipelineBarrier2` per flush.
 *
 * **Reset (`BeginFrame`):** Clears **buffer** state and all **pending** lists. **Does not** clear **image** state: layouts
 * persist across frames so blits/uploads do not regress to spurious `UNDEFINED` → attachment transitions (see
 * `BarrierTrackerVK::Reset` implementation comment).
 *
 * **Not exhaustive:** some paths emit barriers directly (e.g. texture upload, swapchain presentation in `DeviceVKPresentation`)
 * — those must stay consistent with tracked state or use `SetImageState` / `RemoveImageState` when appropriate.
 *
 * **Per-subresource:** pending image barriers carry an explicit subresource range (mip/layer counts). The
 * whole-image layout cache (`m_imageState`) is updated only when `updateGlobalLayoutCache` is true on transition
 * (full-image path from `DeviceVK::TransitionTextureToState`). Partial-range transitions with
 * `updateGlobalLayoutCache == false` do **not** update `m_imageState`; **`GetImageState` may not reflect subranges**.
 *
 * **External barriers:** uploads (`TextureVK` staging submits), `SubmitImageUndefinedToShaderReadOnly`, and
 * presentation paths that emit `vkCmdPipelineBarrier2` outside this tracker must call **`SetImageState`** /
 * **`RemoveImageState`** (see `DeviceVKPresentation`, `CreateTextureVK`) so cached layout matches GPU.
 *
 * **Redundant transitions (Phase 3.2):** `TransitionBuffer` / `TransitionImage` (when `updateGlobalLayoutCache` is true) log a
 * capped number of **Warn**s when `from==to`. Coalescing merges duplicate pending keys before flush. **`SetImageState(Present)`**
 * (or any state) after presentation is normal and does not go through `TransitionImage`—no redundant-transition noise there.
 *
 * **Buffers:** `Reset` clears **`m_bufferState`** each frame. Device-local upload (`BufferVK` one-shot copy) and
 * host-visible initial fills happen **outside** `TransitionBuffer`; use **`SetBufferState`** after those barriers
 * complete, and **`RemoveBufferState`** when the `VkBuffer` is destroyed. Swapchain **`VkImage`** handles use
 * **`SetImageState(Acquired → Undefined)`** after acquire and **`Present`** after the transition to
 * `VK_IMAGE_LAYOUT_PRESENT_SRC_KHR` (plus **`RemoveImageState`** when swapchain images are recreated).
 */
struct BarrierTrackerVK {
    /** Clear buffer tracking and pending barriers; preserve image layout cache. Typically `BeginFrame`. */
    static void Reset(BarrierTrackerVK* tracker);

    /** Transition buffer to newState. buffer is VkBuffer (as void*). No-op if already in newState. */
    static void TransitionBuffer(BarrierTrackerVK* tracker, void* buffer, ResourceState newState);

    /** Logical state after an explicit barrier path (no pending barrier). Clears pending buffer transitions for this handle. */
    static void SetBufferState(BarrierTrackerVK* tracker, void* buffer, ResourceState state);
    static ResourceState GetBufferState(const BarrierTrackerVK* tracker, void* buffer);
    static void RemoveBufferState(BarrierTrackerVK* tracker, void* buffer);

    /** Transition image to newState. image is VkImage (as void*); aspectMask is VkImageAspectFlags (e.g. VK_IMAGE_ASPECT_COLOR_BIT).
     * Uses full subresource (remaining mips/layers) and updates the whole-image layout cache. */
    static void TransitionImage(BarrierTrackerVK* tracker, void* image, uint32_t aspectMask, ResourceState newState);
    /** Subresource-aware transition; `updateGlobalLayoutCache` should be true when the range covers the entire image. */
    static void TransitionImage(BarrierTrackerVK* tracker, void* image, uint32_t aspectMask, uint32_t baseMipLevel,
                                uint32_t levelCount, uint32_t baseArrayLayer, uint32_t arrayLayerCount, ResourceState newState,
                                bool updateGlobalLayoutCache);

    /** Record all pending barriers into the command list's command buffer. Clears pending. */
    static void FlushBarriers(BarrierTrackerVK* tracker, CommandListVK* cmdList);

    /** Whole-image layout cache (VkImage as void*). `Undefined` if never set / removed. Not authoritative after partial-only transitions. */
    static ResourceState GetImageState(const BarrierTrackerVK* tracker, void* image);

    /** Set tracked layout after an explicit barrier (no pending barrier recorded). Clears any **pending** image barriers for this `VkImage`, mirroring **`SetBufferState`**. */
    static void SetImageState(BarrierTrackerVK* tracker, void* image, ResourceState state);
    /** Remove layout cache and **pending** barriers for an image that is being retired/destroyed (e.g. swapchain recreate). */
    static void RemoveImageState(BarrierTrackerVK* tracker, void* image);

private:
    std::unordered_map<void*, ResourceState> m_bufferState;
    std::unordered_map<void*, ResourceState> m_imageState;
    struct PendingBuffer { void* buffer; ResourceState from; ResourceState to; };
    struct PendingImage {
        void* image;
        uint32_t aspectMask;
        uint32_t baseMipLevel;
        uint32_t levelCount;
        uint32_t baseArrayLayer;
        uint32_t arrayLayerCount;
        ResourceState from;
        ResourceState to;
    };
    std::vector<PendingBuffer> m_pendingBuffers;
    std::vector<PendingImage> m_pendingImages;
};

} // namespace rhi
} // namespace rendering
} // namespace eyelib
