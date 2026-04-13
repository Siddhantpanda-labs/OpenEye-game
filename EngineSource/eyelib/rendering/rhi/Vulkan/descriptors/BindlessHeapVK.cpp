#include "eyelib/rendering/rhi/Vulkan/descriptors/BindlessHeapVK.h"
#include "eyelib/rendering/rhi/Vulkan/core/DeviceVK.h"
#include "eyelib/rendering/rhi/Vulkan/descriptors/DescriptorSetVK.h"
#include "eyelib/rendering/rhi/Vulkan/pipelines/PipelineLayoutVK.h"
#include "eyelib/rendering/rhi/Vulkan/resources/TextureVK.h"
#include "eyelib/rendering/rhi/Vulkan/resources/BufferVK.h"
#include "eyelib/core/Log.h"
#include <vulkan/vulkan.h>

namespace eyelib {
namespace rendering {
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
    VkResult poolRes = vkCreateDescriptorPool(vkDevice, &poolInfo, nullptr, &pool);
    if (poolRes != VK_SUCCESS) {
        core::Log::ErrorF("BindlessHeapVK: vkCreateDescriptorPool failed (0x%x)", static_cast<unsigned>(poolRes));
        return;
    }
    m_descriptorPool = pool;
    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;
    VkDescriptorSet set = VK_NULL_HANDLE;
    VkResult allocRes = vkAllocateDescriptorSets(vkDevice, &allocInfo, &set);
    if (allocRes != VK_SUCCESS) {
        core::Log::ErrorF("BindlessHeapVK: vkAllocateDescriptorSets failed (0x%x)", static_cast<unsigned>(allocRes));
        vkDestroyDescriptorPool(vkDevice, pool, nullptr);
        m_descriptorPool = nullptr;
        return;
    }
    m_descriptorSet = set;
}

BindlessHeapVK::~BindlessHeapVK() {
    if (m_descriptorSetWrapper && m_device) {
        DestroyDescriptorSetVK(m_device, m_descriptorSetWrapper);
        m_descriptorSetWrapper = nullptr;
    }
    if (!m_device || !m_descriptorPool) return;
    VkDevice device = static_cast<VkDevice>(m_device->GetDevice());
    vkDestroyDescriptorPool(device, static_cast<VkDescriptorPool>(m_descriptorPool), nullptr);
    m_descriptorPool = nullptr;
    m_descriptorSet = nullptr;
}

IDescriptorSet* BindlessHeapVK::GetDescriptorSet() {
    if (!m_descriptorSet || !m_device) return nullptr;
    if (!m_descriptorSetWrapper) {
        IPipelineLayout* layout = m_device->GetBindlessPipelineLayout();
        if (layout)
            m_descriptorSetWrapper = CreateDescriptorSetWrapperVK(m_device, layout, m_descriptorSet);
    }
    return m_descriptorSetWrapper;
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
} // namespace rendering
} // namespace eyelib
