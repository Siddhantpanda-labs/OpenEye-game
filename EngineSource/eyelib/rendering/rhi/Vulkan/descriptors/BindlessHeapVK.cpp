#include "eyelib/rendering/rhi/Vulkan/descriptors/BindlessHeapVK.h"
#include "eyelib/rendering/rhi/Vulkan/core/DeviceVK.h"
#include "eyelib/rendering/rhi/Vulkan/descriptors/DescriptorSetVK.h"
#include "eyelib/rendering/rhi/Vulkan/pipelines/PipelineLayoutVK.h"
#include "eyelib/rendering/rhi/Vulkan/resources/TextureVK.h"
#include "eyelib/rendering/rhi/Vulkan/resources/BufferVK.h"
#include "eyelib/rendering/rhi/RHITypes.h"
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
    tryInitDescriptorBufferBacking();
}

BindlessHeapVK::~BindlessHeapVK() {
    if (m_descriptorSetWrapper && m_device) {
        DestroyDescriptorSetVK(m_device, m_descriptorSetWrapper);
        m_descriptorSetWrapper = nullptr;
    }
    if (m_device && m_descriptorBuffer) {
        DestroyBufferVK(m_device, m_descriptorBuffer);
        m_descriptorBuffer = nullptr;
    }
    if (!m_device || !m_descriptorPool) return;
    VkDevice device = static_cast<VkDevice>(m_device->GetDevice());
    vkDestroyDescriptorPool(device, static_cast<VkDescriptorPool>(m_descriptorPool), nullptr);
    m_descriptorPool = nullptr;
    m_descriptorSet = nullptr;
}

bool BindlessHeapVK::tryInitDescriptorBufferBacking() {
#if !defined(VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT) || !defined(VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT)
    return false;
#else
    auto alignUpSize = [](VkDeviceSize value, uint64_t alignment) -> size_t {
        if (alignment == 0u)
            return static_cast<size_t>(value);
        const uint64_t v = static_cast<uint64_t>(value);
        return static_cast<size_t>((v + alignment - 1u) / alignment * alignment);
    };
    if (!m_device || !m_descriptorSet) return false;
    if (!m_device->SupportsDescriptorBuffer()) return false;
    const uint32_t csz = m_device->DescriptorBufferCombinedImageSamplerDescriptorSize();
    const uint32_t ssz = m_device->DescriptorBufferStorageBufferDescriptorSize();
    if (csz == 0 || ssz == 0) return false;
    if (!m_device->DescriptorBufferFnGetSetLayoutSizeEXT() || !m_device->DescriptorBufferFnGetBindingOffsetEXT() ||
        !m_device->DescriptorBufferFnGetDescriptorEXT())
        return false;

    VkDevice vkDevice = static_cast<VkDevice>(m_device->GetDevice());
    VkDescriptorSetLayout dsl = static_cast<VkDescriptorSetLayout>(m_device->GetBindlessSetLayout());
    if (vkDevice == VK_NULL_HANDLE || dsl == VK_NULL_HANDLE) return false;

    auto pfnLayoutSize = reinterpret_cast<PFN_vkGetDescriptorSetLayoutSizeEXT>(m_device->DescriptorBufferFnGetSetLayoutSizeEXT());
    auto pfnBindingOffset =
        reinterpret_cast<PFN_vkGetDescriptorSetLayoutBindingOffsetEXT>(m_device->DescriptorBufferFnGetBindingOffsetEXT());

    VkDeviceSize layoutSizeVk = 0;
    pfnLayoutSize(vkDevice, dsl, &layoutSizeVk);
    if (layoutSizeVk == 0) return false;

    VkDeviceSize off0 = 0;
    VkDeviceSize off1 = 0;
    pfnBindingOffset(vkDevice, dsl, 0, &off0);
    pfnBindingOffset(vkDevice, dsl, 1, &off1);

    const size_t backingSize = alignUpSize(layoutSizeVk, m_device->DescriptorBufferOffsetAlignment());
    if (backingSize == 0) return false;

    BufferDesc bd = {};
    bd.sizeInBytes = backingSize;
    bd.usage = BufferUsage::DescriptorBuffer | BufferUsage::ShaderDeviceAddress;
    bd.memoryHint = BufferMemoryHint::HostVisible;
    bd.debugName = "BindlessDescriptorBuffer";
    IBuffer* buf = CreateBufferVK(m_device, bd, nullptr);
    if (!buf) {
        core::Log::Warn("BindlessHeapVK: CreateBufferVK failed for descriptor buffer backing");
        return false;
    }
    const uint64_t addr = m_device->GetBufferDeviceAddress(buf);
    if (addr == 0) {
        DestroyBufferVK(m_device, buf);
        core::Log::Warn("BindlessHeapVK: descriptor buffer backing has no shader device address");
        return false;
    }

    m_descriptorBuffer = buf;
    m_descriptorBufferSize = backingSize;
    m_descriptorBufferAddress = addr;
    m_binding0Offset = static_cast<size_t>(off0);
    m_binding1Offset = static_cast<size_t>(off1);
    m_combinedImageSamplerDescriptorSize = csz;
    m_storageBufferDescriptorSize = ssz;
    m_descriptorBufferBindingIndex = 0;
    m_usesDescriptorBuffer = true;
    return true;
#endif
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
    if (m_nextTextureIndex >= kMaxTextures) return kMaxTextures; // invalid
    return m_nextTextureIndex++;
}

