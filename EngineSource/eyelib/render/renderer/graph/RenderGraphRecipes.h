#pragma once

#include "eyelib/render/framegraph/FrameGraphRecipe.h"

namespace eyelib {
namespace render {

FrameGraphRecipe BuildDeferredRenderGraphRecipe();
FrameGraphRecipe BuildForwardRenderGraphRecipe(bool includeGpuCull);
FrameGraphRecipe BuildForwardPlusRenderGraphRecipe();
/** Stereo forward: shared shadow + split viewports; SkyboxLeft/Right passes are appended after this recipe in the builder. */
FrameGraphRecipe BuildForwardStereoRenderGraphRecipe();

}  // namespace render
}  // namespace eyelib

