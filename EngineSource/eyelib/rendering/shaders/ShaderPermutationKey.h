#pragma once

#include <cstdint>

namespace eyelib {
namespace rendering {
namespace shaders {

/**
 * Stable identity for a cached graphics pipeline permutation (doc §5, D7).
 * Features declare keys in `OnRegister`; pipelines are created only during explicit warm-up (e.g. first schedule).
 */
struct ShaderPermutationKey {
    enum class ProgramId : uint8_t {
        RasterComposeBootstrap = 0,
        /** Deferred rendering baseline: forward opaque, MSAA off, single directional key light. */
        RasterForwardOpaque = 1,
        /** Deferred rendering: G-Buffer output. */
        RasterGBufferOpaque = 2,
        /** Deferred rendering: Deferred lighting/resolve pass. */
        RasterDeferredLighting = 3,
        /** Editor debug-line overlay (line list, depth-tested, no depth write). */
        EditorDebugLines = 4,
        /** Sky pass: samples a `samplerCube` at the far-plane direction (per-scene env cubemap). */
        SkyboxCubemap = 5,
        /** Sky pass: analytic gradient sky shader (no cubemap input). */
        SkyboxProcedural = 6,
    };

    ProgramId program = ProgramId::RasterComposeBootstrap;
    /** Sample count for the color attachment (1 = MSAA off). */
    uint8_t msaaSamples = 1;
    /** Tier for directional lights in the forward path (baseline = 1). */
    uint8_t directionalLightCount = 1;

    constexpr uint32_t Pack() const noexcept {
        return (uint32_t(program) << 16) | (uint32_t(msaaSamples) << 8) | uint32_t(directionalLightCount);
    }

    constexpr bool operator==(ShaderPermutationKey o) const noexcept {
        return program == o.program && msaaSamples == o.msaaSamples && directionalLightCount == o.directionalLightCount;
    }
    constexpr bool operator!=(ShaderPermutationKey o) const noexcept { return !(*this == o); }
};

}  // namespace shaders
}  // namespace rendering
}  // namespace eyelib
