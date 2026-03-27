#pragma once

#include "eyelib/render/framegraph/FrameGraphRecipe.h"

namespace eyelib {
namespace render {

FrameGraphRecipe BuildDeferredRenderGraphRecipe();
FrameGraphRecipe BuildForwardRenderGraphRecipe(bool includeGpuCull);
FrameGraphRecipe BuildForwardPlusRenderGraphRecipe();

}  // namespace render
}  // namespace eyelib

