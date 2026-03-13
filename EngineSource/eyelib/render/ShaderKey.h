#pragma once

#include <cstdint>

namespace eyelib {
namespace render {

/** Pipeline stage (which pass / what we're rendering to). Used in ShaderKey. */
enum class PipelineStage : uint8_t {
    Forward,
    ForwardPlus,
    GBuffer,
    Depth,
    DirectionalShadow,
    Skybox,
    ColorPicker,
    DeferredLighting,  // full-screen; no geometry shader variant
};

/** Instance type (how geometry is drawn: static, skinned, instanced). Used in ShaderKey (Limitless-aligned). */
enum class InstanceType : uint8_t {
    Static,   // no bones / identity bones
    Skinned,  // bone matrices
    Instanced // per-instance model matrix from SSBO; DrawIndexedInstanced
};

/** Material type id for shader variant. Phase 1: all materials use kDefaultMaterialTypeId. */
using MaterialTypeId = uint64_t;

/** Default material type when no per-material variants exist yet. */
constexpr MaterialTypeId kDefaultMaterialTypeId = 0;

/**
 * Key for shader cache: (stage, instance type, material type).
 * Pass × instance type × material type → one compiled program.
 */
struct ShaderKey {
    PipelineStage stage = PipelineStage::GBuffer;
    InstanceType instanceType = InstanceType::Static;
    MaterialTypeId materialTypeId = kDefaultMaterialTypeId;

    bool operator<(const ShaderKey& rhs) const noexcept {
        if (stage != rhs.stage) return static_cast<uint8_t>(stage) < static_cast<uint8_t>(rhs.stage);
        if (instanceType != rhs.instanceType) return static_cast<uint8_t>(instanceType) < static_cast<uint8_t>(rhs.instanceType);
        return materialTypeId < rhs.materialTypeId;
    }
    bool operator==(const ShaderKey& rhs) const noexcept {
        return stage == rhs.stage && instanceType == rhs.instanceType && materialTypeId == rhs.materialTypeId;
    }
};

} // namespace render
} // namespace eyelib
