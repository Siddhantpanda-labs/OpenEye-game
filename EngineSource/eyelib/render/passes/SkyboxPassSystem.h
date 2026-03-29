#pragma once

#include "eyelib/render/PipelineSharedResources.h"
#include "eyelib/render/RenderTypes.h"
#include "eyelib/render/rhi/RHI.h"
#include <array>
#include <memory>

namespace eyelib {
namespace render {

class Shader;
class TextureCube;

struct SkyboxPassBuildParams {
    Shader* shader = nullptr;
    rhi::IDevice* device = nullptr;
    rhi::ICommandList* cmd = nullptr;
    rhi::IRenderTarget* renderTarget = nullptr;
    rhi::IRenderTarget* hdrRenderTarget = nullptr;
    rhi::IRenderTarget* gbufferRenderTarget = nullptr;
    rhi::IDescriptorSet* frameSet = nullptr;
    rhi::IDescriptorSet* postSet = nullptr;
    CameraView camera{};
    uint32_t viewportWidth = 0;
    uint32_t viewportHeight = 0;
    bool useGbufferDepth = false;
    bool isVulkan = false;
};

struct SkyboxPassExecuteParams {
    SkyboxPassBuildParams build{};
    math::Mat4 inverseView = math::Mat4(1.0f);
    math::Mat4 inverseProjection = math::Mat4(1.0f);
};

/** Per-frame inputs for cubemap / procedural skybox draw (orchestrator fills from pipeline state). */
struct SkyboxDrawContext {
    rhi::IDevice* device = nullptr;
    rhi::ICommandList* cmd = nullptr;
    CameraView camera{};
    rhi::IRenderTarget* renderTarget = nullptr;
    rhi::IRenderTarget* hdrRenderTarget = nullptr;
    rhi::IRenderTarget* gbufferRenderTarget = nullptr;
    rhi::IDescriptorSet* frameSet = nullptr;
    uint32_t viewportWidth = 0;
    uint32_t viewportHeight = 0;
    bool useGbufferDepth = false;
    bool isVulkan = false;
};

class SkyboxPassSystem {
public:
    void SetDevice(rhi::IDevice* device);
    void SetSharedResources(PipelineSharedResources* ownedResources, PipelineSharedResources* usingShared);
    void EnsureShaders();
    void ReloadShaders();
    /** Call while device is still valid (e.g. RenderPipeline destructor before clearing device). */
    void Shutdown();

    void EnsurePostSetFilled(rhi::ITexture* binding0Tex);

    bool RenderCubemap(const SkyboxDrawContext& ctx, const std::shared_ptr<TextureCube>& skybox);
    bool RenderProcedural(const SkyboxDrawContext& ctx);

    SkyboxPassExecuteParams BuildParams(const SkyboxPassBuildParams& params) const;
    bool ExecuteCubemap(const SkyboxPassExecuteParams& params);
    bool ExecuteProcedural(const SkyboxPassExecuteParams& params);

private:
    void EnsureCubemapShader();
    void EnsureProceduralShader();
    void EnsureDummyTexture();

    rhi::IDevice* m_device = nullptr;
    PipelineSharedResources* m_ownedResources = nullptr;
    PipelineSharedResources* m_usingShared = nullptr;

    std::shared_ptr<Shader> m_skyboxShader;
    std::shared_ptr<Shader> m_proceduralSkyboxShader;

    static constexpr uint32_t kPostSetsFrames = 3;
    static constexpr uint32_t kPostPassBindingCount = 7;
    std::array<rhi::IDescriptorSet*, kPostSetsFrames> m_postSets = {};
    uint32_t m_lastPostSetFilledFrameNumber = 0xffffffffu;
    rhi::ITexture* m_cachedSkyboxTexture = nullptr;
    rhi::ITexture* m_skyboxDummyTexture = nullptr;
};

}  // namespace render
}  // namespace eyelib
