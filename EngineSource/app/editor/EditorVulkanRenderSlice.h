#pragma once

#include "eyelib/render/rhi/RHI.h"

namespace openeye::editor {

/**
 * Records Scene/Game pipeline work into an isolated Vulkan primary command buffer, then enqueues it for
 * submission before the compose buffer (swapchain blit + ImGui). No-op when device is null or not Vulkan.
 */
class VulkanRenderSliceScope {
public:
    VulkanRenderSliceScope() = default;
    explicit VulkanRenderSliceScope(eyelib::render::rhi::IDevice* device);
    VulkanRenderSliceScope(const VulkanRenderSliceScope&) = delete;
    VulkanRenderSliceScope& operator=(const VulkanRenderSliceScope&) = delete;
    VulkanRenderSliceScope(VulkanRenderSliceScope&& other) noexcept;
    VulkanRenderSliceScope& operator=(VulkanRenderSliceScope&& other) noexcept;
    ~VulkanRenderSliceScope();

    explicit operator bool() const { return m_active; }

private:
    void Release();

    eyelib::render::rhi::IDevice* m_device = nullptr;
    eyelib::render::rhi::ICommandList* m_cmd = nullptr;
    bool m_active = false;
};

} // namespace openeye::editor
