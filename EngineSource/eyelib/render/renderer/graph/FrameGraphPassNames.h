#pragma once

namespace eyelib {
namespace render {
namespace FrameGraphPassNames {

/** String literals must match `RenderGraphRecipes` / `FrameGraphRecipeBuilders` pass names. */

inline constexpr const char* kGpuCull = "GPU Cull";
inline constexpr const char* kShadow = "Shadow";
inline constexpr const char* kGBuffer = "GBuffer";
inline constexpr const char* kCopyDepthOcclusion = "Copy depth for occlusion";
inline constexpr const char* kLighting = "Lighting";
inline constexpr const char* kTransparency = "Transparency";
inline constexpr const char* kSkybox = "Skybox";
inline constexpr const char* kForwardOpaque = "Forward Opaque";
inline constexpr const char* kForwardPlusOpaque = "Forward+ Opaque";

/** Stereo wiring appends these after the base stereo recipe (not in `BuildForwardStereoRenderGraphRecipe`). */
inline constexpr const char* kSkyboxLeft = "SkyboxLeft";
inline constexpr const char* kSkyboxRight = "SkyboxRight";

}  // namespace FrameGraphPassNames
}  // namespace render
}  // namespace eyelib
