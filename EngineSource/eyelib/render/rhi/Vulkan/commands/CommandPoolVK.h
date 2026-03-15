#pragma once

#include <cstdint>

namespace eyelib {
namespace render {
namespace rhi {

/** Thin wrapper over VkCommandPool. Used for deferred command list allocation. */
struct CommandPoolVK {
    void* handle = nullptr;  // VkCommandPool

    /** Create pool with RESET_COMMAND_BUFFER_BIT. Returns true on success. */
    static bool Create(CommandPoolVK* out, void* device, uint32_t queueFamilyIndex);
    /** Destroy pool. */
    static void Destroy(CommandPoolVK* pool, void* device);

    void* GetHandle() const { return handle; }
};

} // namespace rhi
} // namespace render
} // namespace eyelib
