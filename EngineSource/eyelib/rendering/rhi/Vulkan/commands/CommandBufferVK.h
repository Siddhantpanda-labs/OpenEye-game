#pragma once

namespace eyelib {
namespace rendering {
namespace rhi {

/** Thin wrapper over VkCommandBuffer (primary). */
struct CommandBufferVK {
    void* handle = nullptr;  // VkCommandBuffer

    /** Allocate one primary command buffer from the pool. Returns true on success. */
    static bool Allocate(CommandBufferVK* out, void* device, void* commandPool);
    /** Free the command buffer. Pool must still be valid. */
    static void Free(CommandBufferVK* buf, void* device, void* commandPool);

    void* GetHandle() const { return handle; }
};

} // namespace rhi
} // namespace rendering
} // namespace eyelib
