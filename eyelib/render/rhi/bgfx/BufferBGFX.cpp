#include "eyelib/render/rhi/bgfx/BufferBGFX.h"
#include "eyelib/render/rhi/bgfx/DeviceBGFX.h"
#include "eyelib/render/rhi/RHITypes.h"
#include <bgfx/bgfx.h>
#include <cstring>

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
    if (!m_uniform || m_cpuData.empty() || !data) return;
    size_t writeSize = (size == 0) ? (m_size - offset) : size;
    if (offset + writeSize > m_cpuData.size()) return;
    std::memcpy(m_cpuData.data() + offset, data, writeSize);
}

} // namespace rhi
} // namespace render
} // namespace eyelib
