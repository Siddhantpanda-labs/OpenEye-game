#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace eyelib {
namespace rendering {
namespace rhi {

/**
 * Ring staging buffer for CPU→GPU uploads with per-frame regions.
 * Persistently mapped; each frame has a fixed region. BeginFrame(device, fence, frameIndex) waits fence and resets that frame's region.
 * Allocate(frameIndex, ...) suballocates from the frame's region — no stall when ring would wrap (frame 0/1/2 upload in parallel).
 */
struct UploadHeapVK {
    static constexpr uint32_t kMaxFrames = 3;

    /** Create staging buffer (TRANSFER_SRC, host-visible, persistently mapped). bufferSizeInBytes is total; divided into kMaxFrames regions. */
    static bool Create(UploadHeapVK* out, void* allocator, size_t bufferSizeInBytes);
    /** Destroy staging buffer. Pass the same allocator used in Create. */
    static void Destroy(UploadHeapVK* heap, void* allocator);

    /** Wait fence (GPU finished with this frame's region) and reset that frame's write head. Call once per frame before Allocate for that frame. */
    static void BeginFrame(UploadHeapVK* heap, void* device, void* fence, uint32_t frameIndex);

    /**
     * Suballocate from the given frame's region. alignment is applied (e.g. 256 for UBO).
     * frameIndex must be the current frame. Returns true and sets *outOffset and *outMappedPtr; false if no room in this frame's region.
     */
    static bool Allocate(UploadHeapVK* heap, uint32_t frameIndex, size_t size, size_t alignment, size_t* outOffset, void** outMappedPtr);

    /** VkBuffer to use as source in vkCmdCopyBuffer / copy to image. */
    void* GetBuffer() const { return m_buffer; }
    size_t GetSize() const { return m_size; }

private:
    void* m_buffer = nullptr;
    void* m_allocation = nullptr;
    void* m_mapped = nullptr;
    size_t m_size = 0;
    size_t m_regionSize = 0;  // m_size / kMaxFrames
    std::array<size_t, kMaxFrames> m_writeHeadPerFrame = {};
};

} // namespace rhi
} // namespace rendering
} // namespace eyelib
