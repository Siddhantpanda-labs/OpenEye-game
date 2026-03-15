#pragma once

#include <cstdint>

namespace eyelib {
namespace render {
namespace rhi {

/** Holds graphics, compute, and transfer queue handles and their family indices. Opaque VkQueue. */
struct QueuesVK {
    void* graphics = nullptr;   // VkQueue
    void* compute = nullptr;   // VkQueue
    void* transfer = nullptr;  // VkQueue
    uint32_t graphicsFamily = 0;
    uint32_t computeFamily = 0;
    uint32_t transferFamily = 0;

    /** Get queues from device for the given family indices. Call after vkCreateDevice. Returns true on success. */
    static bool Create(QueuesVK* out, void* device, uint32_t graphicsFamily, uint32_t computeFamily, uint32_t transferFamily);
    /** No-op (queues are not destroyed). */
    static void Destroy(QueuesVK* queues, void* device);

    void* GetGraphicsQueue() const { return graphics; }
    void* GetComputeQueue() const { return compute; }
    void* GetTransferQueue() const { return transfer; }
    uint32_t GetGraphicsQueueFamily() const { return graphicsFamily; }
    uint32_t GetComputeQueueFamily() const { return computeFamily; }
    uint32_t GetTransferQueueFamily() const { return transferFamily; }
};

} // namespace rhi
} // namespace render
} // namespace eyelib
