#include "eyelib/render/rhi/Vulkan/ShaderVK.h"
#include "eyelib/render/rhi/Vulkan/DeviceVK.h"
#include <vulkan/vulkan.h>

namespace eyelib {
namespace render {
namespace rhi {

ShaderVK::ShaderVK(DeviceVK* device) : m_device(device) {}

ShaderVK::~ShaderVK() {
    if (m_module && m_device && m_device->GetDevice()) {
        vkDestroyShaderModule(static_cast<VkDevice>(m_device->GetDevice()), static_cast<VkShaderModule>(m_module), nullptr);
        m_module = nullptr;
    }
}

ShaderStage ShaderVK::GetStage() const { return m_stage; }

bool ShaderVK::IsValid() const { return m_module != nullptr; }

} // namespace rhi
} // namespace render
} // namespace eyelib
