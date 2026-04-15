#include "eyelib/rendering/rhi/Vulkan/barriers/BarrierTrackerVK.h"
#include "eyelib/rendering/rhi/Vulkan/commands/CommandListVK.h"
#include "eyelib/rendering/rhi/Vulkan/utils/VulkanEnums.h"
#include "eyelib/core/Log.h"
#include <algorithm>
#include <vulkan/vulkan.h>

namespace eyelib {
namespace rendering {
namespace rhi {

void BarrierTrackerVK::Reset(BarrierTrackerVK* tracker) {
    if (!tracker) return;
    tracker->m_bufferState.clear();
    tracker->m_pendingBuffers.clear();
    tracker->m_pendingImages.clear();
    // Intentionally keep m_imageState: GPU image layouts persist across frames. Clearing image state
    // previously caused incorrect UNDEFINED→attachment transitions after blits (sources still in TRANSFER_SRC, etc.).
}

void BarrierTrackerVK::TransitionBuffer(BarrierTrackerVK* tracker, void* buffer, ResourceState newState) {
    if (!tracker || !buffer) return;
    ResourceState& current = tracker->m_bufferState[buffer];
    if (current == newState) {
        static int warnCount = 0;
        if (warnCount++ < 8)
            core::Log::Warn("BarrierTrackerVK::TransitionBuffer: redundant transition request (from==to).");
        return;
    }
    for (auto it = tracker->m_pendingBuffers.rbegin(); it != tracker->m_pendingBuffers.rend(); ++it) {
        if (it->buffer == buffer) {
            it->to = newState;
            if (it->from == it->to) {
                tracker->m_pendingBuffers.erase(std::next(it).base());
            }
            current = newState;
            return;
        }
    }
    tracker->m_pendingBuffers.push_back({ buffer, current, newState });
    current = newState;
}

void BarrierTrackerVK::SetBufferState(BarrierTrackerVK* tracker, void* buffer, ResourceState state) {
    if (!tracker || !buffer) return;
    tracker->m_bufferState[buffer] = state;
    tracker->m_pendingBuffers.erase(std::remove_if(tracker->m_pendingBuffers.begin(), tracker->m_pendingBuffers.end(),
                                                   [buffer](const PendingBuffer& p) { return p.buffer == buffer; }),
                                    tracker->m_pendingBuffers.end());
}

ResourceState BarrierTrackerVK::GetBufferState(const BarrierTrackerVK* tracker, void* buffer) {
    if (!tracker || !buffer) return ResourceState::Undefined;
    auto it = tracker->m_bufferState.find(buffer);
    return it == tracker->m_bufferState.end() ? ResourceState::Undefined : it->second;
}

void BarrierTrackerVK::RemoveBufferState(BarrierTrackerVK* tracker, void* buffer) {
    if (!tracker || !buffer) return;
    tracker->m_bufferState.erase(buffer);
    tracker->m_pendingBuffers.erase(std::remove_if(tracker->m_pendingBuffers.begin(), tracker->m_pendingBuffers.end(),
                                                   [buffer](const PendingBuffer& p) { return p.buffer == buffer; }),
                                    tracker->m_pendingBuffers.end());
}

void BarrierTrackerVK::TransitionImage(BarrierTrackerVK* tracker, void* image, uint32_t aspectMask, ResourceState newState) {
    TransitionImage(tracker, image, aspectMask, 0u, VK_REMAINING_MIP_LEVELS, 0u, VK_REMAINING_ARRAY_LAYERS, newState, true);
}

void BarrierTrackerVK::TransitionImage(BarrierTrackerVK* tracker, void* image, uint32_t aspectMask, uint32_t baseMipLevel,
                                       uint32_t levelCount, uint32_t baseArrayLayer, uint32_t arrayLayerCount, ResourceState newState,
                                       bool updateGlobalLayoutCache) {
    if (!tracker || !image) return;
    ResourceState& current = tracker->m_imageState[image];
    if (updateGlobalLayoutCache && current == newState) {
        static int warnCount = 0;
        if (warnCount++ < 8)
            core::Log::Warn("BarrierTrackerVK::TransitionImage: redundant transition request (from==to).");
        return;
    }
    const ResourceState fromState = current;
    for (auto it = tracker->m_pendingImages.rbegin(); it != tracker->m_pendingImages.rend(); ++it) {
        if (it->image == image && it->aspectMask == aspectMask && it->baseMipLevel == baseMipLevel && it->levelCount == levelCount &&
            it->baseArrayLayer == baseArrayLayer && it->arrayLayerCount == arrayLayerCount) {
            it->to = newState;
            if (it->from == it->to) {
                tracker->m_pendingImages.erase(std::next(it).base());
            } else if (updateGlobalLayoutCache)
                current = newState;
            return;
        }
    }
    tracker->m_pendingImages.push_back(
        { image, aspectMask, baseMipLevel, levelCount, baseArrayLayer, arrayLayerCount, fromState, newState });
    if (updateGlobalLayoutCache)
        current = newState;
}

void BarrierTrackerVK::FlushBarriers(BarrierTrackerVK* tracker, CommandListVK* cmdList) {
    if (!tracker || !cmdList) return;
    void* cmdBuf = cmdList->GetCommandBuffer();
    if (!cmdBuf) return;
    VkCommandBuffer cmd = static_cast<VkCommandBuffer>(cmdBuf);

    VkPipelineStageFlags2 srcStage = 0;
    VkPipelineStageFlags2 dstStage = 0;
    std::vector<VkBufferMemoryBarrier2> bufBarriers;
    std::vector<VkImageMemoryBarrier2> imgBarriers;

    for (const auto& p : tracker->m_pendingBuffers) {
        VkBufferMemoryBarrier2 b = {};
        b.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
        b.srcStageMask = ToVkPipelineStage2(p.from);
        b.srcAccessMask = ToVkAccessFlags2(p.from);
        b.dstStageMask = ToVkPipelineStage2(p.to);
        b.dstAccessMask = ToVkAccessFlags2(p.to);
        b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.buffer = static_cast<VkBuffer>(p.buffer);
        b.offset = 0;
        b.size = VK_WHOLE_SIZE;
        bufBarriers.push_back(b);
        srcStage |= b.srcStageMask;
        dstStage |= b.dstStageMask;
    }
    for (const auto& p : tracker->m_pendingImages) {
        const bool isDepthAspect = (p.aspectMask & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) != 0;
        auto layoutFor = [isDepthAspect](ResourceState state) -> VkImageLayout {
            VkImageLayout L = ToVkImageLayout(state);
            if (isDepthAspect && state == ResourceState::RenderTarget)
                return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            return L;
        };
        VkImageMemoryBarrier2 b = {};
        b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        b.srcStageMask = ToVkPipelineStage2(p.from);
        b.srcAccessMask = ToVkAccessFlags2(p.from);
        b.dstStageMask = ToVkPipelineStage2(p.to);
        b.dstAccessMask = ToVkAccessFlags2(p.to);
        b.oldLayout = layoutFor(p.from);
        b.newLayout = layoutFor(p.to);
        b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.image = static_cast<VkImage>(p.image);
        b.subresourceRange.aspectMask = static_cast<VkImageAspectFlags>(p.aspectMask);
        b.subresourceRange.baseMipLevel = p.baseMipLevel;
        b.subresourceRange.levelCount = p.levelCount;
        b.subresourceRange.baseArrayLayer = p.baseArrayLayer;
        b.subresourceRange.layerCount = p.arrayLayerCount;
        imgBarriers.push_back(b);
        srcStage |= b.srcStageMask;
        dstStage |= b.dstStageMask;
    }

    if (srcStage == 0) srcStage = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
    if (dstStage == 0) dstStage = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;

    if (!bufBarriers.empty() || !imgBarriers.empty()) {
        VkMemoryBarrier2 global = {};
        global.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
        global.srcStageMask = srcStage;
        global.dstStageMask = dstStage;
        VkDependencyInfo dep = {};
        dep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        dep.memoryBarrierCount = 1;
        dep.pMemoryBarriers = &global;
        dep.bufferMemoryBarrierCount = static_cast<uint32_t>(bufBarriers.size());
        dep.pBufferMemoryBarriers = bufBarriers.data();
        dep.imageMemoryBarrierCount = static_cast<uint32_t>(imgBarriers.size());
        dep.pImageMemoryBarriers = imgBarriers.data();
        vkCmdPipelineBarrier2(cmd, &dep);
    }

    tracker->m_pendingBuffers.clear();
    tracker->m_pendingImages.clear();
}

ResourceState BarrierTrackerVK::GetImageState(const BarrierTrackerVK* tracker, void* image) {
    if (!tracker || !image) return ResourceState::Undefined;
    auto it = tracker->m_imageState.find(image);
    return it == tracker->m_imageState.end() ? ResourceState::Undefined : it->second;
}

void BarrierTrackerVK::SetImageState(BarrierTrackerVK* tracker, void* image, ResourceState state) {
    if (!tracker || !image) return;
    tracker->m_imageState[image] = state;
}

void BarrierTrackerVK::RemoveImageState(BarrierTrackerVK* tracker, void* image) {
    if (!tracker || !image) return;
    tracker->m_imageState.erase(image);
}

} // namespace rhi
} // namespace rendering
} // namespace eyelib
