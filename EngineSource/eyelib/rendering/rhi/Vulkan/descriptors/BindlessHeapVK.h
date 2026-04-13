#pragma once

#include "eyelib/rendering/rhi/RHI.h"
#include <vector>

namespace eyelib {
namespace rendering {
namespace rhi {

struct DeviceVK;

/** **Bindless** table: one **`VkDescriptorSet`**, binding **0** = **`COMBINED_IMAGE_SAMPLER`** array (**`kMaxTextures`**),
 *  binding **1** = **`STORAGE_BUFFER`** array (**`kMaxBuffers`**). Pool uses **`VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT`**
 *  with the device’s **`GetBindlessSetLayout`** (descriptor indexing / **`UPDATE_AFTER_BIND`** must match **`DeviceVK`** creation).
 *  **`BindlessHandle`** is **1 + array index**; **`Allocate*`** failures log pool/alloc errors. Freeing a handle recycles the slot
 *  index only (does not zero GPU descriptors — shaders must treat freed indices as invalid). */
struct BindlessHeapVK : IBindlessHeap {
    explicit BindlessHeapVK(DeviceVK* device);
    ~BindlessHeapVK() override;

    BindlessHandle AllocateTextureHandle(ITexture* texture) override;
    BindlessHandle AllocateBufferHandle(IBuffer* buffer) override;
    void FreeTextureHandle(BindlessHandle handle) override;
    void FreeBufferHandle(BindlessHandle handle) override;
    IDescriptorSet* GetDescriptorSet() override;

    DeviceVK* m_device = nullptr;
    IDescriptorSet* m_descriptorSetWrapper = nullptr;
    void* m_descriptorPool = nullptr;  // VkDescriptorPool
    void* m_descriptorSet = nullptr;   // VkDescriptorSet

    static constexpr uint32_t kMaxTextures = 8192;
    static constexpr uint32_t kMaxBuffers = 8192;

private:
    uint32_t allocTextureSlot();
    void freeTextureSlot(uint32_t index);
    uint32_t allocBufferSlot();
    void freeBufferSlot(uint32_t index);

    uint32_t m_nextTextureIndex = 0;
    uint32_t m_nextBufferIndex = 0;
    std::vector<uint32_t> m_freeTextureSlots;
    std::vector<uint32_t> m_freeBufferSlots;
};

} // namespace rhi
} // namespace rendering
} // namespace eyelib
