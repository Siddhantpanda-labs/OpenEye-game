#pragma once

namespace eyelib {
namespace render {

/** Deferred, forward, or clustered forward+; used by RenderPipeline and frame-graph compile scheduling. */
enum class PipelineMode { Forward, Deferred, ForwardPlus };

}  // namespace render
}  // namespace eyelib
