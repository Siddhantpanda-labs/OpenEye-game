#pragma once

#include "eyelib/render/rhi/RHITypes.h"
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace eyelib {
namespace render {
namespace rhi {

struct CommandListVK;

/**
 * Tracks current resource state and records barriers on Transition. Call TransitionBuffer/TransitionImage,
 * then FlushBarriers(cmdList) to record all pending barriers. Reset() at frame start to clear tracked state.
 * Map keys are raw VkBuffer/VkImage (void*), not engine Resource* — avoids aliasing and multi-view issues.
 * Future: subresource state (e.g. mip 0 COLOR_ATTACHMENT, mip 1 SHADER_READ) for AAA-level correctness.
 */
struct BarrierTrackerVK {
    /** Clear tracked state and pending barriers (e.g. at BeginFrame). */
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
} // namespace render
} // namespace eyelib
