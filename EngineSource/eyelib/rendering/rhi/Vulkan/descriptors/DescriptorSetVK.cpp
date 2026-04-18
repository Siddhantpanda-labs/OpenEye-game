#include "eyelib/rendering/rhi/Vulkan/descriptors/DescriptorSetVK.h"
#include "eyelib/rendering/rhi/Vulkan/core/DeviceVK.h"
#include "eyelib/rendering/rhi/Vulkan/resources/TextureVK.h"
#include "eyelib/rendering/rhi/Vulkan/resources/BufferVK.h"
#include "eyelib/rendering/rhi/Vulkan/utils/VulkanEnums.h"
#include "eyelib/core/Log.h"
#include <vulkan/vulkan.h>
#include <algorithm>
#include <string>

namespace eyelib {
namespace rendering {
namespace rhi {

namespace {

static bool TryGetDeclaredBindingType(const PipelineLayoutVK* layout, uint32_t binding, BindingType* outType) {
    if (!layout) return false;
    const auto& bindings = layout->m_desc.bindings;
    if (bindings.empty()) return false;
    auto it = std::find_if(bindings.begin(), bindings.end(), [binding](const BindingDesc& b) { return b.binding == binding; });
    if (it == bindings.end()) return false;
    if (outType) *outType = it->type;
    return true;
}

static const char* BindingTypeName(BindingType type) {
    switch (type) {
    case BindingType::Texture: return "Texture";
    case BindingType::TextureCube: return "TextureCube";
    case BindingType::UniformBuffer: return "UniformBuffer";
    case BindingType::StorageBuffer: return "StorageBuffer";
    default: return "Unknown";
    }
}

static const char* PipelineLayoutKindName(PipelineLayoutKind kind) {
    switch (kind) {
    case PipelineLayoutKind::PushOnly: return "PushOnly";
    case PipelineLayoutKind::Forward: return "Forward";
    case PipelineLayoutKind::Post: return "Post";
    case PipelineLayoutKind::Shadow: return "Shadow";
    case PipelineLayoutKind::GBuffer: return "GBuffer";
    case PipelineLayoutKind::DeferredLighting: return "DeferredLighting";
    case PipelineLayoutKind::ComputeLightCull: return "ComputeLightCull";
    case PipelineLayoutKind::ComputeGpuCull: return "ComputeGpuCull";
    case PipelineLayoutKind::IBL: return "IBL";
    case PipelineLayoutKind::OutlinePost: return "OutlinePost";
    case PipelineLayoutKind::ComposeBootstrap: return "ComposeBootstrap";
    default: return "Unknown";
    }
}

static std::string DescribeBindings(const PipelineLayoutVK* layout, uint32_t mask, bool onlyMasked) {
    if (!layout) return "{}";
    std::string out = "{";
    bool first = true;
    for (const BindingDesc& b : layout->m_desc.bindings) {
        const bool tracked = (b.binding < DescriptorSetVK::kTrackedBindings);
        const bool inMask = tracked ? ((mask & (1u << b.binding)) != 0u) : false;
        if (onlyMasked && !inMask) continue;
        if (!first) out += ", ";
        first = false;
        out += std::to_string(b.binding);
        out += ":";
        out += BindingTypeName(b.type);
        if (!tracked) out += "(untracked)";
    }
    out += "}";
    return out;
}

static uint32_t ComputeRequiredBindingMask(const PipelineLayoutVK* layout) {
    if (!layout) return 0u;
    uint32_t mask = 0u;
    for (const BindingDesc& b : layout->m_desc.bindings) {
        if (b.binding < DescriptorSetVK::kTrackedBindings)
            mask |= (1u << b.binding);
    }
    return mask;
}

} // namespace

DescriptorSetVK::DescriptorSetVK(DeviceVK* device, PipelineLayoutVK* layout)
    : m_device(device), m_layout(layout) {}

DescriptorSetVK::~DescriptorSetVK() = default;

void DescriptorSetVK::UpdateTexture(uint32_t binding, ITexture* texture) {
    if (!m_device || !m_descriptorSet || !m_layout) return;
    BindingType declared = BindingType::Texture;
    if (TryGetDeclaredBindingType(m_layout, binding, &declared)) {
        if (declared != BindingType::Texture && declared != BindingType::TextureCube) {
            core::Log::ErrorF("DescriptorSetVK::UpdateTexture type mismatch: binding=%u expected=Texture/TextureCube declared=%s layoutBindings=%s",
                              binding, BindingTypeName(declared), DescribeBindings(m_layout, 0u, false).c_str());
            return;
        }
    } else if (!m_layout->m_desc.bindings.empty()) {
        core::Log::ErrorF("DescriptorSetVK::UpdateTexture undeclared binding=%u layoutBindings=%s",
                          binding, DescribeBindings(m_layout, 0u, false).c_str());
        return;
    }
    ITexture* effective = texture;
    if (!texture || !texture->IsGpuBindable()) {
        if (declared == BindingType::TextureCube)
            effective = m_device->GetFallbackShaderResourceTextureCube();
        else
            effective = m_device->GetFallbackShaderResourceTexture2D();
        if (!effective) return;
    }
    TextureVK* texVK = static_cast<TextureVK*>(effective);
    VkImageView imageView = texVK && texVK->m_imageView ? static_cast<VkImageView>(texVK->m_imageView) : VK_NULL_HANDLE;
    const bool needsNearestSampler = (effective && effective->GetFormat() == RHITextureFormat::R32UI);
    VkSampler sampler = static_cast<VkSampler>(needsNearestSampler ? m_device->GetNearestSampler() : m_device->GetDefaultSampler());
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
    if (binding < kTrackedBindings) m_writtenBindingMask |= (1u << binding);
    m_loggedIncomplete = false;
}

void DescriptorSetVK::UpdateUniformBuffer(uint32_t binding, IBuffer* buffer, uint32_t offset, uint32_t size) {
    if (!m_device || !m_descriptorSet || !buffer) return;
    BindingType declared = BindingType::UniformBuffer;
    if (TryGetDeclaredBindingType(m_layout, binding, &declared)) {
        if (declared != BindingType::UniformBuffer) {
            core::Log::ErrorF("DescriptorSetVK::UpdateUniformBuffer type mismatch: binding=%u expected=UniformBuffer declared=%s layoutBindings=%s",
                              binding, BindingTypeName(declared), DescribeBindings(m_layout, 0u, false).c_str());
            return;
        }
    } else if (m_layout && !m_layout->m_desc.bindings.empty()) {
        core::Log::ErrorF("DescriptorSetVK::UpdateUniformBuffer undeclared binding=%u layoutBindings=%s",
                          binding, DescribeBindings(m_layout, 0u, false).c_str());
        return;
    }
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
    if (binding < kTrackedBindings) m_writtenBindingMask |= (1u << binding);
    m_loggedIncomplete = false;
}

void DescriptorSetVK::UpdateStorageBuffer(uint32_t binding, IBuffer* buffer, uint32_t offset, uint32_t size) {
    if (!m_device || !m_descriptorSet || !buffer) return;
    BindingType declared = BindingType::StorageBuffer;
    if (TryGetDeclaredBindingType(m_layout, binding, &declared)) {
        if (declared != BindingType::StorageBuffer) {
            core::Log::ErrorF("DescriptorSetVK::UpdateStorageBuffer type mismatch: binding=%u expected=StorageBuffer declared=%s layoutBindings=%s",
                              binding, BindingTypeName(declared), DescribeBindings(m_layout, 0u, false).c_str());
            return;
        }
    } else if (m_layout && !m_layout->m_desc.bindings.empty()) {
        core::Log::ErrorF("DescriptorSetVK::UpdateStorageBuffer undeclared binding=%u layoutBindings=%s",
                          binding, DescribeBindings(m_layout, 0u, false).c_str());
        return;
    }
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
    if (binding < kTrackedBindings) m_writtenBindingMask |= (1u << binding);
    m_loggedIncomplete = false;
}

bool DescriptorSetVK::IsCompleteForBind(uint32_t setIndex, PipelineLayoutKind layoutKind) const {
    if (!m_ownsDescriptorSet) return true;
    if (!m_layout) return true;
    const uint32_t requiredMask = ComputeRequiredBindingMask(m_layout);
    if (requiredMask == 0u) return true;
    if ((m_writtenBindingMask & requiredMask) == requiredMask) return true;
    if (!m_loggedIncomplete) {
        const uint32_t missing = (requiredMask & ~m_writtenBindingMask);
        const std::string requiredBindings = DescribeBindings(m_layout, requiredMask, true);
        const std::string missingBindings = DescribeBindings(m_layout, missing, true);
        core::Log::ErrorF("DescriptorSetVK incomplete: layoutKind=%s set=%u vkSet=%p requiredMask=0x%08X writtenMask=0x%08X missingMask=0x%08X requiredBindings=%s missingBindings=%s",
                          PipelineLayoutKindName(layoutKind), setIndex, (void*)m_descriptorSet, requiredMask, m_writtenBindingMask, missing,
                          requiredBindings.c_str(), missingBindings.c_str());
        m_loggedIncomplete = true;
    }
    return false;
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
    pl->m_ownsSetLayout = true;
    return pl;
}

IPipelineLayout* CreatePipelineLayoutWrapperVK(DeviceVK* deviceObj, void* vkDescriptorSetLayout) {
    PipelineLayoutDesc empty;
    return CreatePipelineLayoutWrapperVK(deviceObj, vkDescriptorSetLayout, empty);
}

IPipelineLayout* CreatePipelineLayoutWrapperVK(DeviceVK* deviceObj, void* vkDescriptorSetLayout, const PipelineLayoutDesc& desc) {
    if (!deviceObj || !vkDescriptorSetLayout) return nullptr;
    VkDescriptorSetLayout setLayout = static_cast<VkDescriptorSetLayout>(vkDescriptorSetLayout);
    VkDevice device = static_cast<VkDevice>(deviceObj->GetDevice());
    VkPipelineLayoutCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    info.setLayoutCount = 1;
    info.pSetLayouts = &setLayout;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkResult res = vkCreatePipelineLayout(device, &info, nullptr, &pipelineLayout);
    if (res != VK_SUCCESS) {
        core::Log::ErrorF("CreatePipelineLayoutWrapperVK: vkCreatePipelineLayout failed (0x%x)", res);
        return nullptr;
    }
    auto* pl = new PipelineLayoutVK();
    pl->m_desc = desc;
    pl->m_descriptorSetLayout = vkDescriptorSetLayout;
    pl->m_pipelineLayout = pipelineLayout;
    pl->m_ownsSetLayout = false;
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
        if (pl->m_ownsSetLayout && pl->m_descriptorSetLayout != VK_NULL_HANDLE) {
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
    if (res != VK_SUCCESS) {
        core::Log::ErrorF(
            "CreateDescriptorSetVK: vkAllocateDescriptorSets failed (0x%x) — global descriptor pool may be exhausted (max sets / per-type limits).",
            static_cast<unsigned>(res));
        return nullptr;
    }
    auto* ds = new DescriptorSetVK(deviceObj, pl);
    ds->m_descriptorSet = descriptorSet;
    ds->m_ownsDescriptorSet = true;
    return ds;
}

IDescriptorSet* CreateDescriptorSetWrapperVK(DeviceVK* deviceObj, IPipelineLayout* layout, void* existingVkDescriptorSet) {
    if (!deviceObj || !layout || !existingVkDescriptorSet) return nullptr;
    PipelineLayoutVK* pl = static_cast<PipelineLayoutVK*>(layout);
    auto* ds = new DescriptorSetVK(deviceObj, pl);
    ds->m_descriptorSet = existingVkDescriptorSet;
    ds->m_ownsDescriptorSet = false;
    return ds;
}

void DestroyDescriptorSetVK(DeviceVK* deviceObj, IDescriptorSet* set) {
    if (!deviceObj || !set) return;
    DescriptorSetVK* ds = static_cast<DescriptorSetVK*>(set);
    if (ds->m_ownsDescriptorSet) {
        VkDescriptorSet vkSet = static_cast<VkDescriptorSet>(ds->m_descriptorSet);
        VkDevice device = static_cast<VkDevice>(deviceObj->GetDevice());
        VkDescriptorPool descriptorPool = static_cast<VkDescriptorPool>(deviceObj->GetDescriptorPool());
        if (vkSet != VK_NULL_HANDLE && device != VK_NULL_HANDLE && descriptorPool != VK_NULL_HANDLE)
            vkFreeDescriptorSets(device, descriptorPool, 1, &vkSet);
    }
    ds->m_descriptorSet = nullptr;
    delete ds;
}

} // namespace rhi
} // namespace rendering
} // namespace eyelib
