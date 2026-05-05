#pragma once

namespace eyelib {
namespace api {

/**
 * @brief Opaque handle to the RHI device used for rendering.
 * Pass from pipeline (GetDevice()) to asset registry, IBL, renderer, etc.
 * Implementation may cast to rhi::IDevice* internally.
 */
struct RenderDevice {
    RenderDevice() : m_handle(nullptr) {}
    explicit RenderDevice(void* handle) : m_handle(handle) {}

    /** For API implementation use only. */
    void* Get() const { return m_handle; }
    explicit operator bool() const { return m_handle != nullptr; }

private:
    void* m_handle;
};

} // namespace api
} // namespace eyelib
