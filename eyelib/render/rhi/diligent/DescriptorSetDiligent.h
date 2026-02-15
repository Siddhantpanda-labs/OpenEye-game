#pragma once

#include "eyelib/render/rhi/RHI.h"
#include "eyelib/render/rhi/diligent/PipelineLayoutDiligent.h"
#include <vector>

namespace eyelib {
namespace render {
namespace rhi {

struct DescriptorSetDiligent : IDescriptorSet {
    const PipelineLayoutDiligent* layout = nullptr;
    struct TextureBinding { uint32_t binding = 0; ITexture* texture = nullptr; };
    struct BufferBinding { uint32_t binding = 0; IBuffer* buffer = nullptr; uint32_t offset = 0; uint32_t size = 0; bool isStorage = false; };
    std::vector<TextureBinding> textures;
    std::vector<BufferBinding> buffers;

    void UpdateTexture(uint32_t binding, ITexture* texture) override;
    void UpdateUniformBuffer(uint32_t binding, IBuffer* buffer, uint32_t offset = 0, uint32_t size = 0) override;
    void UpdateStorageBuffer(uint32_t binding, IBuffer* buffer, uint32_t offset = 0, uint32_t size = 0) override;
};

} // namespace rhi
} // namespace render
} // namespace eyelib
