#include "eyelib/render/rhi/Vulkan/DescriptorSetVK.h"
#include "eyelib/render/rhi/Vulkan/DeviceVK.h"
#include "eyelib/render/rhi/Vulkan/TextureVK.h"
#include "eyelib/render/rhi/Vulkan/BufferVK.h"
#include "eyelib/render/rhi/Vulkan/VulkanEnums.h"
#include <vulkan/vulkan.h>

namespace eyelib {
namespace render {
namespace rhi {

DescriptorSetVK::DescriptorSetVK(DeviceVK* device, PipelineLayoutVK* layout)
    : m_device(device), m_layout(layout) {}

DescriptorSetVK::~DescriptorSetVK() = default;

void DescriptorSetVK::UpdateTexture(uint32_t binding, ITexture* texture) {
    if (!m_device || !m_descriptorSet || !m_layout) return;
    TextureVK* texVK = texture ? static_cast<TextureVK*>(texture) : nullptr;
    VkImageView imageView = texVK && texVK->m_imageView ? static_cast<VkImageView>(texVK->m_imageView) : VK_NULL_HANDLE;
    VkSampler sampler = static_cast<VkSampler>(m_device->GetDefaultSampler());
    if (sampler == VK_NULL_HANDLE) return;
    VkDescriptorImageInfo imageInfo = {};
    imageInfo.sampler = sampler;
    imageInfo.imageView = imageView;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkWriteDescriptorSet write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = static_cast<VkDescriptorSet>(m_descriptorSet);
    write.dstBinding = binding;
    write.dstArrayElement = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &imageInfo;
    vkUpdateDescriptorSets(static_cast<VkDevice>(m_device->GetDevice()), 1, &write, 0, nullptr);
}

void DescriptorSetVK::UpdateUniformBuffer(uint32_t binding, IBuffer* buffer, uint32_t offset, uint32_t size) {
    if (!m_device || !m_descriptorSet || !buffer) return;
    BufferVK* bufVK = static_cast<BufferVK*>(buffer);
    VkBuffer vkBuf = static_cast<VkBuffer>(bufVK->m_buffer);
    if (vkBuf == VK_NULL_HANDLE) return;
    size_t bufSize = bufVK->GetSize();
    if (size == 0) size = static_cast<uint32_t>(bufSize);
    VkDescriptorBufferInfo bufferInfo = {};
    bufferInfo.buffer = vkBuf;
    bufferInfo.offset = offset;
    bufferInfo.range = size;
    VkWriteDescriptorSet write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = static_cast<VkDescriptorSet>(m_descriptorSet);
    write.dstBinding = binding;
    write.dstArrayElement = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    write.pBufferInfo = &bufferInfo;
    vkUpdateDescriptorSets(static_cast<VkDevice>(m_device->GetDevice()), 1, &write, 0, nullptr);
}

void DescriptorSetVK::UpdateStorageBuffer(uint32_t binding, IBuffer* buffer, uint32_t offset, uint32_t size) {
    if (!m_device || !m_descriptorSet || !buffer) return;
    BufferVK* bufVK = static_cast<BufferVK*>(buffer);
    VkBuffer vkBuf = static_cast<VkBuffer>(bufVK->m_buffer);
    if (vkBuf == VK_NULL_HANDLE) return;
    size_t bufSize = bufVK->GetSize();
    if (size == 0) size = static_cast<uint32_t>(bufSize);
    VkDescriptorBufferInfo bufferInfo = {};
    bufferInfo.buffer = vkBuf;
    bufferInfo.offset = offset;
    bufferInfo.range = size;
    VkWriteDescriptorSet write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = static_cast<VkDescriptorSet>(m_descriptorSet);
    write.dstBinding = binding;
    write.dstArrayElement = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    write.pBufferInfo = &bufferInfo;
    vkUpdateDescriptorSets(static_cast<VkDevice>(m_device->GetDevice()), 1, &write, 0, nullptr);
}

} // namespace rhi
} // namespace render
} // namespace eyelib
