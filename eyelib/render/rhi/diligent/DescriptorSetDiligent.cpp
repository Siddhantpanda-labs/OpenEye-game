#include "eyelib/render/rhi/diligent/DescriptorSetDiligent.h"

namespace eyelib {
namespace render {
namespace rhi {

void DescriptorSetDiligent::UpdateTexture(uint32_t binding, ITexture* texture) {
    for (auto& t : textures) {
        if (t.binding == binding) { t.texture = texture; return; }
    }
    textures.push_back({ binding, texture });
}

void DescriptorSetDiligent::UpdateUniformBuffer(uint32_t binding, IBuffer* buffer, uint32_t offset, uint32_t size) {
    for (auto& b : buffers) {
        if (b.binding == binding && !b.isStorage) {
            b.buffer = buffer; b.offset = offset; b.size = size;
            return;
        }
    }
    buffers.push_back({ binding, buffer, offset, size, false });
}

void DescriptorSetDiligent::UpdateStorageBuffer(uint32_t binding, IBuffer* buffer, uint32_t offset, uint32_t size) {
    for (auto& b : buffers) {
        if (b.binding == binding && b.isStorage) {
            b.buffer = buffer; b.offset = offset; b.size = size;
            return;
        }
    }
    buffers.push_back({ binding, buffer, offset, size, true });
}

} // namespace rhi
} // namespace render
} // namespace eyelib
