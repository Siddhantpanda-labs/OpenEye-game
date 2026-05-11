#pragma once

#include <cstdint>

namespace eyelib {
namespace api {
namespace rendering {

/**
 * @brief Public POD mirror of `eyelib::rendering::rhi::ImGuiVulkanInitInfo` (M4.7).
 *
 * Shape is **field-for-field identical** to the internal struct so the façade adapter is a
 * trivial copy. Field types stay `void*` / `uint32_t` (no `vulkan/vulkan.h` exposure on the
 * public surface — clients cast back to `VkInstance` / `VkDevice` etc. at the ImGui glue site).
 *
 * **Why a public POD when the internal struct is already POD?**
 * The internal type lives in `eyelib::rendering::rhi::*` and `api/rendering/*.h` must not
 * leak that namespace (D2 boundary discipline). Mirroring it as a public POD here keeps
 * `RuntimeOverlay` / editor `ScopedImGui` self-contained while preserving the fast path.
 *
 * Producer: `RenderPipeline::GetImGuiVulkanInitInfo(out)` calls into the device.
 * Consumer: `RuntimeOverlay::Initialize(window, pipeline)` (player) / `ScopedImGui::Initialize`
 * (editor M3.9) — both pass the fields verbatim into `ImGui_ImplVulkan_InitInfo`.
 */
struct ImGuiVulkanInitInfo {
    void*    instance        = nullptr;
    void*    physical_device = nullptr;
    void*    device          = nullptr;
    uint32_t queue_family    = 0;
    void*    queue           = nullptr;
    void*    pipeline_cache  = nullptr;
    void*    descriptor_pool = nullptr;
    uint32_t min_image_count = 2;
    uint32_t image_count     = 0;
    void*    render_pass     = nullptr;
};

} // namespace rendering
} // namespace api
} // namespace eyelib
