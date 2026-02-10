#include "eyelib/render/rhi/bgfx/BufferBGFX.h"
#include "eyelib/render/rhi/bgfx/DeviceBGFX.h"
#include "eyelib/render/rhi/RHITypes.h"
#include <bgfx/bgfx.h>

namespace eyelib {
namespace render {
namespace rhi {

BufferBGFX::~BufferBGFX() = default;

size_t BufferBGFX::GetSize() const { return m_size; }

void* BufferBGFX::Map(size_t offset, size_t size) {
    (void)offset;
    (void)size;
    return nullptr;
}

void BufferBGFX::Unmap() {}

void BufferBGFX::Update(const void* data, size_t offset, size_t size) {
    (void)data;
    (void)offset;
    (void)size;
}

} // namespace rhi
} // namespace render
} // namespace eyelib
