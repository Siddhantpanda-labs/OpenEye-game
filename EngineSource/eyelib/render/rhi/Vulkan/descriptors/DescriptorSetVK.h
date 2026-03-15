#pragma once

#include "eyelib/render/rhi/RHI.h"
#include "eyelib/render/rhi/Vulkan/pipelines/PipelineLayoutVK.h"
#include <vector>

namespace eyelib {
namespace render {
namespace rhi {

struct DeviceVK;

struct DescriptorSetVK : IDescriptorSet {
    explicit DescriptorSetVK(DeviceVK* device, PipelineLayoutVK* layout);
    ~DescriptorSetVK() override;

    void UpdateTexture(uint32_t binding, ITexture* texture) override;
    void UpdateUniformBuffer(uint32_t binding, IBuffer* buffer, uint32_t offset = 0, uint32_t size = 0) override;
    void UpdateStorageBuffer(uint32_t binding, IBuffer* buffer, uint32_t offset = 0, uint32_t size = 0) override;

    DeviceVK* m_device = nullptr;
    PipelineLayoutVK* m_layout = nullptr;
    void* m_descriptorSet = nullptr;
    bool m_ownsDescriptorSet = true;
};

IPipelineLayout* CreatePipelineLayoutVK(DeviceVK* device, const PipelineLayoutDesc& desc);
/** Wrapper around an existing VkDescriptorSetLayout (e.g. resource-space Frame set). Does not own the set layout. */
IPipelineLayout* CreatePipelineLayoutWrapperVK(DeviceVK* device, void* vkDescriptorSetLayout);
void DestroyPipelineLayoutVK(DeviceVK* device, IPipelineLayout* layout);
IDescriptorSet* CreateDescriptorSetVK(DeviceVK* device, IPipelineLayout* layout);
/** Wraps an existing VkDescriptorSet (e.g. from BindlessHeapVK). Caller must not free the set; wrapper does not free it. */
IDescriptorSet* CreateDescriptorSetWrapperVK(DeviceVK* device, IPipelineLayout* layout, void* existingVkDescriptorSet);
void DestroyDescriptorSetVK(DeviceVK* device, IDescriptorSet* set);

} // namespace rhi
} // namespace render
} // namespace eyelib
