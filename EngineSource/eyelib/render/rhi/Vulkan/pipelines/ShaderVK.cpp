#include "eyelib/render/rhi/Vulkan/pipelines/ShaderVK.h"
#include "eyelib/render/rhi/Vulkan/pipelines/VulkanGlslToSpirv.h"
#include "eyelib/render/rhi/Vulkan/core/DeviceVK.h"
#include "eyelib/core/Log.h"
#include <vulkan/vulkan.h>
#include <fstream>
#include <vector>

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

IShader* CreateShaderVK(DeviceVK* device, ShaderStage stage, const char* source, size_t sourceLen) {
    if (!device || !source || sourceLen == 0) return nullptr;
    if (stage != ShaderStage::Vertex && stage != ShaderStage::Fragment && stage != ShaderStage::Compute) {
        core::Log::ErrorF("CreateShaderVK: unsupported stage %u", static_cast<unsigned>(static_cast<uint32_t>(stage)));
        return nullptr;
    }
    const char* fname = "shader.comp";
    if (stage == ShaderStage::Vertex)
        fname = "shader.vert";
    else if (stage == ShaderStage::Fragment)
        fname = "shader.frag";
    std::vector<uint32_t> spirv;
    if (!VulkanCompileGlslToSpirv(source, sourceLen, stage, fname, spirv) || spirv.empty())
        return nullptr;
    return CreateShaderFromBinaryVK(device, stage, spirv.data(), spirv.size() * sizeof(uint32_t));
}

IShader* CreateShaderFromBinaryVK(DeviceVK* device, ShaderStage stage, const void* data, size_t size) {
    if (!device || !device->GetDevice() || !data || size < 4) return nullptr;
    const uint32_t spirvMagic = 0x07230203u;
    if (*static_cast<const uint32_t*>(data) != spirvMagic) {
        core::Log::Error("CreateShaderFromBinaryVK: data is not SPIR-V (invalid magic)");
        return nullptr;
    }
    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = size;
    createInfo.pCode = static_cast<const uint32_t*>(data);
    VkShaderModule module = VK_NULL_HANDLE;
    VkResult res = vkCreateShaderModule(static_cast<VkDevice>(device->GetDevice()), &createInfo, nullptr, &module);
    if (res != VK_SUCCESS) {
        core::Log::ErrorF("CreateShaderFromBinaryVK: vkCreateShaderModule failed (0x%x)", res);
        return nullptr;
    }
    auto* s = new ShaderVK(device);
    s->m_stage = stage;
    s->m_module = module;
    return s;
}

static std::vector<uint8_t> ReadFileBinaryLocal(const char* path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return {};
    const size_t size = static_cast<size_t>(f.tellg());
    f.seekg(0);
    std::vector<uint8_t> buf(size);
    if (!f.read(reinterpret_cast<char*>(buf.data()), size)) return {};
    return buf;
}

bool TryLoadShaderPairVK(DeviceVK* device, const char* vertexPath, const char* fragmentPath, IShader*& outVs, IShader*& outFs) {
    outVs = nullptr;
    outFs = nullptr;
    if (!device || !vertexPath || !fragmentPath) return false;
    std::vector<uint8_t> vsData = ReadFileBinaryLocal(vertexPath);
    std::vector<uint8_t> fsData = ReadFileBinaryLocal(fragmentPath);
    if (vsData.size() < 4 || fsData.size() < 4) return false;
    const uint32_t spirvMagic = 0x07230203u;
    if (*reinterpret_cast<uint32_t*>(vsData.data()) != spirvMagic || *reinterpret_cast<uint32_t*>(fsData.data()) != spirvMagic)
        return false;
    outVs = CreateShaderFromBinaryVK(device, ShaderStage::Vertex, vsData.data(), vsData.size());
    outFs = CreateShaderFromBinaryVK(device, ShaderStage::Fragment, fsData.data(), fsData.size());
    return (outVs && outFs && static_cast<ShaderVK*>(outVs)->IsValid() && static_cast<ShaderVK*>(outFs)->IsValid());
}

void DestroyShaderVK(DeviceVK* device, IShader* shader) {
    (void)device;
    if (!shader) return;
    delete static_cast<ShaderVK*>(shader);
}

} // namespace rhi
} // namespace render
} // namespace eyelib
