#include "eyelib/render/rhi/diligent/BufferDiligent.h"
#if OPENEYE_USE_DILIGENT
#include "Graphics/GraphicsEngine/interface/Buffer.h"
#endif
#include <cstring>

namespace eyelib {
namespace render {
namespace rhi {

BufferDiligent::~BufferDiligent() {
#if OPENEYE_USE_DILIGENT
    if (m_pNative) {
        static_cast<Diligent::IBuffer*>(m_pNative)->Release();
        m_pNative = nullptr;
    }
#endif
}

size_t BufferDiligent::GetSize() const {
    return m_size;
}

void* BufferDiligent::Map(size_t offset, size_t size) {
    // Native GPU buffers (vertex/index) are not mappable without a context; use CPU staging only.
#if OPENEYE_USE_DILIGENT
    if (m_pNative) return nullptr;
#endif
    if (m_cpuData.empty()) return nullptr;
    size_t mapSize = (size == 0) ? (m_size - offset) : size;
    if (offset + mapSize > m_size) return nullptr;
    return m_cpuData.data() + offset;
}

void BufferDiligent::Unmap() {}

void BufferDiligent::Update(const void* data, size_t offset, size_t size) {
    if (!data || offset >= m_size) return;
    size_t copySize = (size == 0) ? (m_size - offset) : size;
    if (offset + copySize > m_size) copySize = m_size - offset;
    // Native vertex/index buffers are updated at creation only; uniform data goes through command list staging.
#if OPENEYE_USE_DILIGENT
    if (m_pNative) return;
#endif
    if (!m_cpuData.empty())
        std::memcpy(m_cpuData.data() + offset, data, copySize);
}

} // namespace rhi
} // namespace render
} // namespace eyelib
