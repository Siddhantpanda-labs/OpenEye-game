#pragma once

#include <array>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace eyelib {
namespace render {
namespace rhi {

struct DeviceVK;

/** Per-frame descriptor set cache: layoutHash -> VkDescriptorSet per frame. Safe across frame overlap; no use-after-free. */
struct DescriptorCacheVK {
    static constexpr uint32_t kMaxFrames = 3;

    /**
     * Create kMaxFrames pools and per-frame maps. poolSizes/maxSetsPerPool as for DescriptorAllocatorVK.
     * Caller must keep poolSizes valid until Destroy.
     */
    static bool Create(DescriptorCacheVK* out, void* device, const void* poolSizes, uint32_t poolSizeCount, uint32_t maxSetsPerPool);
    static void Destroy(DescriptorCacheVK* cache, void* device);

    /** Reset the pool and clear the cache for this frame. Call at start of frame (after fence wait). */
    static void Reset(DescriptorCacheVK* cache, void* device, uint32_t frameIndex);

    /**
     * Get or allocate a descriptor set for the given layout in this frame. layout is VkDescriptorSetLayout; layoutHash is the cache key.
     * frameIndex must be the current frame. Returns true and sets *outSet (VkDescriptorSet); false on failure.
     */
    static bool GetOrCreate(DescriptorCacheVK* cache, void* device, uint32_t frameIndex, void* layout, uint64_t layoutHash, void** outSet,
                            const char* debugName = nullptr, DeviceVK* deviceVKForName = nullptr);

private:
    struct FrameCache {
        void* pool = nullptr;  // VkDescriptorPool
        std::unordered_map<uint64_t, void*> hashToSet;  // layoutHash -> VkDescriptorSet
        std::vector<void*> extraPools;  // grow when pool full
    };
    std::array<FrameCache, kMaxFrames> m_perFrame = {};
    const void* m_poolSizes = nullptr;
    uint32_t m_poolSizeCount = 0;
    uint32_t m_maxSetsPerPool = 0;
};

} // namespace rhi
} // namespace render
} // namespace eyelib
