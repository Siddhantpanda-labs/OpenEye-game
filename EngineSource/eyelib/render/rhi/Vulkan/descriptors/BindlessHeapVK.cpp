#include "eyelib/render/rhi/Vulkan/descriptors/BindlessHeapVK.h"
#include "eyelib/render/rhi/Vulkan/core/DeviceVK.h"
#include "eyelib/render/rhi/Vulkan/resources/TextureVK.h"
#include "eyelib/render/rhi/Vulkan/resources/BufferVK.h"
#include <vulkan/vulkan.h>

namespace eyelib {
namespace render {
namespace rhi {

BindlessHeapVK::BindlessHeapVK(DeviceVK* device) : m_device(device) {
    if (!device) return;
    VkDescriptorSetLayout layout = static_cast<VkDescriptorSetLayout>(device->GetBindlessSetLayout());
    if (layout == VK_NULL_HANDLE) return;
    VkDevice vkDevice = static_cast<VkDevice>(device->GetDevice());
    VkDescriptorPoolSize poolSizes[2] = {
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, kMaxTextures },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, kMaxBuffers },
    };
    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT_EXT;
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = 2;
    poolInfo.pPoolSizes = poolSizes;
    VkDescriptorPool pool = VK_NULL_HANDLE;
    if (vkCreateDescriptorPool(vkDevice, &poolInfo, nullptr, &pool) != VK_SUCCESS) return;
    m_descriptorPool = pool;
    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;
    VkDescriptorSet set = VK_NULL_HANDLE;
    if (vkAllocateDescriptorSets(vkDevice, &allocInfo, &set) != VK_SUCCESS) {
        vkDestroyDescriptorPool(vkDevice, pool, nullptr);
        m_descriptorPool = nullptr;
        return;
    }
    m_descriptorSet = set;
}

BindlessHeapVK::~BindlessHeapVK() {
    if (!m_device || !m_descriptorPool) return;
    VkDevice device = static_cast<VkDevice>(m_device->GetDevice());
    vkDestroyDescriptorPool(device, static_cast<VkDescriptorPool>(m_descriptorPool), nullptr);
    m_descriptorPool = nullptr;
    m_descriptorSet = nullptr;
}

uint32_t BindlessHeapVK::allocTextureSlot() {
    if (!m_freeTextureSlots.empty()) {
        uint32_t slot = m_freeTextureSlots.back();
        m_freeTextureSlots.pop_back();
        return slot;
    }
    if (m_nextTextureIndex >= kMaxTextures) return kMaxTextures;  // invalid
    return m_nextTextureIndex++;
}

void BindlessHeapVK::freeTextureSlot(uint32_t index) {
    m_freeTextureSlots.push_back(index);
}

uint32_t BindlessHeapVK::allocBufferSlot() {
    if (!m_freeBufferSlots.empty()) {
        uint32_t slot = m_freeBufferSlots.back();
        m_freeBufferSlots.pop_back();
        return slot;
    }
    if (m_nextBufferIndex >= kMaxBuffers) return kMaxBuffers;
    return m_nextBufferIndex++;
}

void BindlessHeapVK::freeBufferSlot(uint32_t index) {
    m_freeBufferSlots.push_back(index);
}

BindlessHandle BindlessHeapVK::AllocateTextureHandle(ITexture* texture) {
    if (!texture || !m_descriptorSet || !m_device) return kInvalidBindlessHandle;
    TextureVK* tex = static_cast<TextureVK*>(texture);
    VkImageView imageView = tex->m_imageView ? static_cast<VkImageView>(tex->m_imageView) : VK_NULL_HANDLE;
    if (imageView == VK_NULL_HANDLE) return kInvalidBindlessHandle;
    VkSampler sampler = static_cast<VkSampler>(m_device->GetDefaultSampler());
    if (sampler == VK_NULL_HANDLE) return kInvalidBindlessHandle;

    uint32_t index = allocTextureSlot();
    if (index >= kMaxTextures) return kInvalidBindlessHandle;

    VkDescriptorImageInfo imageInfo = {};
    imageInfo.sampler = sampler;
    imageInfo.imageView = imageView;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkWriteDescriptorSet write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = static_cast<VkDescriptorSet>(m_descriptorSet);
    write.dstBinding = 0;
    write.dstArrayElement = index;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &imageInfo;
    vkUpdateDescriptorSets(static_cast<VkDevice>(m_device->GetDevice()), 1, &write, 0, nullptr);

    return static_cast<BindlessHandle>(index + 1u);
}

BindlessHandle BindlessHeapVK::AllocateBufferHandle(IBuffer* buffer) {
    if (!buffer || !m_descriptorSet || !m_device) return kInvalidBindlessHandle;
    BufferVK* buf = static_cast<BufferVK*>(buffer);
    VkBuffer vkBuf = buf->m_buffer ? static_cast<VkBuffer>(buf->m_buffer) : VK_NULL_HANDLE;
    if (vkBuf == VK_NULL_HANDLE) return kInvalidBindlessHandle;

    uint32_t index = allocBufferSlot();
    if (index >= kMaxBuffers) return kInvalidBindlessHandle;

    size_t range = buf->GetSize();
    VkDescriptorBufferInfo bufferInfo = {};
    bufferInfo.buffer = vkBuf;
    bufferInfo.offset = 0;
    bufferInfo.range = range;
    VkWriteDescriptorSet write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = static_cast<VkDescriptorSet>(m_descriptorSet);
    write.dstBinding = 1;
    write.dstArrayElement = index;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    write.pBufferInfo = &bufferInfo;
    vkUpdateDescriptorSets(static_cast<VkDevice>(m_device->GetDevice()), 1, &write, 0, nullptr);

    return static_cast<BindlessHandle>(index + 1u);
}

void BindlessHeapVK::FreeTextureHandle(BindlessHandle handle) {
    if (handle == kInvalidBindlessHandle || handle == 0) return;
    uint32_t index = static_cast<uint32_t>(handle - 1u);
    if (index >= kMaxTextures) return;
    freeTextureSlot(index);
}

void BindlessHeapVK::FreeBufferHandle(BindlessHandle handle) {
    if (handle == kInvalidBindlessHandle || handle == 0) return;
    uint32_t index = static_cast<uint32_t>(handle - 1u);
    if (index >= kMaxBuffers) return;
    freeBufferSlot(index);
}

} // namespace rhi
} // namespace render
} // namespace eyelib
