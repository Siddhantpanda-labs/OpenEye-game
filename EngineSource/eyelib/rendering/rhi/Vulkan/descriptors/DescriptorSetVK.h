#pragma once

#include "eyelib/rendering/rhi/RHI.h"
#include "eyelib/rendering/rhi/Vulkan/pipelines/PipelineLayoutVK.h"
#include <cstdint>
#include <vector>

namespace eyelib {
namespace rendering {
namespace rhi {

struct DeviceVK;

struct DescriptorSetVK : IDescriptorSet {
    explicit DescriptorSetVK(DeviceVK* device, PipelineLayoutVK* layout);
    ~DescriptorSetVK() override;

    void UpdateTexture(uint32_t binding, ITexture* texture) override;
    void UpdateUniformBuffer(uint32_t binding, IBuffer* buffer, uint32_t offset = 0, uint32_t size = 0) override;
    void UpdateStorageBuffer(uint32_t binding, IBuffer* buffer, uint32_t offset = 0, uint32_t size = 0) override;
    /** Validation: true when all bindings declared in layout desc were updated at least once. */
    bool IsCompleteForBind(uint32_t setIndex, PipelineLayoutKind layoutKind) const;

    DeviceVK* m_device = nullptr;
    PipelineLayoutVK* m_layout = nullptr;
    void* m_descriptorSet = nullptr;
    bool m_ownsDescriptorSet = true;
    static constexpr uint32_t kTrackedBindings = 32;
    uint32_t m_writtenBindingMask = 0u;
    mutable bool m_loggedIncomplete = false;
};

IPipelineLayout* CreatePipelineLayoutVK(DeviceVK* device, const PipelineLayoutDesc& desc);
/** Wrapper around an existing VkDescriptorSetLayout (e.g. resource-space Frame set). Does not own the set layout. */
IPipelineLayout* CreatePipelineLayoutWrapperVK(DeviceVK* device, void* vkDescriptorSetLayout);
/** Wrapper with explicit binding desc (recommended for resource-space set validation). */
IPipelineLayout* CreatePipelineLayoutWrapperVK(DeviceVK* device, void* vkDescriptorSetLayout, const PipelineLayoutDesc& desc);
void DestroyPipelineLayoutVK(DeviceVK* device, IPipelineLayout* layout);
IDescriptorSet* CreateDescriptorSetVK(DeviceVK* device, IPipelineLayout* layout);
/** Wraps an existing VkDescriptorSet (e.g. from BindlessHeapVK). Caller must not free the set; wrapper does not free it. */
IDescriptorSet* CreateDescriptorSetWrapperVK(DeviceVK* device, IPipelineLayout* layout, void* existingVkDescriptorSet);
void DestroyDescriptorSetVK(DeviceVK* device, IDescriptorSet* set);

} // namespace rhi
} // namespace rendering
} // namespace eyelib
