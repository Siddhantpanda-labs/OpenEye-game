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
 * ---- Binding model (Phase 3) ----
 * Prefer pipeline layout + descriptor sets: CreatePipelineLayout(desc), CreateDescriptorSet(layout),
 * UpdateTexture/UpdateUniformBuffer on the set, then BindDescriptorSet(cmd, set). Slot-based
 * BindTexture/BindUniformBuffer remain for legacy paths; new code should use layout-driven bindings.
 *
 * ---- UBO bindings (uniform buffer objects) ----
 * Use BindUniformBuffer(buffer, binding, offset, size). Shaders must use the same bindings.
 *
 *   0 - PerFrame   : Global frame data (time, resolution, etc.)
 *   1 - PerView    : View/projection, view position, etc.
 *   2 - PerMaterial: Material parameters
 *   3 - Bones      : Bone matrices for skinning (u_Bones). Block name "Bones", std140.
 *
 * ---- Texture bindings (when not using descriptor sets) ----
 * Shadow map: use PassBindings::ShadowMap (single canonical slot for lighting/shadow passes).
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

/** Canonical texture binding for shadow map in lighting/forward passes. Use this instead of hardcoded slot. */
namespace PassBindings {
    constexpr uint32_t ShadowMap = 5;
}

/**
 * ---- Compute and GPU-driven draw (Phase 4) ----
 * Compute: Use ICommandList::Dispatch(groupCountX, groupCountY, groupCountZ) with a compute pipeline.
 * Gated by RHICapabilities::supportsCompute. All compute goes through RHI; no ad hoc GL/Vulkan compute outside backends.
 *
 * Indirect draw: Use DrawArraysIndirect, DrawIndexedIndirect, MultiDrawArraysIndirect, MultiDrawIndexedIndirect
 * with a buffer containing DrawArraysIndirectCommand or DrawIndexedIndirectCommand (see RHITypes.h).
 * Gated by RHICapabilities::supportsMultiDrawIndirect. Buffer should have BufferUsage::IndirectDraw.
 *
 * GPU culling flow: (1) Dispatch compute to fill indirect buffer with draw commands. (2) BarrierBufferForIndirectDraw(cmd, indirectBuffer).
 * (3) BindPipeline(graphics), BindVertexBuffer, BindIndexBuffer, then MultiDrawIndexedIndirect(indirectBuffer, offset, drawCount, stride).
 *
 * Bindless: When GetCapabilities().supportsBindless is true, CreateBindlessHeap() can return a heap (skeleton currently returns nullptr).
 * Use AllocateTextureHandle/AllocateBufferHandle to get BindlessHandle values for shaders; BindBindlessHeap(cmd, heap) before draw.
 */

/** Maximum size (bytes) of push constant block. Vulkan minimum is 128; keep <= 256 for portability. */
constexpr size_t kPushConstantMaxSize = 128u;

/** Standard bone uniform name and block name. Bones always go in UBO at binding Bones (3). */
constexpr const char* kBonesUniformName = "u_Bones";
constexpr const char* kBonesBlockName   = "Bones";

/**
 * Bones uniform block name for skinned rendering (OpenGL/Diligent).
 */

} // namespace rhi
} // namespace render
} // namespace eyelib
