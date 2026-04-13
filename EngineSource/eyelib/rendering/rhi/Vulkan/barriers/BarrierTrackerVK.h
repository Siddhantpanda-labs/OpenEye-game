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
 * Tracks **logical** resource state and batches **`vkCmdPipelineBarrier`** calls. Keys are raw **`VkBuffer` / `VkImage`**
 * handles (`void*`), not `ITexture*` / `IBuffer*` — one entry per GPU resource.
 *
 * **Workflow:** `TransitionBuffer` / `TransitionImage` → `FlushBarriers` (often via `DeviceVK::FlushBarrierTracker`, which
 * ends active dynamic rendering first). Pending barriers are merged into **one** `vkCmdPipelineBarrier` per flush.
 *
 * **Reset (`BeginFrame`):** Clears **buffer** state and all **pending** lists. **Does not** clear **image** state: layouts
 * persist across frames so blits/uploads do not regress to spurious `UNDEFINED` → attachment transitions (see
 * `BarrierTrackerVK::Reset` implementation comment).
 *
 * **Not exhaustive:** some paths emit barriers directly (e.g. texture upload, swapchain presentation in `DeviceVKPresentation`)
 * — those must stay consistent with tracked state or use `SetImageState` / `RemoveImageState` when appropriate.
 *
 * **Future:** per-subresource tracking (mip/layer) for full correctness on partial transitions.
 */
struct BarrierTrackerVK {
    /** Clear buffer tracking and pending barriers; preserve image layout cache. Typically `BeginFrame`. */
    static void Reset(BarrierTrackerVK* tracker);

    /** Transition buffer to newState. buffer is VkBuffer (as void*). No-op if already in newState. */
    static void TransitionBuffer(BarrierTrackerVK* tracker, void* buffer, ResourceState newState);

    /** Transition image to newState. image is VkImage (as void*); aspectMask is VkImageAspectFlags (e.g. VK_IMAGE_ASPECT_COLOR_BIT). */
    static void TransitionImage(BarrierTrackerVK* tracker, void* image, uint32_t aspectMask, ResourceState newState);

    /** Record all pending barriers into the command list's command buffer. Clears pending. */
    static void FlushBarriers(BarrierTrackerVK* tracker, CommandListVK* cmdList);

    /** Tracked layout for an image (VkImage as void*). Undefined if unknown. */
    static ResourceState GetImageState(const BarrierTrackerVK* tracker, void* image);

    /** Set tracked layout after an explicit barrier (no pending barrier recorded). */
    static void SetImageState(BarrierTrackerVK* tracker, void* image, ResourceState state);
    /** Remove all tracking for an image that is being retired/destroyed. */
    static void RemoveImageState(BarrierTrackerVK* tracker, void* image);

private:
    std::unordered_map<void*, ResourceState> m_bufferState;
    std::unordered_map<void*, ResourceState> m_imageState;
    struct PendingBuffer { void* buffer; ResourceState from; ResourceState to; };
    struct PendingImage { void* image; uint32_t aspectMask; ResourceState from; ResourceState to; };
    std::vector<PendingBuffer> m_pendingBuffers;
    std::vector<PendingImage> m_pendingImages;
};

} // namespace rhi
} // namespace rendering
} // namespace eyelib
