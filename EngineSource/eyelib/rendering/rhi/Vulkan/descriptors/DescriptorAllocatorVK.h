#pragma once

#include <cstdint>

namespace eyelib {
namespace rendering {
namespace rhi {

struct DeviceVK;

/** **Ring of `VkDescriptorPool`s** (`kMaxFrames`, aligned with frame-in-flight). Each frame: **`Reset`** after the GPU fence
 *  for that slot has completed, then **`Allocate`** transient sets. Pools use **`VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT`**
 *  so individual sets can be freed if needed; typical use is reset-the-whole-pool each frame (no fragmentation within a frame).
 *  **`vkAllocateDescriptorSets`** failures (e.g. **`VK_ERROR_OUT_OF_POOL_MEMORY`**) are logged — tune **`maxSetsPerPool`** / **`VkDescriptorPoolSize`** counts from **`DeviceVK`** setup. */
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
} // namespace rendering
} // namespace eyelib
