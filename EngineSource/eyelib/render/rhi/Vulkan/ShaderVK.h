#pragma once

#include "eyelib/render/rhi/RHI.h"

namespace eyelib {
namespace render {
namespace rhi {

struct DeviceVK;

struct ShaderVK : IShader {
    explicit ShaderVK(DeviceVK* device);
    ~ShaderVK() override;

    ShaderStage GetStage() const override;
    bool IsValid() const override;

    DeviceVK* m_device = nullptr;
    ShaderStage m_stage = ShaderStage::Vertex;
    // VkShaderModule (filled when shader system is implemented)
    void* m_module = nullptr;
};

} // namespace rhi
} // namespace render
} // namespace eyelib
