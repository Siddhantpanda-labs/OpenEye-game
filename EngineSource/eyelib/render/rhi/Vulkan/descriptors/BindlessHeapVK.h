#pragma once

#include "eyelib/render/rhi/RHI.h"
#include <vector>

namespace eyelib {
namespace render {
namespace rhi {

struct DeviceVK;

/** Vulkan bindless heap: one descriptor set with texture array (binding 0) and buffer array (binding 1). */
struct BindlessHeapVK : IBindlessHeap {
    explicit BindlessHeapVK(DeviceVK* device);
    ~BindlessHeapVK() override;

    BindlessHandle AllocateTextureHandle(ITexture* texture) override;
    BindlessHandle AllocateBufferHandle(IBuffer* buffer) override;
    void FreeTextureHandle(BindlessHandle handle) override;
    void FreeBufferHandle(BindlessHandle handle) override;

    DeviceVK* m_device = nullptr;
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
} // namespace render
} // namespace eyelib
