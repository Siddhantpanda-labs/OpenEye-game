#pragma once

#include "api/rendering/RenderDevice.h"

namespace eyelib {
namespace api {
namespace rendering {

/**
 * @brief Lightweight client-facing renderer wrapper (no RHI types exposed).
 *
 * Holds a `RenderDevice` and provides minimal swapchain-level operations (currently:
 * `Clear`). Most rendering work goes through `RenderPipeline` (M1.4); this type exists
 * so editor/player call sites that previously held an `eyelib::api::Renderer` migrate
 * mechanically with only namespace + library substitution.
 *
 * Lifecycle:
 *   1. Construct (no GPU work).
 *   2. `SetDevice(device)` — usually obtained from `RenderPipeline::GetDevice()` after
 *      the pipeline initializes the device, or directly from a façade-side device
 *      creation helper added in M2.1 / M3 / M4 as needed.
 *   3. `Initialize()` — currently a no-op success; reserved for future per-renderer
 *      state setup. Returns false only on internal contract violation.
 *   4. `Clear(r,g,b,a)` — issues a default-framebuffer clear on the device's active
 *      recording command list (between `IDevice::BeginFrame` and `EndFrame`).
 *   5. `Shutdown()` — drops the device handle. Idempotent. Called by destructor.
 *
 * Thread safety: not thread-safe; one `Renderer` per render-thread context (matches
 * legacy semantics).
 */
class Renderer {
public:
    Renderer();
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    /** Set the device this renderer issues commands on. May be re-set; `Shutdown` clears it. */
    void SetDevice(RenderDevice device);

    /** No-op success in v1; reserved for future per-renderer initialization. */
    bool Initialize();

    /** Drop the device handle (idempotent). */
    void Shutdown();

    /**
     * Clear the default framebuffer (swapchain backbuffer) to the given RGBA colour.
     * Requires `SetDevice` to have been called with a valid device and the device to be
     * inside its `BeginFrame` / `EndFrame` window. No-op when no device is set.
     */
    void Clear(float r, float g, float b, float a);

private:
    class Impl;
    Impl* m_impl = nullptr;
};

} // namespace rendering
} // namespace api
} // namespace eyelib
