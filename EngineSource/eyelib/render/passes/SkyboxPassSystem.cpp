#include "eyelib/render/passes/SkyboxPassSystem.h"
#include "eyelib/core/Log.h"
#include "eyelib/render/PassPushConstants.h"
#include "eyelib/render/RenderBackend.h"
#include "eyelib/render/backend/RenderBackendPolicy.h"
#include "eyelib/render/resources/Mesh.h"
#include "eyelib/render/resources/Shader.h"
#include "eyelib/render/resources/TextureCube.h"
#include "eyelib/render/rhi/PipelineAttachmentFormatSets.h"
#include <glm/gtc/matrix_inverse.hpp>

namespace eyelib {
namespace render {

void SkyboxPassSystem::SetDevice(rhi::IDevice* device) {
    m_device = device;
}

void SkyboxPassSystem::SetSharedResources(PipelineSharedResources* ownedResources, PipelineSharedResources* usingShared) {
    m_ownedResources = ownedResources;
    m_usingShared = usingShared;
}

void SkyboxPassSystem::EnsureCubemapShader() {
    if (m_skyboxShader && m_skyboxShader->IsValid()) return;
    if (!m_device) return;
    if (m_usingShared && m_usingShared->skyboxShader && m_usingShared->skyboxShader->IsValid()) {
        m_skyboxShader = m_usingShared->skyboxShader;
        m_cachedSkyboxTexture = nullptr;
        return;
    }
    m_skyboxShader = std::make_shared<Shader>();
    rhi::DepthStencilState skyboxDepth;
    skyboxDepth.depthTest = true;
    skyboxDepth.depthWrite = false;
    skyboxDepth.depthCompare = rhi::DepthCompare::LessEqual;
    if (!m_skyboxShader->Load(m_device, "passes/sky/skybox", Mesh::GetFullscreenQuadVertexLayoutRHI(), rhi::PrimitiveTopology::TriangleList, &skyboxDepth, rhi::PipelineLayoutKind::Post,
                              std::optional{::eyelib::render::rhi::StandardAttachmentFormats::PostHDRWithDepth()})) {
        core::Log::WarnF("Skybox shader failed to load (passes/sky/skybox). Run from project root or ensure assets/shaders/opengl/ is available.");
        return;
    }
    if (!m_skyboxShader->IsValid()) {
        core::Log::Warn("Skybox shader invalid after load.");
        return;
    }
    if (m_ownedResources) m_ownedResources->skyboxShader = m_skyboxShader;
    m_cachedSkyboxTexture = nullptr;
}

void SkyboxPassSystem::EnsureProceduralShader() {
    if (m_proceduralSkyboxShader && m_proceduralSkyboxShader->IsValid()) return;
    if (!m_device) return;
    if (m_usingShared && m_usingShared->proceduralSkyboxShader && m_usingShared->proceduralSkyboxShader->IsValid()) {
        m_proceduralSkyboxShader = m_usingShared->proceduralSkyboxShader;
        return;
    }
    m_proceduralSkyboxShader = std::make_shared<Shader>();
    rhi::DepthStencilState skyboxDepth;
    skyboxDepth.depthTest = true;
    skyboxDepth.depthWrite = false;
    skyboxDepth.depthCompare = rhi::DepthCompare::LessEqual;
    if (!m_proceduralSkyboxShader->Load(m_device, "passes/sky/skybox_procedural", Mesh::GetFullscreenQuadVertexLayoutRHI(), rhi::PrimitiveTopology::TriangleList, &skyboxDepth, rhi::PipelineLayoutKind::Post,
                                        std::optional{::eyelib::render::rhi::StandardAttachmentFormats::PostHDRWithDepth()})) {
        core::Log::WarnF("Procedural skybox shader failed to load (passes/sky/skybox_procedural).");
        return;
    }
    if (!m_proceduralSkyboxShader->IsValid()) {
        core::Log::Warn("Procedural skybox shader invalid after load.");
        return;
    }
    if (m_ownedResources) m_ownedResources->proceduralSkyboxShader = m_proceduralSkyboxShader;
}

void SkyboxPassSystem::EnsureShaders() {
    EnsureCubemapShader();
    EnsureProceduralShader();
}

void SkyboxPassSystem::ReloadShaders() {
    if (m_skyboxShader) m_skyboxShader->Reload();
    if (m_proceduralSkyboxShader) m_proceduralSkyboxShader->Reload();
}

void SkyboxPassSystem::Shutdown() {
    if (m_device) {
        for (rhi::IDescriptorSet*& set : m_postSets) {
            if (set) {
                m_device->EnqueueDestroyDescriptorSet(set);
                set = nullptr;
            }
        }
    }
    m_cachedSkyboxTexture = nullptr;
    if (m_skyboxDummyTexture && m_device) {
        m_device->DestroyTexture(m_skyboxDummyTexture);
        m_skyboxDummyTexture = nullptr;
    }
    m_lastPostSetFilledFrameNumber = 0xffffffffu;
}

void SkyboxPassSystem::EnsureDummyTexture() {
    if (m_skyboxDummyTexture || !m_device) return;
    rhi::TextureDesc desc = {};
    desc.type = rhi::TextureType::Texture2D;
    desc.width = 1;
    desc.height = 1;
    desc.format = rhi::RHITextureFormat::RGBA8;
    desc.usage = rhi::TextureUsage::ShaderResource;
    desc.mipLevels = 1;
    static const uint8_t black[4] = { 0, 0, 0, 255 };
    m_skyboxDummyTexture = m_device->CreateTexture(desc, black);
}

void SkyboxPassSystem::EnsurePostSetFilled(rhi::ITexture* binding0Tex) {
    if (!m_device) return;
    const uint32_t frameIndex = m_device->GetCurrentFrameIndex() % kPostSetsFrames;
    const uint32_t frameNumber = m_device->GetFrameNumber();
    rhi::IDescriptorSet*& setRef = m_postSets[frameIndex];
    if (!setRef) {
        rhi::IPipelineLayout* postLayout = m_device->GetResourceSetLayout(rhi::IDevice::ResourceSetType::PostPass);
        if (!postLayout) return;
        setRef = m_device->CreateDescriptorSet(postLayout);
        if (!setRef) return;
    }
    rhi::ITexture* expectedTex = binding0Tex ? binding0Tex : m_skyboxDummyTexture;
    if (m_lastPostSetFilledFrameNumber == frameNumber && m_cachedSkyboxTexture == expectedTex) return;
    EnsureDummyTexture();
    rhi::ITexture* dummy = m_skyboxDummyTexture;
    if (!dummy) return;
    rhi::ITexture* tex0 = binding0Tex ? binding0Tex : dummy;
    setRef->UpdateTexture(0, tex0);
    for (uint32_t b = 1; b < kPostPassBindingCount; ++b)
        setRef->UpdateTexture(b, dummy);
    m_lastPostSetFilledFrameNumber = frameNumber;
    m_cachedSkyboxTexture = tex0;
}

bool SkyboxPassSystem::RenderCubemap(const SkyboxDrawContext& ctx, const std::shared_ptr<TextureCube>& skybox) {
    if (!skybox || !skybox->IsValid()) {
        static bool logged = false;
        if (!logged) {
            core::Log::WarnF("RenderSkybox: skipped (no skybox or texture invalid)");
            logged = true;
        }
        return false;
    }
    EnsureCubemapShader();
    if (!m_skyboxShader || !m_skyboxShader->IsValid()) {
        static bool logged = false;
        if (!logged) {
            core::Log::WarnF("RenderSkybox: skipped (skybox shader not loaded)");
            logged = true;
        }
        return false;
    }
    rhi::ITexture* skyboxTex = skybox->GetTextureRHI();
    EnsurePostSetFilled(skyboxTex);
    const uint32_t frameIndex = m_device ? m_device->GetCurrentFrameIndex() % kPostSetsFrames : 0u;
    rhi::IDescriptorSet* skyboxPostSet = m_postSets[frameIndex];

    SkyboxPassBuildParams build{};
    build.shader = m_skyboxShader.get();
    build.device = ctx.device;
    build.cmd = ctx.cmd;
    build.renderTarget = ctx.renderTarget;
    build.hdrRenderTarget = ctx.hdrRenderTarget;
    build.gbufferRenderTarget = ctx.gbufferRenderTarget;
    build.frameSet = ctx.frameSet;
    build.postSet = skyboxPostSet;
    build.camera = ctx.camera;
    build.viewportWidth = ctx.viewportWidth;
    build.viewportHeight = ctx.viewportHeight;
    build.useGbufferDepth = ctx.useGbufferDepth;
    build.isVulkan = ctx.isVulkan;
    SkyboxPassExecuteParams params = BuildParams(build);
    return ExecuteCubemap(params);
}

bool SkyboxPassSystem::RenderProcedural(const SkyboxDrawContext& ctx) {
    EnsureProceduralShader();
    if (!m_proceduralSkyboxShader || !m_proceduralSkyboxShader->IsValid()) return false;
    EnsurePostSetFilled(nullptr);
    const uint32_t frameIndex = m_device ? m_device->GetCurrentFrameIndex() % kPostSetsFrames : 0u;
    rhi::IDescriptorSet* skyboxPostSet = m_postSets[frameIndex];

    SkyboxPassBuildParams build{};
    build.shader = m_proceduralSkyboxShader.get();
    build.device = ctx.device;
    build.cmd = ctx.cmd;
    build.renderTarget = ctx.renderTarget;
    build.hdrRenderTarget = ctx.hdrRenderTarget;
    build.gbufferRenderTarget = ctx.gbufferRenderTarget;
    build.frameSet = ctx.frameSet;
    build.postSet = skyboxPostSet;
    build.camera = ctx.camera;
    build.viewportWidth = ctx.viewportWidth;
    build.viewportHeight = ctx.viewportHeight;
    build.useGbufferDepth = ctx.useGbufferDepth;
    build.isVulkan = ctx.isVulkan;
    SkyboxPassExecuteParams params = BuildParams(build);
    return ExecuteProcedural(params);
}

SkyboxPassExecuteParams SkyboxPassSystem::BuildParams(const SkyboxPassBuildParams& params) const {
    SkyboxPassExecuteParams out{};
    out.build = params;
    out.inverseView = glm::inverse(params.camera.view);
    out.inverseProjection = glm::inverse(params.camera.projection);
    return out;
}

bool SkyboxPassSystem::ExecuteCubemap(const SkyboxPassExecuteParams& params) {
    Shader* const shader = params.build.shader;
    if (!shader || !shader->IsValid()) return false;
    rhi::ICommandList* cmd = params.build.cmd;
    rhi::IRenderTarget* rt = params.build.renderTarget;
    if (cmd && rt) {
        cmd->SetRenderTarget(rt);
        cmd->SetViewport(0, 0, params.build.viewportWidth, params.build.viewportHeight);
    }
    if (rt == params.build.hdrRenderTarget && params.build.useGbufferDepth && params.build.gbufferRenderTarget &&
        params.build.device && params.build.hdrRenderTarget)
        params.build.device->BlitDepthOnly(params.build.gbufferRenderTarget, params.build.hdrRenderTarget);
    if (cmd && rt)
        cmd->SetRenderTarget(rt);
    if (cmd) {
        cmd->SetCullFace(false);
        cmd->SetDepthTest(true);
        cmd->SetDepthWrite(false);
        cmd->SetDepthFuncLess(false);
    }
    shader->Bind(params.build.device);
    if (params.build.isVulkan && cmd) {
        abi::WriteSkyboxInversePushConstants(cmd, params.inverseView, params.inverseProjection);
    } else {
        shader->SetMat4("u_InverseViewMatrix", params.inverseView);
        shader->SetMat4("u_InverseProjectionMatrix", params.inverseProjection);
    }
    shader->SetInt("u_Skybox", 0);
    if (!params.build.frameSet || !params.build.postSet) {
        core::Log::InfoF("Skybox cubemap: skip draw (frameSet=%p postSet=%p)", (void*)params.build.frameSet, (void*)params.build.postSet);
        return false;
    }
    rhi::IPipeline* pipe = shader->GetPipelineRHI();
    if (!cmd || !pipe) {
        core::Log::InfoF("Skybox cubemap: skip draw (cmd=%p pipe=%p)", (void*)cmd, (void*)pipe);
        return false;
    }
    cmd->BindPipeline(pipe);
    rhi::IDescriptorSet* sets[2] = { params.build.frameSet, params.build.postSet };
    cmd->BindDescriptorSets(0, 2, sets);
    if (params.build.viewportWidth == 0 || params.build.viewportHeight == 0) {
        core::Log::WarnF("Skybox cubemap: zero viewport vpW=%u vpH=%u (draw skipped)", params.build.viewportWidth, params.build.viewportHeight);
        return false;
    }
    cmd->SetViewport(0, 0, params.build.viewportWidth, params.build.viewportHeight);
    cmd->DrawFullscreenQuad(pipe);
    shader->Unbind();
    if (cmd) {
        cmd->SetDepthTest(true);
        cmd->SetDepthWrite(false);
        cmd->SetDepthFuncLess(true);
        cmd->SetCullFace(true);
        cmd->SetRenderTarget(nullptr);
    }
    return true;
}

bool SkyboxPassSystem::ExecuteProcedural(const SkyboxPassExecuteParams& params) {
    Shader* const shader = params.build.shader;
    if (!shader || !shader->IsValid()) return false;
    rhi::ICommandList* cmd = params.build.cmd;
    rhi::IRenderTarget* rt = params.build.renderTarget;
    if (cmd && rt) {
        cmd->SetRenderTarget(rt);
        cmd->SetViewport(0, 0, params.build.viewportWidth, params.build.viewportHeight);
    }
    if (rt == params.build.hdrRenderTarget && params.build.useGbufferDepth && params.build.gbufferRenderTarget &&
        params.build.device && params.build.hdrRenderTarget)
        params.build.device->BlitDepthOnly(params.build.gbufferRenderTarget, params.build.hdrRenderTarget);
    if (cmd && rt)
        cmd->SetRenderTarget(rt);
    if (cmd)
        cmd->SetCullFace(false);
    if (cmd) {
        cmd->SetDepthTest(true);
        cmd->SetDepthWrite(true);
        cmd->SetDepthFuncLess(false);
    }

    shader->Bind(params.build.device);
    if (params.build.isVulkan && cmd) {
        abi::WriteSkyboxInversePushConstants(cmd, params.inverseView, params.inverseProjection);
    } else {
        shader->SetMat4("u_InverseViewMatrix", params.inverseView);
        shader->SetMat4("u_InverseProjectionMatrix", params.inverseProjection);
    }
    if (!params.build.frameSet || !params.build.postSet) {
        core::Log::InfoF("Skybox procedural: skip draw (frameSet=%p postSet=%p)", (void*)params.build.frameSet, (void*)params.build.postSet);
        return false;
    }
    rhi::IPipeline* pipe = shader->GetPipelineRHI();
    if (!cmd || !pipe) {
        core::Log::InfoF("Skybox procedural: skip draw (cmd=%p pipe=%p)", (void*)cmd, (void*)pipe);
        return false;
    }
    cmd->BindPipeline(pipe);
    rhi::IDescriptorSet* sets[2] = { params.build.frameSet, params.build.postSet };
    cmd->BindDescriptorSets(0, 2, sets);
    if (params.build.viewportWidth == 0 || params.build.viewportHeight == 0) {
        core::Log::WarnF("Skybox procedural: zero viewport vpW=%u vpH=%u (draw skipped)", params.build.viewportWidth, params.build.viewportHeight);
        return false;
    }
    cmd->SetViewport(0, 0, params.build.viewportWidth, params.build.viewportHeight);
    cmd->DrawFullscreenQuad(pipe);

    shader->Unbind();
    if (cmd) {
        cmd->SetDepthTest(true);
        cmd->SetDepthWrite(true);
        cmd->SetDepthFuncLess(true);
        cmd->SetCullFace(true);
        cmd->SetRenderTarget(nullptr);
    }
    return true;
}

}  // namespace render
}  // namespace eyelib
