#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace eyelib {
namespace render {
namespace rhi {

/** Per-frame deferred destruction. Resources enqueued for frame F are destroyed when Tick(device, allocator, F) is called (after that frame's fence has been waited). */
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

    /** Process and clear the queue for this frame index. Call after waiting that frame's fence (e.g. in BeginFrame). */
    void Tick(void* device, void* allocator, uint32_t frameIndex);
};

} // namespace rhi
} // namespace render
} // namespace eyelib
