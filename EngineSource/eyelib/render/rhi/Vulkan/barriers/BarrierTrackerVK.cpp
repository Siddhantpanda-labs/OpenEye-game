#include "eyelib/render/rhi/Vulkan/barriers/BarrierTrackerVK.h"
#include "eyelib/render/rhi/Vulkan/commands/CommandListVK.h"
#include "eyelib/render/rhi/Vulkan/utils/VulkanEnums.h"
#include <vulkan/vulkan.h>

namespace eyelib {
namespace render {
namespace rhi {

void BarrierTrackerVK::Reset(BarrierTrackerVK* tracker) {
    if (!tracker) return;
    tracker->m_bufferState.clear();
    tracker->m_imageState.clear();
    tracker->m_pendingBuffers.clear();
    tracker->m_pendingImages.clear();
}

void BarrierTrackerVK::TransitionBuffer(BarrierTrackerVK* tracker, void* buffer, ResourceState newState) {
    if (!tracker || !buffer) return;
    ResourceState& current = tracker->m_bufferState[buffer];
    if (current == newState) return;
    tracker->m_pendingBuffers.push_back({ buffer, current, newState });
    current = newState;
}

void BarrierTrackerVK::TransitionImage(BarrierTrackerVK* tracker, void* image, uint32_t aspectMask, ResourceState newState) {
    if (!tracker || !image) return;
    ResourceState& current = tracker->m_imageState[image];
    if (current == newState) return;
    tracker->m_pendingImages.push_back({ image, aspectMask, current, newState });
    current = newState;
}

void BarrierTrackerVK::FlushBarriers(BarrierTrackerVK* tracker, CommandListVK* cmdList) {
    if (!tracker || !cmdList) return;
    void* cmdBuf = cmdList->GetCommandBuffer();
    if (!cmdBuf) return;
    VkCommandBuffer cmd = static_cast<VkCommandBuffer>(cmdBuf);

    VkPipelineStageFlags srcStage = 0;
    VkPipelineStageFlags dstStage = 0;
    std::vector<VkBufferMemoryBarrier> bufBarriers;
    std::vector<VkImageMemoryBarrier> imgBarriers;

    for (const auto& p : tracker->m_pendingBuffers) {
        VkBufferMemoryBarrier b = {};
        b.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        b.srcAccessMask = ToVkAccessFlags(p.from);
        b.dstAccessMask = ToVkAccessFlags(p.to);
        b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.buffer = static_cast<VkBuffer>(p.buffer);
        b.offset = 0;
        b.size = VK_WHOLE_SIZE;
        bufBarriers.push_back(b);
        srcStage |= ToVkPipelineStage(p.from);
        dstStage |= ToVkPipelineStage(p.to);
    }
    for (const auto& p : tracker->m_pendingImages) {
        const bool isDepthAspect = (p.aspectMask & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) != 0;
        auto layoutFor = [isDepthAspect](ResourceState state) -> VkImageLayout {
            VkImageLayout L = ToVkImageLayout(state);
            if (isDepthAspect && state == ResourceState::RenderTarget)
                return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            return L;
        };
        VkImageMemoryBarrier b = {};
        b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        b.srcAccessMask = ToVkAccessFlags(p.from);
        b.dstAccessMask = ToVkAccessFlags(p.to);
        b.oldLayout = layoutFor(p.from);
        b.newLayout = layoutFor(p.to);
        b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.image = static_cast<VkImage>(p.image);
        b.subresourceRange.aspectMask = static_cast<VkImageAspectFlags>(p.aspectMask);
        b.subresourceRange.baseMipLevel = 0;
        b.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
        b.subresourceRange.baseArrayLayer = 0;
        b.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
        imgBarriers.push_back(b);
        srcStage |= ToVkPipelineStage(p.from);
        dstStage |= ToVkPipelineStage(p.to);
    }

    if (srcStage == 0) srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    if (dstStage == 0) dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

    if (!bufBarriers.empty() || !imgBarriers.empty()) {
        vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0,
            0, nullptr,
            static_cast<uint32_t>(bufBarriers.size()), bufBarriers.data(),
            static_cast<uint32_t>(imgBarriers.size()), imgBarriers.data());
    }

    tracker->m_pendingBuffers.clear();
    tracker->m_pendingImages.clear();
}

} // namespace rhi
} // namespace render
} // namespace eyelib
