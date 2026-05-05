#pragma once

#include "eyelib/render/RenderTypes.h"
#include "eyelib/render/post/PostProcessor.h"
#include "eyelib/render/rhi/RHI.h"

namespace eyelib {
namespace render {

class GBuffer;

struct PostProcessPassBuildParams {
    PostProcessor* postProcessor = nullptr;
    const GBuffer* gbuffer = nullptr;
    rhi::ITexture* hdrTex = nullptr;
    rhi::ITexture* skyboxTex = nullptr;
    rhi::ITexture* probeTex = nullptr;
    CameraView camView{};
    math::Mat4 invView = math::Mat4(1.0f);
    math::Mat4 invProj = math::Mat4(1.0f);
    float time = 0.0f;
    float ssrNearPlane = 0.0f;
    float ssrFarPlane = 0.0f;
};

struct PostProcessPassExecuteParams {
    PostProcessor* postProcessor = nullptr;
    const GBuffer* gbuffer = nullptr;
    rhi::ITexture* hdrTex = nullptr;
    rhi::ITexture* skyboxTex = nullptr;
    rhi::ITexture* probeTex = nullptr;
    CameraView camView{};
    math::Mat4 invView = math::Mat4(1.0f);
    math::Mat4 invProj = math::Mat4(1.0f);
    float time = 0.0f;
    PostProcessor::Settings settings{};
};

class PostProcessPassSystem {
public:
    PostProcessPassExecuteParams BuildParams(const PostProcessPassBuildParams& params) const;
    bool Execute(const PostProcessPassExecuteParams& params) const;
};

}  // namespace render
}  // namespace eyelib

