#pragma once

#include "eyelib/render/rhi/RHI.h"
#include <cstddef>
#include <vector>

namespace eyelib {
namespace render {
namespace rhi {

struct BufferDiligent : IBuffer {
    ~BufferDiligent() override;
    size_t GetSize() const override;
    void* Map(size_t offset, size_t size) override;
    void Unmap() override;
    void Update(const void* data, size_t offset, size_t size) override;

    size_t m_size = 0;
    std::vector<uint8_t> m_cpuData;  // host-visible staging when no native buffer
    /** When non-null, holds Diligent::IBuffer* (owned; Release in dtor). Used for Vertex/Index and internal uniform buffers. */
    void* m_pNative = nullptr;
};

} // namespace rhi
} // namespace render
} // namespace eyelib
