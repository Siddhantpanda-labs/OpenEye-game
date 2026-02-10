#pragma once

#include "eyelib/render/rhi/RHI.h"
#include <cstdint>

namespace eyelib {
namespace render {
namespace rhi {

struct DeviceBGFX;

struct BufferBGFX : IBuffer {
    ~BufferBGFX() override;
    size_t GetSize() const override;
    void* Map(size_t offset, size_t size) override;
    void Unmap() override;
    void Update(const void* data, size_t offset, size_t size) override;

    static constexpr uint16_t kInvalidIdx = 0xffff;
    uint16_t m_vbIdx = kInvalidIdx;
    uint16_t m_ibIdx = kInvalidIdx;
    size_t m_size = 0;
    BufferMemoryHint m_hint = BufferMemoryHint::DeviceOnly;
    bool m_vertex = false;
    bool m_index = false;
};

} // namespace rhi
} // namespace render
} // namespace eyelib
