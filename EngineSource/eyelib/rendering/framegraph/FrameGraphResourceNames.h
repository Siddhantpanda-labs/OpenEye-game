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
inline constexpr const char* kDirectionalLightUniformBuffer = "DirectionalLightUniformBuffer";

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

}  // namespace writer_names

}  // namespace rendering
}  // namespace eyelib
