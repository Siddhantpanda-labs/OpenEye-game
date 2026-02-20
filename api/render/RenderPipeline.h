#pragma once

#include "api/render/RenderScene.h"
#include "eyelib/render/RenderBackend.h"
#include <cstdint>
#include <memory>

namespace eyelib {
namespace api {

class Window;
class RenderTarget;

/**
 * @brief Render pipeline API (opaque). Initialize, set mode, render scene.
 * No OpenGL types exposed. Single init path: pass window and backend (e.g. from --renderer).
 */
class RenderPipeline {
public:
    enum class Mode { Forward, Deferred, ForwardPlus };

    RenderPipeline();
    ~RenderPipeline();

    RenderPipeline(const RenderPipeline&) = delete;
    RenderPipeline& operator=(const RenderPipeline&) = delete;
    RenderPipeline(RenderPipeline&& other) noexcept;
    RenderPipeline& operator=(RenderPipeline&& other) noexcept;

    /** Create device and pipeline. Requires a window; backend chosen at runtime (e.g. --renderer opengl|diligent). */
    bool Initialize(Window& window, uint32_t width, uint32_t height,
                    ::eyelib::render::RenderBackend backend = ::eyelib::render::RenderBackend::Default);
    void Resize(uint32_t width, uint32_t height);
    void Resize(const RenderTarget& target);
    void Shutdown();

    void SetMode(Mode mode);
    Mode GetMode() const;

    /** Render scene. */
    void Render(const RenderScene& scene);
    void BlitToScreen();
    /** Wait for GPU to finish (call before swap to reduce flicker). */
    void Finish();
    bool HasValidOutput() const;

    /** Opaque device handle for creating RHI resources (e.g. RenderTarget). Null if not initialized. */
    void* GetDevice() const;

    /** Native texture handle of pipeline output (after Render). For ImGui: use as ImTextureID. OpenGL: texture ID; null if not ready. */
    void* GetOutputColorTextureNativeHandle() const;

    /** Restore default framebuffer (0) and viewport. Call after Render() when not calling BlitToScreen() (e.g. editor scene view) so subsequent drawing (e.g. ImGui) targets the window. */
    void RestoreDefaultFramebufferAndViewport(uint32_t width, uint32_t height);

    /** Insert a GPU fence after current command stream. Call before Finish() so fence is after all draws. Returns opaque fence (null if unsupported). */
    void* InsertFrameFence();
    /** Wait for fence (no destroy). Call before SwapBuffers so present happens after GPU completes. No-op if fence is null. */
    void WaitFence(void* fence);
    /** Wait for fence then destroy it. Call after SwapBuffers before reusing frame slot. No-op if fence is null. */
    void WaitAndDestroyFence(void* fence);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace api
} // namespace eyelib
