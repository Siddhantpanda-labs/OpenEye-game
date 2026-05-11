#pragma once

#include "PlayerConfig.h"
#include "api/platform/Window.h"
#include "api/world/All.h"
#include "eyelib/render/RenderBackend.h"

#include <string>
#include <cstddef>

namespace eyelib::api::rendering {
class RenderPipeline;
}

namespace openeye::player {

/** Data for the overlay UI filled by the game thread; consumed only by the render thread. */
struct OverlaySnapshot {
    float fps = 0.0f;
    float frameTimeMs = 0.0f;
    size_t entityCount = 0;
    std::string scenePath;

    /** VR streaming: when true, overlay may show VR server panel (QR, address, client count). */
    bool streamingActive = false;
    size_t streamingClientCount = 0;
    /** e.g. "192.168.1.42:27016"; empty if unknown. */
    std::string streamingServerAddress;
    /** When streaming and client connected: actual frames-sent/sec. 0 otherwise. */
    float streamingFps = 0.0f;
};

class RuntimeOverlay {
public:
    /** Legacy path. Initialize for OpenGL or Vulkan. For Vulkan, `device` must be `eyelib::render::rhi::IDevice*`
     *  (legacy stack — from `WorldRenderBridge::GetDevice().Get()`). Used when `kUseNewRenderPipeline == false`. */
    bool Initialize(eyelib::api::Window& window, eyelib::render::RenderBackend backend, void* device);

    /** M4.7 — new-API path. Initialize ImGui-Vulkan against the new pipeline's device.
     *  Pulls `eyelib::api::rendering::ImGuiVulkanInitInfo` via the façade and registers a
     *  post-blit callback so `ImGui_ImplVulkan_RenderDrawData` runs at the right point in
     *  the new module's frame graph. Used when `kUseNewRenderPipeline == true`. */
    bool Initialize(eyelib::api::Window& window, eyelib::api::rendering::RenderPipeline& pipeline);

    /** Render using snapshot only (render-thread safe; no World access). */
    void Render(const OverlaySnapshot& snapshot);
    void Shutdown();
    bool IsReady() const { return m_ready; }
    /** True when using Vulkan (overlay draw is recorded in post-blit callback). */
    bool IsVulkan() const { return m_vulkan; }

private:
    bool m_ready = false;
    bool m_vulkan = false;
    // QR code texture for VR server panel (only used when OPENEYE_BUILD_STREAMING)
    void* m_qrTextureId = nullptr;
    int m_qrTextureWidth = 0;
    int m_qrTextureHeight = 0;
    std::string m_lastQRAddress;
};

} // namespace openeye::player
