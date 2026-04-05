#pragma once

namespace eyelib {
namespace render {

/** Deferred, forward, or clustered forward+; used by RenderPipeline and frame-graph compile scheduling. */
/** Hybrid: shadow → GBuffer (opaque) → deferred lighting → forward transparency → skybox (industry default). */
enum class PipelineMode { Hybrid, Forward, ForwardPlus };

}  // namespace render
}  // namespace eyelib
