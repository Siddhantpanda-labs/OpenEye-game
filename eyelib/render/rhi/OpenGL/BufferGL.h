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

    /** Call before destroying buffer if persistent and ever mapped (DeviceGL::DestroyBuffer). */
    void UnmapPersistent();

    unsigned int m_handle = 0;
    size_t m_size = 0;
    BufferMemoryHint m_hint = BufferMemoryHint::DeviceOnly;
    bool m_persistent = false;       // created with GL_MAP_PERSISTENT_BIT
    void* m_persistentPtr = nullptr; // non-null while persistently mapped
    bool m_sparse = false;          // created with GL_SPARSE_STORAGE_BIT_ARB
};

} // namespace rhi
} // namespace render
} // namespace eyelib
