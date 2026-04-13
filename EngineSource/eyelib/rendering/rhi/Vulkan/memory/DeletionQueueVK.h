#pragma once

#include <array>
#include <cstdint>
#include <set>
#include <vector>

namespace eyelib {
namespace rendering {
namespace rhi {

/** Per-frame deferred destruction. Resources enqueued for frame F are destroyed when Tick(device, allocator, F) is called
 *  **after** that frame's fence has been waited (see DeviceVK::BeginFrame). VMA-backed buffers/images are freed with vmaDestroy*;
 *  Tick destroys **framebuffer → image view → image** so dependents are torn down before images. */
struct DeletionQueueVK {
    static constexpr uint32_t kMaxFrames = 3;

    struct FrameDeletions {
        std::vector<std::pair<void*, void*>> buffers;   // (VkBuffer, VmaAllocation)
        std::vector<std::pair<void*, void*>> images;    // (VkImage, VmaAllocation)
        std::vector<void*> imageViews;                   // VkImageView
        std::vector<void*> pipelines;                    // VkPipeline
        std::vector<void*> pipelineLayouts;              // VkPipelineLayout
        std::vector<void*> descriptorSetLayouts;        // VkDescriptorSetLayout
        std::vector<void*> framebuffers;                 // VkFramebuffer
        std::vector<void*> samplers;                    // VkSampler
    };
    std::array<FrameDeletions, kMaxFrames> m_perFrame = {};

    /** Enqueue for given frame index (use device current frame when destroying). */
    void EnqueueBuffer(uint32_t frameIndex, void* allocator, void* buffer, void* allocation);
    void EnqueueImage(uint32_t frameIndex, void* allocator, void* image, void* allocation);
    void EnqueueImageView(uint32_t frameIndex, void* device, void* imageView);
    void EnqueuePipeline(uint32_t frameIndex, void* device, void* pipeline);
    void EnqueuePipelineLayout(uint32_t frameIndex, void* device, void* pipelineLayout);
    void EnqueueDescriptorSetLayout(uint32_t frameIndex, void* device, void* descriptorSetLayout);
    void EnqueueFramebuffer(uint32_t frameIndex, void* device, void* framebuffer);
    void EnqueueSampler(uint32_t frameIndex, void* device, void* sampler);

    /** Process and clear the queue for this frame index. Call after waiting that frame's fence (e.g. in BeginFrame).
     *  If alreadyDestroyedPipelines is non-null, each pipeline handle is only destroyed once across multiple Tick calls (used on shutdown when same cached pipeline can be enqueued by multiple owners). */
    void Tick(void* device, void* allocator, uint32_t frameIndex, std::set<void*>* alreadyDestroyedPipelines = nullptr);
};

} // namespace rhi
} // namespace rendering
} // namespace eyelib
