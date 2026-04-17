#pragma once

#include "eyelib/rendering/rhi/RHI.h"

namespace eyelib {
namespace rendering {
namespace rhi {

struct DeviceVK;

struct AccelerationStructureVK : IAccelerationStructure {
    explicit AccelerationStructureVK(DeviceVK* device);
    ~AccelerationStructureVK() override;

    AccelerationStructureType GetAccelerationStructureType() const override { return m_type; }
    uint64_t GetDeviceAddress() const override;
    uint64_t GetSizeInBytes() const override { return m_sizeInBytes; }
    void* GetNativeAccelerationStructure() override;

    DeviceVK* m_device = nullptr;
    AccelerationStructureType m_type = AccelerationStructureType::BottomLevel;
    void* m_as = nullptr; // VkAccelerationStructureKHR
    /** `VkBuffer` that owns the AS storage subrange (for device refcount / destroy ordering); null after destroy. */
    void* m_storageVkBuffer = nullptr;
    uint64_t m_deviceAddress = 0;
    uint64_t m_sizeInBytes = 0;
    /** True after `DestroyAccelerationStructure`; native handle is queued to `DeletionQueueVK`, wrapper delete is deferred like textures. */
    bool m_retired = false;
};

IAccelerationStructure* CreateAccelerationStructureVK(DeviceVK* device, const AccelerationStructureDesc& desc);

} // namespace rhi
} // namespace rendering
} // namespace eyelib
