#include "eyelib/render/rhi/Vulkan/descriptors/DescriptorAllocatorVK.h"
#include "eyelib/render/rhi/Vulkan/core/DeviceVK.h"
#include "eyelib/render/rhi/Vulkan/utils/DebugUtilsVK.h"
#include "eyelib/core/Log.h"
#include <vulkan/vulkan.h>
#include <cstring>

namespace eyelib {
namespace render {
namespace rhi {

bool DescriptorAllocatorVK::Create(DescriptorAllocatorVK* out, void* device, const void* poolSizes, uint32_t poolSizeCount, uint32_t maxSetsPerPool) {
    if (!out || !device || !poolSizes || poolSizeCount == 0) return false;
    VkDevice dev = static_cast<VkDevice>(device);
    const VkDescriptorPoolSize* sizes = static_cast<const VkDescriptorPoolSize*>(poolSizes);
    for (uint32_t i = 0; i < kMaxFrames; ++i) {
        VkDescriptorPoolCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        info.maxSets = maxSetsPerPool;
        info.poolSizeCount = poolSizeCount;
        info.pPoolSizes = sizes;
        VkDescriptorPool pool = VK_NULL_HANDLE;
        VkResult res = vkCreateDescriptorPool(dev, &info, nullptr, &pool);
        if (res != VK_SUCCESS) {
            core::Log::ErrorF("DescriptorAllocatorVK::Create: vkCreateDescriptorPool frame %u failed (0x%x)", i, res);
            for (uint32_t j = 0; j < i; ++j)
                vkDestroyDescriptorPool(dev, static_cast<VkDescriptorPool>(out->m_pools[j]), nullptr);
            return false;
        }
        out->m_pools[i] = pool;
    }
    return true;
}

void DescriptorAllocatorVK::Destroy(DescriptorAllocatorVK* alloc, void* device) {
    if (!alloc || !device) return;
    VkDevice dev = static_cast<VkDevice>(device);
    for (uint32_t i = 0; i < kMaxFrames; ++i) {
        if (alloc->m_pools[i]) {
            vkDestroyDescriptorPool(dev, static_cast<VkDescriptorPool>(alloc->m_pools[i]), nullptr);
            alloc->m_pools[i] = nullptr;
        }
    }
}

void DescriptorAllocatorVK::Reset(DescriptorAllocatorVK* alloc, void* device, uint32_t frameIndex) {
    if (!alloc || !device || frameIndex >= kMaxFrames) return;
    if (!alloc->m_pools[frameIndex]) return;
    VkDevice dev = static_cast<VkDevice>(device);
    vkResetDescriptorPool(dev, static_cast<VkDescriptorPool>(alloc->m_pools[frameIndex]), 0);
}

bool DescriptorAllocatorVK::Allocate(DescriptorAllocatorVK* alloc, void* device, uint32_t frameIndex, void* layout, void** outSet,
                                     const char* debugName, DeviceVK* deviceVKForName) {
    if (!alloc || !device || !outSet || frameIndex >= kMaxFrames || !alloc->m_pools[frameIndex] || !layout) return false;
    VkDevice dev = static_cast<VkDevice>(device);
    VkDescriptorSetLayout setLayout = static_cast<VkDescriptorSetLayout>(layout);
    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = static_cast<VkDescriptorPool>(alloc->m_pools[frameIndex]);
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &setLayout;
    VkDescriptorSet set = VK_NULL_HANDLE;
    VkResult res = vkAllocateDescriptorSets(dev, &allocInfo, &set);
    if (res != VK_SUCCESS) {
        core::Log::ErrorF("DescriptorAllocatorVK::Allocate failed (0x%x)", res);
        return false;
    }
    *outSet = set;
    if (debugName && deviceVKForName)
        SetDebugNameDescriptorSet(deviceVKForName, set, debugName);
    return true;
}

} // namespace rhi
} // namespace render
} // namespace eyelib
