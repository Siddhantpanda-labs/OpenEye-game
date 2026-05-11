#pragma once

namespace eyelib {
namespace api {
namespace rendering {

/**
 * @brief Opaque handle to the RHI device used for rendering.
 *
 * Pass from `RenderPipeline::GetDevice()` (added in M1.4) to other façade objects
 * (asset registry GPU resolver, IBL helper, secondary pipelines, ImGui-Vulkan glue).
 * The implementation casts internally to `eyelib::rendering::rhi::IDevice*`; clients
 * MUST treat the handle as opaque and use the public façade APIs only.
 *
 * Mirrors the shape of legacy `eyelib::api::RenderDevice` so editor/player migration
 * is mechanical: existing call sites that pass a handle around continue to compile
 * with only namespace/type substitution.
 */
struct RenderDevice {
    RenderDevice() noexcept : m_handle(nullptr) {}
    explicit RenderDevice(void* handle) noexcept : m_handle(handle) {}

    /** For façade implementation use only (`api/rendering/*.cpp`). */
    void* Get() const noexcept { return m_handle; }
    explicit operator bool() const noexcept { return m_handle != nullptr; }

private:
    void* m_handle;
};

} // namespace rendering
} // namespace api
} // namespace eyelib
