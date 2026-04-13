#include "eyelib/rendering/rhi/Vulkan/descriptors/DescriptorCacheVK.h"
#include "eyelib/rendering/rhi/Vulkan/core/DeviceVK.h"
#include "eyelib/rendering/rhi/Vulkan/utils/DebugUtilsVK.h"
#include "eyelib/core/Log.h"
#include <vulkan/vulkan.h>

namespace eyelib {
namespace rendering {
namespace rhi {

bool DescriptorCacheVK::Create(DescriptorCacheVK* out, void* device, const void* poolSizes, uint32_t poolSizeCount, uint32_t maxSetsPerPool) {
    if (!out || !device || !poolSizes || poolSizeCount == 0) return false;
    VkDevice dev = static_cast<VkDevice>(device);
    const VkDescriptorPoolSize* sizes = static_cast<const VkDescriptorPoolSize*>(poolSizes);
    VkDescriptorPoolCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;  // allow reset and reuse
    info.maxSets = maxSetsPerPool;
    info.poolSizeCount = poolSizeCount;
    info.pPoolSizes = sizes;
    for (uint32_t i = 0; i < kMaxFrames; ++i) {
        VkDescriptorPool pool = VK_NULL_HANDLE;
        VkResult res = vkCreateDescriptorPool(dev, &info, nullptr, &pool);
        if (res != VK_SUCCESS) {
            core::Log::ErrorF("DescriptorCacheVK::Create: vkCreateDescriptorPool frame %u failed (0x%x)", i, res);
            for (uint32_t j = 0; j < i; ++j) {
                vkDestroyDescriptorPool(dev, static_cast<VkDescriptorPool>(out->m_perFrame[j].pool), nullptr);
                out->m_perFrame[j].pool = nullptr;
            }
            return false;
        }
        out->m_perFrame[i].pool = pool;
    }
    out->m_poolSizes = poolSizes;
    out->m_poolSizeCount = poolSizeCount;
    out->m_maxSetsPerPool = maxSetsPerPool;
    return true;
}

void DescriptorCacheVK::Destroy(DescriptorCacheVK* cache, void* device) {
    if (!cache || !device) return;
    VkDevice dev = static_cast<VkDevice>(device);
    for (uint32_t i = 0; i < kMaxFrames; ++i) {
        FrameCache& fc = cache->m_perFrame[i];
        fc.hashToSet.clear();
        if (fc.pool) {
            vkDestroyDescriptorPool(dev, static_cast<VkDescriptorPool>(fc.pool), nullptr);
            fc.pool = nullptr;
        }
        for (void* p : fc.extraPools) {
            if (p) vkDestroyDescriptorPool(dev, static_cast<VkDescriptorPool>(p), nullptr);
        }
        fc.extraPools.clear();
    }
    cache->m_poolSizes = nullptr;
    cache->m_poolSizeCount = 0;
    cache->m_maxSetsPerPool = 0;
}

void DescriptorCacheVK::Reset(DescriptorCacheVK* cache, void* device, uint32_t frameIndex) {
    if (!cache || !device || frameIndex >= kMaxFrames) return;
    VkDevice dev = static_cast<VkDevice>(device);
    FrameCache& fc = cache->m_perFrame[frameIndex];
    fc.hashToSet.clear();
    if (fc.pool)
        vkResetDescriptorPool(dev, static_cast<VkDescriptorPool>(fc.pool), 0);
    for (void* p : fc.extraPools) {
        if (p) vkResetDescriptorPool(dev, static_cast<VkDescriptorPool>(p), 0);
    }
}

bool DescriptorCacheVK::GetOrCreate(DescriptorCacheVK* cache, void* device, uint32_t frameIndex, void* layout, uint64_t layoutHash, void** outSet,
                                    const char* debugName, DeviceVK* deviceVKForName) {
    if (!cache || !device || !outSet || !layout || frameIndex >= kMaxFrames) return false;
    FrameCache& fc = cache->m_perFrame[frameIndex];
    auto it = fc.hashToSet.find(layoutHash);
    if (it != fc.hashToSet.end()) {
        *outSet = it->second;
        return true;
    }
    VkDevice dev = static_cast<VkDevice>(device);
    VkDescriptorSetLayout setLayout = static_cast<VkDescriptorSetLayout>(layout);
    const VkDescriptorPoolSize* sizes = static_cast<const VkDescriptorPoolSize*>(cache->m_poolSizes);
    if (!sizes) return false;

    auto tryAllocate = [&](VkDescriptorPool pool) -> bool {
        VkDescriptorSetAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = pool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &setLayout;
        VkDescriptorSet set = VK_NULL_HANDLE;
        VkResult res = vkAllocateDescriptorSets(dev, &allocInfo, &set);
        if (res == VK_SUCCESS) {
            fc.hashToSet[layoutHash] = set;
            *outSet = set;
            if (debugName && deviceVKForName)
                SetDebugNameDescriptorSet(deviceVKForName, set, debugName);
            return true;
        }
        return false;
    };

    if (fc.pool && tryAllocate(static_cast<VkDescriptorPool>(fc.pool)))
        return true;
    for (void* p : fc.extraPools) {
        if (p && tryAllocate(static_cast<VkDescriptorPool>(p)))
            return true;
    }
    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = cache->m_maxSetsPerPool;
    poolInfo.poolSizeCount = cache->m_poolSizeCount;
    poolInfo.pPoolSizes = sizes;
    VkDescriptorPool newPool = VK_NULL_HANDLE;
    VkResult res = vkCreateDescriptorPool(dev, &poolInfo, nullptr, &newPool);
    if (res != VK_SUCCESS) {
        core::Log::ErrorF("DescriptorCacheVK::GetOrCreate: vkCreateDescriptorPool (grow) failed (0x%x)", res);
        return false;
    }
    fc.extraPools.push_back(newPool);
    if (!tryAllocate(newPool)) {
        core::Log::ErrorF("DescriptorCacheVK::GetOrCreate: vkAllocateDescriptorSets (new pool) failed");
        vkDestroyDescriptorPool(dev, newPool, nullptr);
        fc.extraPools.pop_back();
        return false;
    }
    return true;
}

} // namespace rhi
} // namespace rendering
} // namespace eyelib