void BindlessHeapVK::freeTextureSlot(uint32_t index) { m_freeTextureSlots.push_back(index); }

uint32_t BindlessHeapVK::allocBufferSlot() {
    if (!m_freeBufferSlots.empty()) {
        uint32_t slot = m_freeBufferSlots.back();
        m_freeBufferSlots.pop_back();
        return slot;
    }
    if (m_nextBufferIndex >= kMaxBuffers) return kMaxBuffers;
    return m_nextBufferIndex++;
}

void BindlessHeapVK::freeBufferSlot(uint32_t index) { m_freeBufferSlots.push_back(index); }

bool BindlessHeapVK::UpdateDescriptorBufferTexture(uint32_t index, TextureVK* tex) {
#if !defined(VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT)
    (void)index;
    (void)tex;
    return false;
#else
    if (!m_usesDescriptorBuffer || !m_descriptorBuffer || !m_device || !tex) return false;
    const VkImageView imageView = tex->GetVkImageViewForShaderSamplerBinding();
    if (imageView == VK_NULL_HANDLE) return false;
    VkSampler sampler = static_cast<VkSampler>(m_device->GetDefaultSampler());
    if (sampler == VK_NULL_HANDLE) return false;

    VkDescriptorImageInfo imageInfo = {};
    imageInfo.sampler = sampler;
    imageInfo.imageView = imageView;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorGetInfoEXT getInfo = {};
    getInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT;
    getInfo.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    getInfo.data.pCombinedImageSampler = &imageInfo;

    BufferVK* db = static_cast<BufferVK*>(m_descriptorBuffer);
    const size_t dstOff = m_binding0Offset + static_cast<size_t>(index) * m_combinedImageSamplerDescriptorSize;
    if (dstOff + m_combinedImageSamplerDescriptorSize > db->GetSize()) return false;

    VkDevice vkDevice = static_cast<VkDevice>(m_device->GetDevice());
    auto pfnGet = reinterpret_cast<PFN_vkGetDescriptorEXT>(m_device->DescriptorBufferFnGetDescriptorEXT());
    void* dst = db->Map(dstOff, m_combinedImageSamplerDescriptorSize);
    if (!dst) return false;
    pfnGet(vkDevice, &getInfo, m_combinedImageSamplerDescriptorSize, dst);
    db->FlushHostVisibleWrites(dstOff, m_combinedImageSamplerDescriptorSize);
    db->Unmap();
    return true;
#endif
}

bool BindlessHeapVK::UpdateDescriptorBufferStorageBuffer(uint32_t index, BufferVK* buf) {
#if !defined(VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT) || !defined(VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT)
    (void)index;
    (void)buf;
    return false;
#else
    if (!m_usesDescriptorBuffer || !m_descriptorBuffer || !m_device || !buf) return false;
    VkBuffer vkBuf = buf->m_buffer ? static_cast<VkBuffer>(buf->m_buffer) : VK_NULL_HANDLE;
    if (vkBuf == VK_NULL_HANDLE) return false;

    const uint64_t address = m_device->GetBufferDeviceAddress(buf);
    if (address == 0) return false;

    VkDescriptorAddressInfoEXT addrInfo = {};
    addrInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT;
    addrInfo.address = address;
    addrInfo.range = buf->GetSize();
    addrInfo.format = VK_FORMAT_UNDEFINED;

    VkDescriptorGetInfoEXT getInfo = {};
    getInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT;
    getInfo.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    getInfo.data.pStorageBuffer = &addrInfo;

    BufferVK* db = static_cast<BufferVK*>(m_descriptorBuffer);
    const size_t dstOff = m_binding1Offset + static_cast<size_t>(index) * m_storageBufferDescriptorSize;
    if (dstOff + m_storageBufferDescriptorSize > db->GetSize()) return false;

    VkDevice vkDevice = static_cast<VkDevice>(m_device->GetDevice());
    auto pfnGet = reinterpret_cast<PFN_vkGetDescriptorEXT>(m_device->DescriptorBufferFnGetDescriptorEXT());
    void* dst = db->Map(dstOff, m_storageBufferDescriptorSize);
    if (!dst) return false;
    pfnGet(vkDevice, &getInfo, m_storageBufferDescriptorSize, dst);
    db->FlushHostVisibleWrites(dstOff, m_storageBufferDescriptorSize);
    db->Unmap();
    return true;
#endif
}

