#pragma once

#include "api/rendering/RenderDevice.h"

namespace eyelib {
namespace api {
namespace rendering {

/**
 * @brief RAII Vulkan editor command-buffer slice scope on the new render module.
 *
 * Mirrors the legacy `openeye::editor::VulkanRenderSliceScope` shape but takes
 * the new façade's opaque `RenderDevice` handle. The implementation translates the
 * handle internally to `eyelib::rendering::rhi::IDevice*` and drives the same
 * push/pop transient-recording + `EnqueueFrameSliceCommandList` contract that the
 * legacy editor uses, so editor / inspector pipeline render calls stay isolated on
 * their own Vulkan primary command buffer (submitted before the compose buffer that
 * owns the ImGui swapchain blit).
 *
 * **Why this lives in `api/rendering`, not `app/editor`:** the editor MUST NOT
 * include `eyelib/rendering/rhi/...` directly (Hard constraint + LegacyRenderFirewall).
 * `api/rendering/*.cpp` is the only place outside `eyelib/rendering/` that may include
 * those headers. This scope is the corresponding public façade.
 *
 * No-op (constructs in inactive state) when:
 *  - `device.Get()` is null,
 *  - the underlying RHI backend is not Vulkan,
 *  - `IDevice::CreateCommandList()` returns null.
 *
 * Move-only; non-copyable.
 */
class RenderSliceScope {
public:
    RenderSliceScope() noexcept = default;
    explicit RenderSliceScope(RenderDevice device) noexcept;

    RenderSliceScope(const RenderSliceScope&) = delete;
    RenderSliceScope& operator=(const RenderSliceScope&) = delete;

    RenderSliceScope(RenderSliceScope&& other) noexcept;
    RenderSliceScope& operator=(RenderSliceScope&& other) noexcept;

    ~RenderSliceScope();

    /** @return true while the scope owns an active slice command buffer. */
    explicit operator bool() const noexcept { return m_active; }

private:
    void Release() noexcept;

    void* m_device = nullptr;  // erased eyelib::rendering::rhi::IDevice*
    void* m_cmd = nullptr;     // erased eyelib::rendering::rhi::ICommandList*
    bool  m_active = false;
};

} // namespace rendering
} // namespace api
} // namespace eyelib
