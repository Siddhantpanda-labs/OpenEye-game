#pragma once

#include <cstddef>
#include <cstdint>

namespace eyelib {
namespace render {
namespace rhi {

/**
 * Standardized uniform resource layout for all backends.
 * Backends only execute; they do not decide what goes in UBO vs push constants.
 *
 * ---- UBO bindings (uniform buffer objects) ----
 * Use BindUniformBuffer(buffer, binding, offset, size). Shaders must use the same bindings.
 *
 *   0 - PerFrame   : Global frame data (time, resolution, etc.)
 *   1 - PerView    : View/projection, view position, etc.
 *   2 - PerMaterial: Material parameters
 *   3 - Bones      : Bone matrices for skinning (u_Bones). Block name "Bones", std140.
 *
 * ---- Push constants (small per-draw data) ----
 * Use SetPushConstants(offset, size, data). Total size must not exceed kPushConstantMaxSize.
 * Layout (all backends use the same offsets):
 *
 *   Offset 0  (64 bytes): mat4  u_Model (or first matrix)
 *   Offset 64 (4 bytes):  uint  drawId / instanceId (optional)
 *   Reserved up to kPushConstantMaxSize.
 *
 * Use push constants only for tiny, hot per-draw data (e.g. one mat4, one uint).
 * Everything else (bones, view/proj, materials, large arrays) goes in UBO.
 */
namespace UniformBinding {
    constexpr uint32_t PerFrame   = 0;
    constexpr uint32_t PerView    = 1;
    constexpr uint32_t PerMaterial = 2;
    constexpr uint32_t Bones      = 3;
}

/** Maximum size (bytes) of push constant block. Vulkan minimum is 128; keep <= 256 for portability. */
constexpr size_t kPushConstantMaxSize = 128u;

/** Standard bone uniform name and block name. Bones always go in UBO at binding Bones (3). */
constexpr const char* kBonesUniformName = "u_Bones";
constexpr const char* kBonesBlockName   = "Bones";

/**
 * Note: BGFX does not expose binding application-created buffers as UBO. The BGFX backend
 * follows the same RHI contract (bones buffer created/updated and BindUniformBuffer(3) called)
 * but the actual bind is a no-op until BGFX adds support. Use OpenGL for full skinned rendering.
 */

} // namespace rhi
} // namespace render
} // namespace eyelib
