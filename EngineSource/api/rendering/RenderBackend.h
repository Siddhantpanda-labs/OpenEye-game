#pragma once

namespace eyelib {
namespace api {
namespace rendering {

/**
 * @brief Render backend selection for the next-gen rendering module.
 *
 * The new module (eyelib/rendering) is Vulkan-only by CMake (legacy OpenGL/Diligent paths
 * are excluded from the build). `Default` resolves to `Vulkan`. A future expansion may
 * re-introduce other backends; for now any non-Vulkan request falls back to Vulkan with
 * a one-time warning at the façade layer.
 *
 * This enum is intentionally NOT a re-export of the legacy `eyelib::render::RenderBackend`
 * — the API stays self-contained per the migration discipline (PIMPL, no leakage of
 * implementation namespaces). This enum is independent of the legacy render API.
 */
enum class RenderBackend {
    Vulkan,
    Default = Vulkan,
};

} // namespace rendering
} // namespace api
} // namespace eyelib
