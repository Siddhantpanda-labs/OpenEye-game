#include "eyelib/rendering/rhi/Vulkan/resources/AccelerationStructureVK.h"
#include "eyelib/rendering/rhi/Vulkan/resources/BufferVK.h"
#include "eyelib/rendering/rhi/Vulkan/core/DeviceVK.h"
#include "eyelib/rendering/rhi/Vulkan/utils/DebugUtilsVK.h"
#include "eyelib/core/Log.h"
#include <vulkan/vulkan.h>

namespace eyelib {
namespace rendering {
namespace rhi {

AccelerationStructureVK::AccelerationStructureVK(DeviceVK* device) : m_device(device) {}

uint64_t AccelerationStructureVK::GetDeviceAddress() const {
    return m_retired ? 0u : m_deviceAddress;
}

void* AccelerationStructureVK::GetNativeAccelerationStructure() {
    return m_retired ? nullptr : m_as;
}

AccelerationStructureVK::~AccelerationStructureVK() {
    if (m_as) {
        core::Log::Error("AccelerationStructureVK: VkAccelerationStructureKHR leaked — call IDevice::DestroyAccelerationStructure.");
    }
}

IAccelerationStructure* CreateAccelerationStructureVK(DeviceVK* device, const AccelerationStructureDesc& desc) {
#if !defined(VK_KHR_acceleration_structure)
    (void)device;
    (void)desc;
    return nullptr;
#else
    if (!device || !device->SupportsRayQuery()) {
        core::Log::Error("CreateAccelerationStructureVK: ray query / acceleration structure slice is not enabled on this device.");
        return nullptr;
    }
    auto* pfnCreate = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(device->GetCreateAccelerationStructureKHR());
    auto* pfnGetAddress = reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(device->GetGetAccelerationStructureDeviceAddressKHR());
    if (!pfnCreate || !pfnGetAddress) {
        core::Log::Error("CreateAccelerationStructureVK: required Vulkan entry points are null.");
        return nullptr;
    }
    VkDevice vkDev = static_cast<VkDevice>(device->GetDevice());
    if (vkDev == VK_NULL_HANDLE) {
        core::Log::Error("CreateAccelerationStructureVK: VkDevice is null.");
        return nullptr;
    }

    auto* storage = desc.storageBuffer ? static_cast<BufferVK*>(desc.storageBuffer) : nullptr;
    if (!storage || !storage->m_buffer) {
        core::Log::Error("CreateAccelerationStructureVK: storageBuffer is null or not a valid Vulkan buffer.");
        return nullptr;
    }
    if (desc.sizeInBytes == 0) {
        core::Log::Error("CreateAccelerationStructureVK: sizeInBytes must be non-zero (from GetAccelerationStructureBuildSizes).");
        return nullptr;
    }
#if defined(VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR)
    if ((storage->m_vkUsage & VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR) == 0) {
        core::Log::Error(
            "CreateAccelerationStructureVK: storageBuffer must be created with `BufferUsage::AccelerationStructureStorage` "
            "(Vulkan: VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR).");
        return nullptr;
    }
#endif
    const VkDeviceSize storageOffset = static_cast<VkDeviceSize>(desc.storageOffset);
    const VkDeviceSize asSize = static_cast<VkDeviceSize>(desc.sizeInBytes);
    if (static_cast<uint64_t>(storageOffset) + static_cast<uint64_t>(asSize) > storage->GetSize()) {
        core::Log::Error("CreateAccelerationStructureVK: storage subrange exceeds buffer size.");
        return nullptr;
    }

    const uint32_t align = device->GetRayTracingScratchAlignment();
    if (align > 1u && (static_cast<uint64_t>(desc.storageOffset) % static_cast<uint64_t>(align)) != 0) {
        core::Log::ErrorF(
            "CreateAccelerationStructureVK: storageOffset (%zu) is not aligned to minAccelerationStructureScratchOffsetAlignment (%u).",
            desc.storageOffset,
            align);
        return nullptr;
    }

    VkAccelerationStructureTypeKHR vkType = VK_ACCELERATION_STRUCTURE_TYPE_MAX_ENUM_KHR;
    switch (desc.type) {
        case AccelerationStructureType::BottomLevel: vkType = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR; break;
        case AccelerationStructureType::TopLevel: vkType = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR; break;
    }

    VkAccelerationStructureCreateInfoKHR ci = {};
    ci.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    ci.pNext = nullptr;
    ci.createFlags = 0;
    ci.buffer = static_cast<VkBuffer>(storage->m_buffer);
    ci.offset = storageOffset;
    ci.size = asSize;
    ci.type = vkType;
    ci.deviceAddress = 0;

    VkAccelerationStructureKHR as = VK_NULL_HANDLE;
    const VkResult rc = pfnCreate(vkDev, &ci, nullptr, &as);
    if (rc != VK_SUCCESS || as == VK_NULL_HANDLE) {
        core::Log::ErrorF("CreateAccelerationStructureVK: vkCreateAccelerationStructureKHR failed (VkResult=%d).", static_cast<int>(rc));
        return nullptr;
    }

    VkAccelerationStructureDeviceAddressInfoKHR addrInfo = {};
    addrInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
    addrInfo.accelerationStructure = as;
    const VkDeviceAddress addr = pfnGetAddress(vkDev, &addrInfo);

    auto* out = new AccelerationStructureVK(device);
    out->m_type = desc.type;
    out->m_as = as;
    out->m_storageVkBuffer = storage->m_buffer;
    out->m_deviceAddress = static_cast<uint64_t>(addr);
    out->m_sizeInBytes = static_cast<uint64_t>(asSize);
    device->NoteAccelerationStructureCreatedOnBuffer(storage->m_buffer);
    if (desc.debugName && *desc.debugName)
        SetDebugNameAccelerationStructure(device, out->m_as, desc.debugName);
    return out;
#endif
}

} // namespace rhi
} // namespace rendering
} // namespace eyelib