BindlessHandle BindlessHeapVK::AllocateTextureHandle(ITexture* texture) {
    if (!texture || !m_device) return kInvalidBindlessHandle;
    if (!m_descriptorSet) {
        if (!m_warnedAllocateOnUninitializedHeap) {
            core::Log::Error(
                "BindlessHeapVK: AllocateTextureHandle called on uninitialized heap (pool or set creation failed); returning invalid handle.");
            m_warnedAllocateOnUninitializedHeap = true;
        }
        return kInvalidBindlessHandle;
    }
    TextureVK* tex = static_cast<TextureVK*>(texture);
    const VkImageView imageView = tex->GetVkImageViewForShaderSamplerBinding();
    if (imageView == VK_NULL_HANDLE) return kInvalidBindlessHandle;
    VkSampler sampler = static_cast<VkSampler>(m_device->GetDefaultSampler());
    if (sampler == VK_NULL_HANDLE) return kInvalidBindlessHandle;

    uint32_t index = allocTextureSlot();
    if (index >= kMaxTextures) {
        if (!m_warnedTextureHeapExhausted) {
            core::Log::WarnF("BindlessHeapVK::AllocateTextureHandle: sampled-image table full (%u slots); returning invalid handle.", kMaxTextures);
            m_warnedTextureHeapExhausted = true;
        }
        return kInvalidBindlessHandle;
    }

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

    if (m_usesDescriptorBuffer && !UpdateDescriptorBufferTexture(index, tex)) {
        if (!m_warnedDescriptorBufferTextureEncodeFailure) {
            core::Log::Warn(
                "BindlessHeapVK: descriptor buffer texture encoding failed; disabling vkCmdBindDescriptorBuffersEXT path for this heap; "
                "classic descriptor-set bindless remains valid.");
            m_warnedDescriptorBufferTextureEncodeFailure = true;
        }
        m_usesDescriptorBuffer = false;
    }

    return static_cast<BindlessHandle>(index + 1u);
}

BindlessHandle BindlessHeapVK::AllocateBufferHandle(IBuffer* buffer) {
    if (!buffer || !m_device) return kInvalidBindlessHandle;
    if (!m_descriptorSet) {
        if (!m_warnedAllocateOnUninitializedHeap) {
            core::Log::Error(
                "BindlessHeapVK: AllocateBufferHandle called on uninitialized heap (pool or set creation failed); returning invalid handle.");
            m_warnedAllocateOnUninitializedHeap = true;
        }
        return kInvalidBindlessHandle;
    }
    BufferVK* buf = static_cast<BufferVK*>(buffer);
    VkBuffer vkBuf = buf->m_buffer ? static_cast<VkBuffer>(buf->m_buffer) : VK_NULL_HANDLE;
    if (vkBuf == VK_NULL_HANDLE) return kInvalidBindlessHandle;

    const size_t bufSize = buf->GetSize();
    if (bufSize == 0) {
        if (!m_warnedZeroSizeStorageBuffer) {
            core::Log::Warn("BindlessHeapVK::AllocateBufferHandle: rejecting zero-size buffer for storage-buffer bindless slot.");
            m_warnedZeroSizeStorageBuffer = true;
        }
        return kInvalidBindlessHandle;
    }

    uint32_t index = allocBufferSlot();
    if (index >= kMaxBuffers) {
        if (!m_warnedBufferHeapExhausted) {
            core::Log::WarnF("BindlessHeapVK::AllocateBufferHandle: storage buffer table full (%u slots); returning invalid handle.", kMaxBuffers);
            m_warnedBufferHeapExhausted = true;
        }
        return kInvalidBindlessHandle;
    }

    size_t range = bufSize;
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

    if (m_usesDescriptorBuffer && !UpdateDescriptorBufferStorageBuffer(index, buf)) {
        if (!m_warnedDescriptorBufferBufferEncodeFailure) {
            core::Log::Warn(
                "BindlessHeapVK: descriptor buffer storage-buffer encoding failed; disabling vkCmdBindDescriptorBuffersEXT path for this heap; "
                "classic descriptor-set bindless remains valid.");
            m_warnedDescriptorBufferBufferEncodeFailure = true;
        }
        m_usesDescriptorBuffer = false;
    }

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
