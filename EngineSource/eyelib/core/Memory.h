#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdlib>

#ifdef _WIN32
#include <malloc.h>
#endif

namespace eyelib {
namespace core {

/** Align pointer up to alignment boundary. */
inline void* AlignUp(void* ptr, size_t alignment) {
    uintptr_t p = reinterpret_cast<uintptr_t>(ptr);
    uintptr_t aligned = (p + alignment - 1) & ~(alignment - 1);
    return reinterpret_cast<void*>(aligned);
}

/** Align pointer down to alignment boundary. */
inline void* AlignDown(void* ptr, size_t alignment) {
    uintptr_t p = reinterpret_cast<uintptr_t>(ptr);
    uintptr_t aligned = p & ~(alignment - 1);
    return reinterpret_cast<void*>(aligned);
}

/** Check if pointer is aligned. */
inline bool IsAligned(void* ptr, size_t alignment) {
    return (reinterpret_cast<uintptr_t>(ptr) & (alignment - 1)) == 0;
}

/** Allocate aligned memory. Returns nullptr on failure. */
inline void* AllocateAligned(size_t size, size_t alignment) {
    void* ptr = nullptr;
#ifdef _WIN32
    ptr = _aligned_malloc(size, alignment);
#else
    if (posix_memalign(&ptr, alignment, size) != 0)
        ptr = nullptr;
#endif
    return ptr;
}

/** Free aligned memory. */
inline void FreeAligned(void* ptr) {
    if (!ptr) return;
#ifdef _WIN32
    _aligned_free(ptr);
#else
    free(ptr);
#endif
}

/** Memory copy. */
inline void MemoryCopy(void* dest, const void* src, size_t count) {
    std::memcpy(dest, src, count);
}

/** Memory move. */
inline void MemoryMove(void* dest, const void* src, size_t count) {
    std::memmove(dest, src, count);
}

/** Memory set. */
inline void MemorySet(void* dest, int value, size_t count) {
    std::memset(dest, value, count);
}

} // namespace core
} // namespace eyelib
