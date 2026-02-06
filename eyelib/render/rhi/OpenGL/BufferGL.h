#pragma once

#include "eyelib/render/rhi/RHI.h"

namespace eyelib {
namespace render {
namespace rhi {

struct BufferGL : IBuffer {
    size_t GetSize() const override;
    void* Map(size_t offset, size_t size) override;
    void Unmap() override;
    void Update(const void* data, size_t offset, size_t size) override;

    unsigned int m_handle = 0;
    size_t m_size = 0;
    BufferMemoryHint m_hint = BufferMemoryHint::DeviceOnly;
};

} // namespace rhi
} // namespace render
} // namespace eyelib
