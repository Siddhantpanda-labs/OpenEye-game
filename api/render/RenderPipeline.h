#pragma once

#include "api/render/RenderDevice.h"
#include "api/render/RenderScene.h"
#include "eyelib/math/Types.h"
#include "api/render/RenderBackend.h"
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

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
    /** Initialize with a shared RHI device (caller owns device; use for multiple pipelines per window). */
    bool InitializeWithSharedDevice(RenderDevice device, uint32_t width, uint32_t height);
    void Resize(uint32_t width, uint32_t height);
    void Resize(const RenderTarget& target);
    void Shutdown();

    void SetMode(Mode mode);
    Mode GetMode() const;

    /** Render scene. When entityIdsForOpaque is non-null and size matches opaque drawable count, runs EntityID pass for GPU picking. */
    void Render(const RenderScene& scene, const std::vector<uint32_t>* entityIdsForOpaque = nullptr);
    /** Read entity ID at pixel (x,y) in render target coordinates, origin bottom-left. Call after Render(scene, ids) and Finish(). Returns true and writes ID to *outId; 0 = no entity. */
    bool ReadPixelEntityId(uint32_t x, uint32_t y, uint32_t* outId) const;
    /** Draw outline from entity ID buffer. Call after Render(scene, entityIds). selectedPackedId = 0 skips. RGB 0..1. */
    void RenderOutlineFromIdBuffer(uint32_t selectedPackedId, float r = 1.0f, float g = 0.5f, float b = 0.0f);
    /** Draw world-space debug lines with depth test (occluded by mesh). Draws into HDR. */
    void RenderDebugLines(const eyelib::math::Mat4& view, const eyelib::math::Mat4& proj,
        const std::vector<std::pair<eyelib::math::Vec3, eyelib::math::Vec3>>& lines, float r, float g, float b);
    /** Enqueue debug lines to be drawn before post in the next Render(). Use from scene view so lines are in HDR and get tone-mapped. */
    void SetPendingDebugLines(const eyelib::math::Mat4& view, const eyelib::math::Mat4& proj,
        const std::vector<std::pair<eyelib::math::Vec3, eyelib::math::Vec3>>& lines, float r, float g, float b);
    /** Enqueue debug lines with per-line color (RGB per line). */
    void SetPendingDebugLinesColored(const eyelib::math::Mat4& view, const eyelib::math::Mat4& proj,
        const std::vector<std::pair<std::pair<eyelib::math::Vec3, eyelib::math::Vec3>, eyelib::math::Vec4>>& linesWithColors);
    /** Reload all pipeline shaders (for hot-reload when shader files change). */
    void ReloadShaders();
    void BlitToScreen();
    /** Wait for GPU to finish (call before swap to reduce flicker). */
    void Finish();
    bool HasValidOutput() const;

    /** True when this pipeline owns the device (Initialize(Window&) was used). Application must run global teardown before Shutdown() when true. */
    bool OwnsDevice() const;
    /** Device handle for asset loading, other pipelines, etc. Null if not initialized. */
    RenderDevice GetDevice() const;

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
