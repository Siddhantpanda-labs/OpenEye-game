#pragma once

#include <cstddef>

namespace eyelib {
namespace core {
namespace allocator {

/** Base allocator interface. */
class IAllocator {
public:
    virtual ~IAllocator() = default;
    virtual void* Allocate(size_t size, size_t alignment = alignof(std::max_align_t)) = 0;
    virtual void Deallocate(void* ptr, size_t size) = 0;
    virtual void Reset() = 0;
    virtual size_t GetUsedMemory() const = 0;
    virtual size_t GetTotalMemory() const = 0;
};

} // namespace allocator
} // namespace core
} // namespace eyelib
