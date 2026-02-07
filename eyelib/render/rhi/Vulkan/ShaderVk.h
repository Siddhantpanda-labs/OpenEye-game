#pragma once

#include "eyelib/render/rhi/RHI.h"
#include <vulkan/vulkan_core.h>
#include <vector>

namespace eyelib {
namespace render {
namespace rhi {

struct DeviceVk;

struct ShaderVk : IShader {
    ShaderStage GetStage() const override;
    bool IsValid() const override;

    static ShaderVk* Create(DeviceVk* device, ShaderStage stage, const char* source, size_t sourceLen);
    static void Destroy(DeviceVk* device, ShaderVk* shader);

    VkShaderModule m_module = VK_NULL_HANDLE;
    ShaderStage m_stage = ShaderStage::Vertex;
    DeviceVk* m_device = nullptr;
};

} // namespace rhi
} // namespace render
} // namespace eyelib
