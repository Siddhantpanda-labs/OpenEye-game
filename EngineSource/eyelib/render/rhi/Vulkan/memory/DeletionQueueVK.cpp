#include "eyelib/render/rhi/Vulkan/memory/DeletionQueueVK.h"
#include "eyelib/render/rhi/Vulkan/memory/vk_mem_alloc.h"
#include <vulkan/vulkan.h>

namespace eyelib {
namespace render {
namespace rhi {

void DeletionQueueVK::EnqueueBuffer(uint32_t frameIndex, void* allocator, void* buffer, void* allocation) {
    (void)allocator;
    if (frameIndex >= kMaxFrames || !buffer) return;
    m_perFrame[frameIndex].buffers.emplace_back(buffer, allocation);
}

void DeletionQueueVK::EnqueueImage(uint32_t frameIndex, void* allocator, void* image, void* allocation) {
    (void)allocator;
    if (frameIndex >= kMaxFrames || !image) return;
    m_perFrame[frameIndex].images.emplace_back(image, allocation);
}

void DeletionQueueVK::EnqueueImageView(uint32_t frameIndex, void* device, void* imageView) {
    (void)device;
    if (frameIndex >= kMaxFrames || !imageView) return;
    m_perFrame[frameIndex].imageViews.push_back(imageView);
}

void DeletionQueueVK::EnqueuePipeline(uint32_t frameIndex, void* device, void* pipeline) {
    (void)device;
    if (frameIndex >= kMaxFrames || !pipeline) return;
    m_perFrame[frameIndex].pipelines.push_back(pipeline);
}

void DeletionQueueVK::EnqueuePipelineLayout(uint32_t frameIndex, void* device, void* pipelineLayout) {
    (void)device;
    if (frameIndex >= kMaxFrames || !pipelineLayout) return;
    m_perFrame[frameIndex].pipelineLayouts.push_back(pipelineLayout);
}

void DeletionQueueVK::EnqueueDescriptorSetLayout(uint32_t frameIndex, void* device, void* descriptorSetLayout) {
    (void)device;
    if (frameIndex >= kMaxFrames || !descriptorSetLayout) return;
    m_perFrame[frameIndex].descriptorSetLayouts.push_back(descriptorSetLayout);
}

void DeletionQueueVK::EnqueueFramebuffer(uint32_t frameIndex, void* device, void* framebuffer) {
    (void)device;
    if (frameIndex >= kMaxFrames || !framebuffer) return;
    m_perFrame[frameIndex].framebuffers.push_back(framebuffer);
}

void DeletionQueueVK::EnqueueSampler(uint32_t frameIndex, void* device, void* sampler) {
    (void)device;
    if (frameIndex >= kMaxFrames || !sampler) return;
    m_perFrame[frameIndex].samplers.push_back(sampler);
}

void DeletionQueueVK::Tick(void* device, void* allocator, uint32_t frameIndex, std::set<void*>* alreadyDestroyedPipelines) {
    if (frameIndex >= kMaxFrames) return;
    VkDevice dev = device ? static_cast<VkDevice>(device) : VK_NULL_HANDLE;
    VmaAllocator vma = allocator ? static_cast<VmaAllocator>(allocator) : nullptr;
    FrameDeletions& d = m_perFrame[frameIndex];

    for (auto& p : d.buffers) {
        VkBuffer buf = static_cast<VkBuffer>(p.first);
        VmaAllocation alloc = static_cast<VmaAllocation>(p.second);
        if (vma && buf != VK_NULL_HANDLE)
            vmaDestroyBuffer(vma, buf, alloc);
    }
    d.buffers.clear();

    // Destroy dependent objects before their parents:
    // framebuffer -> image view -> image.
    for (void* p : d.framebuffers) {
        if (dev && p)
            vkDestroyFramebuffer(dev, static_cast<VkFramebuffer>(p), nullptr);
    }
    d.framebuffers.clear();

    for (void* p : d.imageViews) {
        if (dev && p)
            vkDestroyImageView(dev, static_cast<VkImageView>(p), nullptr);
    }
    d.imageViews.clear();

    for (auto& p : d.images) {
        VkImage img = static_cast<VkImage>(p.first);
        VmaAllocation alloc = static_cast<VmaAllocation>(p.second);
        if (vma && img != VK_NULL_HANDLE)
            vmaDestroyImage(vma, img, alloc);
    }
    d.images.clear();

    for (void* p : d.pipelines) {
        if (!dev || !p) continue;
        if (alreadyDestroyedPipelines) {
            if (alreadyDestroyedPipelines->count(p)) continue;
            alreadyDestroyedPipelines->insert(p);
        }
        vkDestroyPipeline(dev, static_cast<VkPipeline>(p), nullptr);
    }
    d.pipelines.clear();

    for (void* p : d.pipelineLayouts) {
        if (dev && p)
            vkDestroyPipelineLayout(dev, static_cast<VkPipelineLayout>(p), nullptr);
    }
    d.pipelineLayouts.clear();

    for (void* p : d.descriptorSetLayouts) {
        if (dev && p)
            vkDestroyDescriptorSetLayout(dev, static_cast<VkDescriptorSetLayout>(p), nullptr);
    }
    d.descriptorSetLayouts.clear();

    for (void* p : d.samplers) {
        if (dev && p)
            vkDestroySampler(dev, static_cast<VkSampler>(p), nullptr);
    }
    d.samplers.clear();
}

} // namespace rhi
} // namespace render
} // namespace eyelib
