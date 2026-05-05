#pragma once

#include <cstdint>

namespace eyelib {
namespace render {
namespace rhi {

struct DeviceVK;

/** Per-frame descriptor pools. Reset the current frame's pool each BeginFrame; allocate transient sets from it. */
struct DescriptorAllocatorVK {
    static constexpr uint32_t kMaxFrames = 3;

    /**
     * Create kMaxFrames descriptor pools. poolSizes is pointer to VkDescriptorPoolSize array; poolSizeCount its length.
     * maxSetsPerPool is the max sets per pool.
     */
    static bool Create(DescriptorAllocatorVK* out, void* device, const void* poolSizes, uint32_t poolSizeCount, uint32_t maxSetsPerPool);
    static void Destroy(DescriptorAllocatorVK* alloc, void* device);

    /** Reset the pool for this frame (call at start of frame after fence wait). */
    static void Reset(DescriptorAllocatorVK* alloc, void* device, uint32_t frameIndex);

    /** Allocate a descriptor set from the pool for this frame. layout is VkDescriptorSetLayout. Returns true and sets *outSet (VkDescriptorSet). */
    static bool Allocate(DescriptorAllocatorVK* alloc, void* device, uint32_t frameIndex, void* layout, void** outSet,
                         const char* debugName = nullptr, DeviceVK* deviceVKForName = nullptr);

private:
    void* m_pools[kMaxFrames] = {};  // VkDescriptorPool per frame
};

} // namespace rhi
} // namespace render
} // namespace eyelib
