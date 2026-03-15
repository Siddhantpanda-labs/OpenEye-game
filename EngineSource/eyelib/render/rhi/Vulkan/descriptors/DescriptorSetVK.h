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
    // VkDescriptorSet (filled when descriptor system is implemented)
    void* m_descriptorSet = nullptr;
};

IPipelineLayout* CreatePipelineLayoutVK(DeviceVK* device, const PipelineLayoutDesc& desc);
void DestroyPipelineLayoutVK(DeviceVK* device, IPipelineLayout* layout);
IDescriptorSet* CreateDescriptorSetVK(DeviceVK* device, IPipelineLayout* layout);
void DestroyDescriptorSetVK(DeviceVK* device, IDescriptorSet* set);

} // namespace rhi
} // namespace render
} // namespace eyelib
