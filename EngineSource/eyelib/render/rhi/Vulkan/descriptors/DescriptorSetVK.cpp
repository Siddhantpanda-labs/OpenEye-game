#include "eyelib/render/rhi/Vulkan/descriptors/DescriptorSetVK.h"
#include "eyelib/render/rhi/Vulkan/core/DeviceVK.h"
#include "eyelib/render/rhi/Vulkan/resources/TextureVK.h"
#include "eyelib/render/rhi/Vulkan/resources/BufferVK.h"
#include "eyelib/render/rhi/Vulkan/utils/VulkanEnums.h"
#include "eyelib/core/Log.h"
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

IPipelineLayout* CreatePipelineLayoutVK(DeviceVK* deviceObj, const PipelineLayoutDesc& desc) {
    if (!deviceObj) return nullptr;
    VkDevice device = static_cast<VkDevice>(deviceObj->GetDevice());
    if (!device) return nullptr;

    std::vector<VkDescriptorSetLayoutBinding> layoutBindings(desc.bindings.size());
    for (size_t i = 0; i < desc.bindings.size(); ++i) {
        const BindingDesc& b = desc.bindings[i];
        layoutBindings[i].binding = b.binding;
        layoutBindings[i].descriptorType = ToVkDescriptorType(b.type);
        layoutBindings[i].descriptorCount = 1;
        layoutBindings[i].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
        layoutBindings[i].pImmutableSamplers = nullptr;
    }
    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(layoutBindings.size());
    layoutInfo.pBindings = layoutBindings.data();

    VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
    VkResult res = vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &setLayout);
    if (res != VK_SUCCESS) {
        core::Log::ErrorF("CreatePipelineLayoutVK: vkCreateDescriptorSetLayout failed (0x%x)", res);
        return nullptr;
    }

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &setLayout;

    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    res = vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout);
    if (res != VK_SUCCESS) {
        vkDestroyDescriptorSetLayout(device, setLayout, nullptr);
        core::Log::ErrorF("CreatePipelineLayoutVK: vkCreatePipelineLayout failed (0x%x)", res);
        return nullptr;
    }

    auto* pl = new PipelineLayoutVK();
    pl->m_desc = desc;
    pl->m_descriptorSetLayout = setLayout;
    pl->m_pipelineLayout = pipelineLayout;
    return pl;
}

void DestroyPipelineLayoutVK(DeviceVK* deviceObj, IPipelineLayout* layout) {
    if (!deviceObj || !layout) return;
    PipelineLayoutVK* pl = static_cast<PipelineLayoutVK*>(layout);
    VkDevice device = static_cast<VkDevice>(deviceObj->GetDevice());
    if (device) {
        if (pl->m_pipelineLayout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device, static_cast<VkPipelineLayout>(pl->m_pipelineLayout), nullptr);
            pl->m_pipelineLayout = nullptr;
        }
        if (pl->m_descriptorSetLayout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(device, static_cast<VkDescriptorSetLayout>(pl->m_descriptorSetLayout), nullptr);
            pl->m_descriptorSetLayout = nullptr;
        }
    }
    delete pl;
}

IDescriptorSet* CreateDescriptorSetVK(DeviceVK* deviceObj, IPipelineLayout* layout) {
    if (!deviceObj || !layout) return nullptr;
    PipelineLayoutVK* pl = static_cast<PipelineLayoutVK*>(layout);
    VkDevice device = static_cast<VkDevice>(deviceObj->GetDevice());
    VkDescriptorPool descriptorPool = static_cast<VkDescriptorPool>(deviceObj->GetDescriptorPool());
    if (!pl || device == VK_NULL_HANDLE || descriptorPool == VK_NULL_HANDLE || pl->m_descriptorSetLayout == VK_NULL_HANDLE) return nullptr;
    VkDescriptorSetLayout setLayout = static_cast<VkDescriptorSetLayout>(pl->m_descriptorSetLayout);
    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &setLayout;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    VkResult res = vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet);
    if (res != VK_SUCCESS) return nullptr;
    auto* ds = new DescriptorSetVK(deviceObj, pl);
    ds->m_descriptorSet = descriptorSet;
    return ds;
}

void DestroyDescriptorSetVK(DeviceVK* deviceObj, IDescriptorSet* set) {
    if (!deviceObj || !set) return;
    DescriptorSetVK* ds = static_cast<DescriptorSetVK*>(set);
    VkDescriptorSet vkSet = static_cast<VkDescriptorSet>(ds->m_descriptorSet);
    VkDevice device = static_cast<VkDevice>(deviceObj->GetDevice());
    VkDescriptorPool descriptorPool = static_cast<VkDescriptorPool>(deviceObj->GetDescriptorPool());
    if (vkSet != VK_NULL_HANDLE && device != VK_NULL_HANDLE && descriptorPool != VK_NULL_HANDLE)
        vkFreeDescriptorSets(device, descriptorPool, 1, &vkSet);
    ds->m_descriptorSet = nullptr;
    delete ds;
}

} // namespace rhi
} // namespace render
} // namespace eyelib
