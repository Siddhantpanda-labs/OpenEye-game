#include "eyelib/rendering/rhi/Vulkan/pipelines/VulkanBindingUtils.h"
#include "eyelib/rendering/rhi/Vulkan/core/DeviceVK.h"
#include "eyelib/rendering/rhi/Vulkan/commands/CommandListVK.h"
#include "eyelib/rendering/rhi/Vulkan/resources/BufferVK.h"
#include "eyelib/rendering/rhi/Vulkan/resources/TextureVK.h"

namespace eyelib {
namespace rendering {
namespace rhi {
namespace vkhelpers {

bool CreateComputeSet0(VkDevice device, VkDescriptorPool descriptorPool,
                       VkDescriptorSetLayout* outLayout, VkDescriptorSet* outSet) {
    if (!device || !descriptorPool || !outLayout || !outSet) return false;
    *outLayout = VK_NULL_HANDLE;
    *outSet = VK_NULL_HANDLE;
    VkDescriptorSetLayoutBinding computeSet0Bindings[6] = {};
    computeSet0Bindings[0].binding = 0;
    computeSet0Bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    computeSet0Bindings[0].descriptorCount = 1;
    computeSet0Bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    computeSet0Bindings[1].binding = 1;
    computeSet0Bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    computeSet0Bindings[1].descriptorCount = 1;
    computeSet0Bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    computeSet0Bindings[2].binding = 2;
    computeSet0Bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    computeSet0Bindings[2].descriptorCount = 1;
    computeSet0Bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    computeSet0Bindings[3].binding = 3;
    computeSet0Bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    computeSet0Bindings[3].descriptorCount = 1;
    computeSet0Bindings[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    computeSet0Bindings[4].binding = 4;
    computeSet0Bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    computeSet0Bindings[4].descriptorCount = 1;
    computeSet0Bindings[4].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    computeSet0Bindings[5].binding = 5;
    computeSet0Bindings[5].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    computeSet0Bindings[5].descriptorCount = 1;
    computeSet0Bindings[5].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    VkDescriptorSetLayoutCreateInfo computeSet0LayoutInfo = {};
    computeSet0LayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    computeSet0LayoutInfo.bindingCount = 6;
    computeSet0LayoutInfo.pBindings = computeSet0Bindings;
    VkResult res = vkCreateDescriptorSetLayout(device, &computeSet0LayoutInfo, nullptr, outLayout);
    if (res != VK_SUCCESS) return false;

    VkDescriptorSetAllocateInfo computeSet0Alloc = {};
    computeSet0Alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    computeSet0Alloc.descriptorPool = descriptorPool;
    computeSet0Alloc.descriptorSetCount = 1;
    computeSet0Alloc.pSetLayouts = outLayout;
    res = vkAllocateDescriptorSets(device, &computeSet0Alloc, outSet);
    if (res != VK_SUCCESS) {
        vkDestroyDescriptorSetLayout(device, *outLayout, nullptr);
        *outLayout = VK_NULL_HANDLE;
        return false;
    }
    return true;
}

bool CreateComputePipelineLayout(VkDevice device, VkDescriptorSetLayout computeSet0Layout, VkDescriptorSetLayout bindlessSetLayout,
                                 bool useIndependentSets, VkPipelineLayout* outPipelineLayout) {
    if (!device || !computeSet0Layout || !bindlessSetLayout || !outPipelineLayout) return false;
    *outPipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout setLayouts[2] = { computeSet0Layout, bindlessSetLayout };
    VkPushConstantRange pushRange = {};
    pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushRange.offset = 0;
    pushRange.size = 16;
    VkPipelineLayoutCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
#if defined(VK_PIPELINE_LAYOUT_CREATE_INDEPENDENT_SETS_BIT_EXT)
    if (useIndependentSets)
        info.flags |= VK_PIPELINE_LAYOUT_CREATE_INDEPENDENT_SETS_BIT_EXT;
#else
    (void)useIndependentSets;
#endif
    info.setLayoutCount = 2;
    info.pSetLayouts = setLayouts;
    info.pushConstantRangeCount = 1;
    info.pPushConstantRanges = &pushRange;
    return vkCreatePipelineLayout(device, &info, nullptr, outPipelineLayout) == VK_SUCCESS;
}

void UpdateComputeSet0(DeviceVK* deviceObj, CommandListVK* cmd) {
    if (!deviceObj || !cmd) return;
    IBuffer* paramsUbo = deviceObj->GetLegacyComputeParamsUBO();
    if (paramsUbo) {
        const uint8_t* params = cmd->GetComputeParamsData();
        if (params) paramsUbo->Update(params, 0, 256);
    }

    VkDevice device = static_cast<VkDevice>(deviceObj->GetDevice());
    VkDescriptorSet set = static_cast<VkDescriptorSet>(deviceObj->GetComputeSet0());
    VkSampler sampler = static_cast<VkSampler>(deviceObj->GetDefaultSampler());
    if (device == VK_NULL_HANDLE || set == VK_NULL_HANDLE || sampler == VK_NULL_HANDLE) return;

    VkDescriptorBufferInfo bufferInfos[5] = {};
    VkDescriptorImageInfo imageInfo = {};
    VkWriteDescriptorSet writes[6] = {};
    uint32_t numWrites = 0;
    uint32_t bufIdx = 0;
    for (uint32_t binding : { 0u, 1u, 2u, 3u }) {
        IBuffer* buf = nullptr;
        uint32_t offset = 0, size = 0;
        cmd->GetBoundStorageBuffer(binding, &buf, &offset, &size);
        if (!buf) continue;
        BufferVK* bVK = static_cast<BufferVK*>(buf);
        VkBuffer vkBuf = static_cast<VkBuffer>(bVK->m_buffer);
        if (vkBuf == VK_NULL_HANDLE) continue;
        bufferInfos[bufIdx].buffer = vkBuf;
        bufferInfos[bufIdx].offset = offset;
        bufferInfos[bufIdx].range = size > 0 ? size : VK_WHOLE_SIZE;
        writes[numWrites].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[numWrites].dstSet = set;
        writes[numWrites].dstBinding = binding;
        writes[numWrites].dstArrayElement = 0;
        writes[numWrites].descriptorCount = 1;
        writes[numWrites].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[numWrites].pBufferInfo = &bufferInfos[bufIdx];
        bufIdx++;
        numWrites++;
    }
    ITexture* tex4 = cmd->GetBoundTexture(4);
    if (tex4) {
        TextureVK* tVK = static_cast<TextureVK*>(tex4);
        const VkImageView view = tVK->GetVkImageViewForShaderSamplerBinding();
        if (view != VK_NULL_HANDLE) {
            imageInfo.sampler = sampler;
            imageInfo.imageView = view;
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            writes[numWrites].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[numWrites].dstSet = set;
            writes[numWrites].dstBinding = 4;
            writes[numWrites].dstArrayElement = 0;
            writes[numWrites].descriptorCount = 1;
            writes[numWrites].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[numWrites].pImageInfo = &imageInfo;
            numWrites++;
        }
    }
    IBuffer* ubo5 = nullptr;
    uint32_t ubo5Offset = 0, ubo5Size = 0;
    cmd->GetBoundUniformBuffer(5, &ubo5, &ubo5Offset, &ubo5Size);
    if (ubo5) {
        BufferVK* bVK = static_cast<BufferVK*>(ubo5);
        VkBuffer vkBuf = static_cast<VkBuffer>(bVK->m_buffer);
        if (vkBuf != VK_NULL_HANDLE) {
            bufferInfos[bufIdx].buffer = vkBuf;
            bufferInfos[bufIdx].offset = ubo5Offset;
            bufferInfos[bufIdx].range = ubo5Size > 0 ? ubo5Size : VK_WHOLE_SIZE;
            writes[numWrites].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[numWrites].dstSet = set;
            writes[numWrites].dstBinding = 5;
            writes[numWrites].dstArrayElement = 0;
            writes[numWrites].descriptorCount = 1;
            writes[numWrites].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            writes[numWrites].pBufferInfo = &bufferInfos[bufIdx];
            numWrites++;
        }
    }
    if (numWrites > 0) vkUpdateDescriptorSets(device, numWrites, writes, 0, nullptr);
}

} // namespace vkhelpers
} // namespace rhi
} // namespace rendering
} // namespace eyelib

