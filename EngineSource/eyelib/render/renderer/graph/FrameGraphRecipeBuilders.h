#pragma once

#include "eyelib/render/framegraph/FrameGraph.h"
#include "eyelib/render/framegraph/FrameGraphRecipe.h"
#include "eyelib/render/rhi/RHI.h"
#include <functional>

namespace eyelib {
namespace render {

/** HDR + GBuffer + shadow textures for deferred frame graph imports. */
struct DeferredFrameGraphWiringInput {
    uint32_t width = 0;
    uint32_t height = 0;
    rhi::ITexture* hdrColor = nullptr;
    rhi::ITexture* hdrDepth = nullptr;
    rhi::ITexture* gbufferPos = nullptr;
    rhi::ITexture* gbufferNorm = nullptr;
    rhi::ITexture* gbufferAlb = nullptr;
    rhi::ITexture* gbufferPbr = nullptr;
    rhi::ITexture* gbufferVelocity = nullptr;
    rhi::ITexture* gbufferDepth = nullptr;
    rhi::ITexture* shadowDepth = nullptr;
    uint32_t shadowWidth = 0;
    uint32_t shadowHeight = 0;
};

void WireDeferredFrameGraph(FrameGraph& fg, const DeferredFrameGraphWiringInput& in,
                          const std::function<FrameGraph::ExecuteFunc(const std::string&)>& getExecutor);

/** HDR + shadow for forward / forward+ / stereo forward graphs. */
struct ForwardFrameGraphWiringInput {
    uint32_t width = 0;
    uint32_t height = 0;
    rhi::ITexture* hdrColor = nullptr;
    rhi::ITexture* shadowDepth = nullptr;
    uint32_t shadowWidth = 0;
    uint32_t shadowHeight = 0;
};

void WireForwardFrameGraph(FrameGraph& fg, const ForwardFrameGraphWiringInput& in, bool includeGpuCull,
                           const std::function<FrameGraph::ExecuteFunc(const std::string&)>& getExecutor);

void WireForwardPlusFrameGraph(FrameGraph& fg, const ForwardFrameGraphWiringInput& in,
                               const std::function<FrameGraph::ExecuteFunc(const std::string&)>& getExecutor);

void WireForwardStereoFrameGraph(FrameGraph& fg, const ForwardFrameGraphWiringInput& in,
                                 const std::function<FrameGraph::ExecuteFunc(const std::string&)>& getExecutor,
                                 const FrameGraph::ExecuteFunc& skyboxLeft,
                                 const FrameGraph::ExecuteFunc& skyboxRight);

}  // namespace render
}  // namespace eyelib
