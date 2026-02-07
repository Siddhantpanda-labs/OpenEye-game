#include "eyelib/render/rhi/Vulkan/ShaderVk.h"
#include "eyelib/render/rhi/Vulkan/DeviceVk.h"
#include "eyelib/render/rhi/Vulkan/VkShaderCompiler.h"
#include <vulkan/vulkan_core.h>
#include <cstring>

namespace eyelib {
namespace render {
namespace rhi {

ShaderVk* ShaderVk::Create(DeviceVk* device, ShaderStage stage, const char* source, size_t sourceLen) {
    if (!device || !device->GetDevice() || !source || sourceLen == 0) return nullptr;
    std::vector<uint32_t> spirv = CompileGLSLToSPIRV(stage, source, sourceLen);
    if (spirv.empty()) return nullptr;
    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = spirv.size() * sizeof(uint32_t);
    createInfo.pCode = spirv.data();
    auto* sh = new ShaderVk();
    sh->m_device = device;
    sh->m_stage = stage;
    VkResult r = vkCreateShaderModule(device->GetDevice(), &createInfo, nullptr, &sh->m_module);
    if (r != VK_SUCCESS) { delete sh; return nullptr; }
    return sh;
}

void ShaderVk::Destroy(DeviceVk* device, ShaderVk* shader) {
    if (!device || !shader) return;
    if (shader->m_module != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device->GetDevice(), shader->m_module, nullptr);
        shader->m_module = VK_NULL_HANDLE;
    }
    delete shader;
}

ShaderStage ShaderVk::GetStage() const { return m_stage; }
bool ShaderVk::IsValid() const { return m_module != VK_NULL_HANDLE; }

} // namespace rhi
} // namespace render
} // namespace eyelib
