#pragma once

#include <cstdint>

namespace eyelib {
namespace rendering {
namespace rhi {

/** VkCommandPool with `VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT`.
 *  **Per-frame pools** (`FrameContextVK`) are reset in `DeviceVK::BeginFrame` only **after** `vkWaitForFences`
 *  for that ring slot—safe to reuse command buffers from that pool. **Standalone** pools (e.g. deferred one-shot
 *  lists) must not be reset while GPU may still execute buffers allocated from them. */
struct CommandPoolVK {
    void* handle = nullptr;  // VkCommandPool

    /** Create pool with RESET_COMMAND_BUFFER_BIT. Returns true on success. */
    static bool Create(CommandPoolVK* out, void* device, uint32_t queueFamilyIndex);
    /** Destroy pool. */
    static void Destroy(CommandPoolVK* pool, void* device);

    void* GetHandle() const { return handle; }
};

} // namespace rhi
} // namespace rendering
} // namespace eyelib
