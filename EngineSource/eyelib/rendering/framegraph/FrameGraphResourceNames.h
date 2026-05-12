#pragma once

namespace eyelib {
namespace rendering {

/**
 * Central constants mirroring `BlackboardSlot` entries (doc §11). Intended for logging,
 * debug strings, and external tooling. The authoritative lookup is still the typed
 * `BlackboardSlot` enum; these strings must stay 1:1 with that enum.
 */
namespace resource_names {

inline constexpr const char* kMainHdrRenderTarget   = "MainHdrRenderTarget";
inline constexpr const char* kMainDepth             = "MainDepth";
inline constexpr const char* kGBufferRenderTarget   = "GBufferRenderTarget";
inline constexpr const char* kPerFrameUniformBuffer = "PerFrameUniformBuffer";
inline constexpr const char* kPerViewUniformBuffer  = "PerViewUniformBuffer";
inline constexpr const char* kPerObjectStorageBuffer = "PerObjectStorageBuffer";
inline constexpr const char* kMaterialTableStorageBuffer = "MaterialTableStorageBuffer";
inline constexpr const char* kDirectionalLightUniformBuffer = "DirectionalLightUniformBuffer";
inline constexpr const char* kSpotLightUniformBuffer = "SpotLightUniformBuffer";
inline constexpr const char* kIblIrradianceCubemap = "IblIrradianceCubemap";
inline constexpr const char* kIblPrefilterCubemap = "IblPrefilterCubemap";
inline constexpr const char* kIblBrdfLut = "IblBrdfLut";
inline constexpr const char* kDirectionalShadowMapArray = "DirectionalShadowMapArray";
inline constexpr const char* kSpotShadowMapAtlas = "SpotShadowMapAtlas";
inline constexpr const char* kPointShadowCubemapArray = "PointShadowCubemapArray";
inline constexpr const char* kShadowParamsBuffer = "ShadowParamsBuffer";
inline constexpr const char* kPointLightUniformBuffer = "PointLightUniformBuffer";

}  // namespace resource_names

/**
 * Central writer/owner identifiers used with:
 *   - `BlackboardWriterRegistry::TryReserve(slot, ownerName)`
 *   - `FrameGraphBlackboard::PublishRenderTarget(slot, writerName, rt)`
 *
 * The blackboard enforces single-writer semantics by exact string match (doc §3).
 * All publishers must use the constant declared here for their owning subsystem,
 * not inline string literals.
 */
namespace writer_names {

/** Core long-lived allocations (`RenderResources` publishes `MainHdrRenderTarget` each frame). */
inline constexpr const char* kRenderResources = "RenderResources";

/** First raster feature (Forward path). Reserved for future publishes (tone-mapped LDR, etc.). */
inline constexpr const char* kRasterFeature   = "Raster";

/** Default / bootstrap IBL textures (`IblDefaultProbes`); distinct from `RenderResources`. */
inline constexpr const char* kLightingIbl = "LightingIbl";
/** Shadow feature-owned resources (`features/shadows`). */
inline constexpr const char* kShadowFeature = "ShadowFeature";
/** Editor-only feature (`features/editor`): debug-line overlay, future entity-ID pass. Consumes
 *  only — does not own any blackboard slot today (line vertex buffer is feature-private). */
inline constexpr const char* kEditorFeature = "EditorFeature";
/** Sky feature (`features/raster/SkyboxFeature`): cubemap + procedural sky overlay onto
 *  `MainHdrRenderTarget`, plus the per-scene IBL probe pair. Owns `IblIrradianceCubemap`
 *  + `IblPrefilterCubemap` (baked from the user-supplied cubemap when `Kind::Cubemap`,
 *  fallback to `RasterFeature::GetDefaultEnvCubemap()` for procedural / none). The
 *  invariant-per-scene `IblBrdfLut` stays under `kLightingIbl` (`RasterFeature`). */
inline constexpr const char* kSkyboxFeature = "SkyboxFeature";

}  // namespace writer_names

}  // namespace rendering
}  // namespace eyelib
