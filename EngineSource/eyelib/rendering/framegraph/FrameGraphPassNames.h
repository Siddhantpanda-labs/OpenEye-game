#pragma once

namespace eyelib {
namespace rendering {

/**
 * Central constants for frame-graph pass `DebugName()` strings and profiler scopes (doc §11, §13).
 * Passes must return one of these from `IFrameGraphPass::DebugName()` instead of scattering literals.
 *
 * Keep the name set tight: add a constant when (and only when) a concrete pass is introduced.
 */
namespace pass_names {

inline constexpr const char* kClearBlackboardRenderTarget = "ClearBlackboardRenderTarget";
inline constexpr const char* kFullscreenHdrCompose        = "FullscreenHdrCompose";
inline constexpr const char* kBlitMainHdrToDefault        = "BlitMainHdrToDefault";
inline constexpr const char* kPublishOffscreenOutput      = "PublishOffscreenOutput";

inline constexpr const char* kDepthPrepass                = "DepthPrepass";
inline constexpr const char* kForwardOpaque               = "ForwardOpaque";
inline constexpr const char* kDirectionalShadow           = "DirectionalShadow";

/** Editor / debug overlay passes (only scheduled when `EditorFeature` is registered). */
inline constexpr const char* kEditorDebugLines            = "EditorDebugLines";

}  // namespace pass_names

}  // namespace rendering
}  // namespace eyelib
