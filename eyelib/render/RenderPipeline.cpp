#include "eyelib/render/RenderPipeline.h"
#include "eyelib/render/GBuffer.h"
#include "eyelib/render/ShadowMap.h"
#include "eyelib/render/ClusteredLightGrid.h"
#include "eyelib/render/PostProcessor.h"
#include "eyelib/render/passes/GBufferPass.h"
#include "eyelib/render/passes/ShadowPass.h"
#include "eyelib/render/passes/LightingPass.h"
#include "eyelib/render/passes/TransparencyPass.h"
#include "eyelib/render/DebugDrawRenderer.h"
#include "eyelib/debug/Presets.h"
#include "eyelib/render/framegraph/FrameGraph.h"
#include "eyelib/render/resources/Material.h"
#include "eyelib/render/resources/MaterialInstance.h"
#include "eyelib/render/resources/Mesh.h"
#include "eyelib/render/resources/Shader.h"
#include "eyelib/render/resources/Texture2D.h"
#include "eyelib/render/resources/TextureCube.h"
#include "eyelib/render/FullscreenQuad.h"
#include "eyelib/core/Log.h"
#include "eyelib/core/Time.h"
#include "eyelib/render/rhi/RHI.h"
#include "eyelib/render/rhi/RHIContext.h"
#include "eyelib/render/rhi/RHIUtils.h"
#include "eyelib/math/TransformUtils.h"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <glm/gtc/matrix_inverse.hpp>
#include <span>
#include <string>

namespace eyelib {
namespace render {

namespace {
constexpr size_t kMaxBones = 128;
}

RenderPipeline::RenderPipeline() = default;

RenderPipeline::~RenderPipeline() {
    Shutdown();
}

void RenderPipeline::Shutdown() {
    DestroyHDRFramebuffer();
    // Destroy all objects that hold a raw pointer to m_device before destroying the device.
    // Their destructors call back into the device (DestroyRenderTarget, DestroyPipeline, etc.).
    m_postProcessor.reset();
    m_forwardPlusShader.reset();
    m_forwardPlusSkinnedShader.reset();
    m_clusteredLightGrid.reset();
    m_frameGraph.reset();
    m_lightingPass.reset();
    m_shadowPass.reset();
    m_gbufferPass.reset();
    m_shadowMap.reset();
    m_gbuffer.reset();
    if (m_device) {
        if (m_pointLightBuffer) { m_device->DestroyBuffer(m_pointLightBuffer); m_pointLightBuffer = nullptr; }
        if (m_spotLightBuffer) { m_device->DestroyBuffer(m_spotLightBuffer); m_spotLightBuffer = nullptr; }
        if (m_lightIndexBuffer) { m_device->DestroyBuffer(m_lightIndexBuffer); m_lightIndexBuffer = nullptr; }
    }
    rhi::SetDefaultDevice(nullptr);
    m_device.reset();
    m_width = m_height = 0;
}

bool RenderPipeline::Initialize(std::unique_ptr<rhi::IDevice> device, uint32_t width, uint32_t height) {
    if (!device) {
        core::Log::Error("RenderPipeline::Initialize: device is null");
        return false;
    }
    m_width = width;
    m_height = height;
    m_device = std::move(device);
    rhi::SetDefaultDevice(m_device.get());

    core::Log::Info("RenderPipeline::Initialize: initializing GBuffer");
    m_gbuffer = std::make_unique<GBuffer>();
    if (!m_gbuffer->Initialize(width, height)) {
        core::Log::Error("RenderPipeline::Initialize: GBuffer::Initialize failed");
        return false;
    }

    core::Log::Info("RenderPipeline::Initialize: initializing ShadowMap");
    m_shadowMap = std::make_unique<ShadowMap>();
    if (!m_shadowMap->Initialize(m_device.get(), 2048)) {
        core::Log::Error("RenderPipeline::Initialize: ShadowMap::Initialize failed");
        return false;
    }

    m_gbufferPass = std::make_unique<GBufferPass>();
    m_shadowPass = std::make_unique<ShadowPass>();
    m_lightingPass = std::make_unique<LightingPass>();
    m_frameGraph = std::make_unique<FrameGraph>();
    m_frameGraph->SetDevice(m_device.get());
    m_frameGraph->SetFrameSize(width, height);
    m_clusteredLightGrid = std::make_unique<ClusteredLightGrid>();
    m_clusteredLightGrid->Initialize(width, height, 0.1f, 1000.0f);  // Default, will be updated per-frame

    core::Log::Info("RenderPipeline::Initialize: initializing PostProcessor");
    m_postProcessor = std::make_unique<PostProcessor>();
    if (!m_postProcessor->Initialize(width, height)) {
        core::Log::Error("RenderPipeline::Initialize: PostProcessor::Initialize failed");
        return false;
    }

    core::Log::Info("RenderPipeline::Initialize: creating HDR framebuffer");
    CreateHDRFramebuffer();
    if (m_hdrRenderTarget == nullptr) {
        core::Log::Error("RenderPipeline::Initialize: CreateHDRFramebuffer failed (m_hdrRenderTarget is null)");
        return false;
    }

    // Initialize previous matrices (identity for first frame)
    m_prevView = math::Mat4(1.0f);
    m_prevProjection = math::Mat4(1.0f);

    core::Log::Info("RenderPipeline::Initialize: pipeline init success");
    return true;
}

void RenderPipeline::Resize(uint32_t width, uint32_t height) {
#ifndef NDEBUG
    assert(!m_rendering && "Resize may only be called at frame boundary, before Render().");
#endif
    if (width == 0 || height == 0) return;
    if (width == m_width && height == m_height) return;
    m_width = width;
    m_height = height;
    if (m_gbuffer) m_gbuffer->Resize(width, height);
    DestroyHDRFramebuffer();
    CreateHDRFramebuffer();
    if (m_frameGraph) m_frameGraph->SetFrameSize(width, height);
    if (m_clusteredLightGrid) {
        m_clusteredLightGrid->Initialize(width, height, 0.1f, 1000.0f);  // Will be updated per-frame with actual camera
    }
    if (m_postProcessor) {
        m_postProcessor->Resize(width, height);
    }
    rhi::ICommandList* cmd = m_device ? m_device->GetImmediateCommandList() : nullptr;
    if (cmd) cmd->SetViewport(0, 0, width, height);
}

void RenderPipeline::SetFrameSize(unsigned w, unsigned h) {
    Resize(static_cast<uint32_t>(w), static_cast<uint32_t>(h));
}

void RenderPipeline::EnsureResources(uint32_t width, uint32_t height) {
    if (!m_gbuffer || !m_device) return;
    Resize(width, height);
}

void RenderPipeline::CreateHDRFramebuffer() {
    if (m_width == 0 || m_height == 0 || !m_device) return;
    DestroyHDRFramebuffer();
    std::vector<rhi::RHITextureFormat> colorFormats = { rhi::RHITextureFormat::RGBA16F };
    m_hdrRenderTarget = m_device->CreateRenderTarget(
        static_cast<uint32_t>(m_width), static_cast<uint32_t>(m_height),
        colorFormats, rhi::RHITextureFormat::Depth24, 1);
}

void RenderPipeline::DestroyHDRFramebuffer() {
    // Caller must ensure this is only called at frame boundary (e.g. from Resize), not while a frame is in flight.
    if (m_device && m_hdrRenderTarget) {
        m_device->DestroyRenderTarget(m_hdrRenderTarget);
        m_hdrRenderTarget = nullptr;
    }
}

rhi::ITexture* RenderPipeline::GetHDRColorTextureRHI() const {
    return m_hdrRenderTarget ? m_hdrRenderTarget->GetColorTexture(0) : nullptr;
}

void RenderPipeline::BuildAndExecuteFrameGraph(const IRenderSceneView& view) {
    if (!m_frameGraph || !m_gbuffer || !m_shadowMap || !m_gbufferPass || !m_shadowPass || !m_lightingPass) return;
    m_frameGraph->Clear();
    m_frameGraph->SetFrameSize(m_width, m_height);

    const CameraView cam = view.GetCamera();
    const LightParams light = view.GetLight();

    // Fill scratch lights for lighting pass (view may be API or engine)
    m_scratchPointLights.clear();
    m_scratchSpotLights.clear();
    view.ForEachPointLight([this](const PointLight& p) { m_scratchPointLights.push_back(p); });
    view.ForEachSpotLight([this](const SpotLight& s) { m_scratchSpotLights.push_back(s); });

    rhi::ITexture* hdrColorTex = m_hdrRenderTarget ? m_hdrRenderTarget->GetColorTexture(0) : nullptr;
    rhi::ITexture* hdrDepthTex = m_hdrRenderTarget ? m_hdrRenderTarget->GetDepthTexture() : nullptr;
    auto hdrColor = m_frameGraph->ImportTextureRHI("HDR", hdrColorTex, m_width, m_height);
    auto hdrDepth = m_frameGraph->ImportTextureRHI("HDRDepth", hdrDepthTex, m_width, m_height);
    auto gbPos = m_frameGraph->ImportTexture("GBufferPos", m_gbuffer->GetPositionTexture(), m_width, m_height);
    auto gbNorm = m_frameGraph->ImportTexture("GBufferNorm", m_gbuffer->GetNormalTexture(), m_width, m_height);
    auto gbAlb = m_frameGraph->ImportTexture("GBufferAlb", m_gbuffer->GetAlbedoTexture(), m_width, m_height);
    auto gbPBR = m_frameGraph->ImportTexture("GBufferPBR", m_gbuffer->GetPBRParamsTexture(), m_width, m_height);
    auto gbVelocity = m_frameGraph->ImportTexture("GBufferVelocity", m_gbuffer->GetVelocityTexture(), m_width, m_height);
    auto gbDepth = m_frameGraph->ImportTexture("GBufferDepth", m_gbuffer->GetDepthTexture(), m_width, m_height);
    auto shadowTex = m_frameGraph->ImportTextureRHI("Shadow", m_shadowMap->GetDepthTextureRHI(), m_shadowMap->GetResolution(), m_shadowMap->GetResolution());
    rhi::ICommandList* cmd = GetRHICommandList();

    if (light.castShadows) {
        m_frameGraph->AddPass("Shadow",
            [this, &view, &light, &cam, cmd](const FGPassResources&) {
                m_shadowPass->Render(*m_shadowMap, cmd,
                    [&view](std::function<void(const Drawable&)> c) { view.ForEachOpaqueDrawable(c); },
                    light.direction, cam.view, cam.projection, cam.viewPos, cam.nearPlane, cam.farPlane);
            },
            {}, { shadowTex }, true);
    }

    m_frameGraph->AddPass("GBuffer",
        [this, &view, &cam, cmd](const FGPassResources&) {
            m_gbufferPass->Render(*m_gbuffer, cmd,
                [&view](std::function<void(const Drawable&)> c) { view.ForEachOpaqueDrawable(c); },
                cam.view, cam.projection, m_prevView, m_prevProjection);
        },
        {}, { gbPos, gbNorm, gbAlb, gbPBR, gbVelocity, gbDepth }, true);

    std::vector<FGTextureHandle> lightingReads = { gbPos, gbNorm, gbAlb, gbPBR };
    if (light.castShadows) lightingReads.push_back(shadowTex);
    rhi::ClearValue clearBlack = { 0.f, 0.f, 0.f, 1.f };
    m_frameGraph->AddPass("Lighting",
        [this, &view, &cam, cmd, clearBlack](const FGPassResources&) {
            if (cmd && m_hdrRenderTarget)
                rhi::ClearAndSetRenderTargetWithViewport(cmd, m_hdrRenderTarget, 0, 0, m_width, m_height, clearBlack);
            ShadowMap* shadowMapForLighting = view.GetLight().castShadows ? m_shadowMap.get() : nullptr;
            m_lightingPass->Render(*m_gbuffer, cmd, view.GetLight(), cam.view, cam.projection, cam.viewPos, shadowMapForLighting,
                &m_scratchPointLights, &m_scratchSpotLights);
        },
        lightingReads, { hdrColor }, true);

    m_frameGraph->AddPass("Transparency",
        [this, &view, &cam, cmd](const FGPassResources&) {
            if (m_device && m_hdrRenderTarget)
                m_device->BlitDepthOnly(m_gbuffer->GetRenderTargetRHI(), m_hdrRenderTarget);
            if (cmd && m_hdrRenderTarget) {
                cmd->SetRenderTarget(m_hdrRenderTarget);
                cmd->SetViewport(0, 0, m_width, m_height);
            }
            m_scratchTransparent.clear();
            view.ForEachTransparentDrawable([this](const Drawable& d) { m_scratchTransparent.push_back(d); });
            TransparencyPass::Render(cmd, m_scratchTransparent, cam.view, cam.projection, cam.viewPos);
        },
        {}, { hdrColor }, true);

    m_frameGraph->Compile();
    m_frameGraph->Execute(GetRHICommandList());
}

void RenderPipeline::RenderForward(const IRenderSceneView& view) {
    m_frameGraph->Clear();
    m_frameGraph->SetFrameSize(m_width, m_height);
    rhi::ICommandList* cmd = GetRHICommandList();
    if (cmd && m_hdrRenderTarget) {
        rhi::ClearValue cv = { 0.1f, 0.1f, 0.15f, 1.f };
        rhi::ClearAndSetRenderTargetWithViewport(cmd, m_hdrRenderTarget, 0, 0, m_width, m_height, cv);
    }
    if (cmd) { cmd->SetDepthTest(true); cmd->SetCullFace(false); }

    core::Log::Info("RenderForward: executing");

    const int kShadowMapUnit = 5;
    if (m_shadowMap && cmd) {
        rhi::ITexture* shadowTex = m_shadowMap->GetDepthTextureRHI();
        if (shadowTex) cmd->BindTexture(shadowTex, kShadowMapUnit);
    }

    m_scratchPointLights.clear();
    m_scratchSpotLights.clear();
    view.ForEachPointLight([this](const PointLight& p) { m_scratchPointLights.push_back(p); });
    view.ForEachSpotLight([this](const SpotLight& s) { m_scratchSpotLights.push_back(s); });
    const int kMaxPointLights = 8;
    const int kMaxSpotLights = 4;
    const int numPoint = static_cast<int>(std::min(m_scratchPointLights.size(), size_t(kMaxPointLights)));
    const int numSpot = static_cast<int>(std::min(m_scratchSpotLights.size(), size_t(kMaxSpotLights)));

    const CameraView cam = view.GetCamera();
    const LightParams light = view.GetLight();

    size_t forwardDrawCount = 0;
    size_t forwardSkipNoShader = 0;
    view.ForEachOpaqueDrawable([&](const Drawable& d) {
        if (!d.mesh || !d.material) return;
        if (d.materialInstance) {
            d.materialInstance->Bind();
        } else {
            d.material->Bind();
        }
        Shader* sh = (d.materialInstance ? d.materialInstance->GetMaterial() : d.material)->GetShader().get();
        if (!sh) {
            forwardSkipNoShader++;
            return;
        }
        {
            sh->SetMat4("u_Model", d.transform);
            sh->SetMat4("u_NormalMatrix", math::Mat4FromMat3(math::NormalMatrix(d.transform)));
            sh->SetMat4("u_View", cam.view);
            sh->SetMat4("u_Projection", cam.projection);
            sh->SetVec3("u_ViewPos", cam.viewPos);
            sh->SetVec3("u_LightDir", light.direction);
            sh->SetVec3("u_LightColor", light.color);
            sh->SetFloat("u_LightIntensity", light.intensity);
            sh->SetVec3("u_Ambient", light.ambient);
            sh->SetFloat("u_Exposure", light.exposure);
            if (m_shadowMap) {
                sh->SetInt("u_ShadowMap", kShadowMapUnit);
                sh->SetMat4("u_LightSpaceMatrix", m_shadowMap->GetShadowMatrix());
                sh->SetFloat("u_ShadowBias", 0.005f);
                sh->SetInt("u_UseShadows", 1);
            } else {
                sh->SetInt("u_UseShadows", 0);
            }
            const math::Mat4* bonePtr = d.bones;
            size_t boneCount = d.boneCount;
            if ((!bonePtr || boneCount == 0) && d.materialInstance) {
                d.materialInstance->ForEachMat4Array([&](const std::string& name, const math::Mat4* values, size_t count) {
                    if (name == "u_Bones" && values && count > 0) {
                        bonePtr = values;
                        boneCount = count;
                    }
                });
            } else if (!bonePtr || boneCount == 0) {
                d.material->ForEachMat4Array([&](const std::string& name, const math::Mat4* values, size_t count) {
                    if (name == "u_Bones" && values && count > 0) {
                        bonePtr = values;
                        boneCount = count;
                    }
                });
            }
            if (bonePtr && boneCount > 0) {
                boneCount = std::min(boneCount, kMaxBones);
                sh->SetMat4Array("u_Bones", bonePtr, boneCount);
            }
            sh->SetInt("u_PointLightCount", numPoint);
            for (int i = 0; i < numPoint; ++i) {
                const auto& p = m_scratchPointLights[static_cast<size_t>(i)];
                sh->SetVec3("u_PointLightPos[" + std::to_string(i) + "]", p.position);
                sh->SetVec3("u_PointLightColor[" + std::to_string(i) + "]", p.color);
                sh->SetFloat("u_PointLightIntensity[" + std::to_string(i) + "]", p.intensity);
                sh->SetFloat("u_PointLightRadius[" + std::to_string(i) + "]", p.radius);
            }
            sh->SetInt("u_SpotLightCount", numSpot);
            for (int i = 0; i < numSpot; ++i) {
                const auto& s = m_scratchSpotLights[static_cast<size_t>(i)];
                sh->SetVec3("u_SpotLightPos[" + std::to_string(i) + "]", s.position);
                sh->SetVec3("u_SpotLightDir[" + std::to_string(i) + "]", s.direction);
                sh->SetVec3("u_SpotLightColor[" + std::to_string(i) + "]", s.color);
                sh->SetFloat("u_SpotLightIntensity[" + std::to_string(i) + "]", s.intensity);
                sh->SetFloat("u_SpotLightRange[" + std::to_string(i) + "]", s.range);
                sh->SetFloat("u_SpotLightInnerCos[" + std::to_string(i) + "]", s.innerConeCos);
                sh->SetFloat("u_SpotLightOuterCos[" + std::to_string(i) + "]", s.outerConeCos);
            }
        d.mesh->Bind();
        d.mesh->Draw();
        d.mesh->Unbind();
        sh->Unbind();
        forwardDrawCount++;
        }
    });

    core::Log::InfoF("RenderForward: drew %zu opaque, skipped %zu (no shader)", forwardDrawCount, forwardSkipNoShader);

    if (m_shadowMap && cmd) cmd->UnbindTexture(kShadowMapUnit);

    m_scratchTransparent.clear();
    view.ForEachTransparentDrawable([this](const Drawable& d) { m_scratchTransparent.push_back(d); });
    TransparencyPass::Render(cmd, m_scratchTransparent, cam.view, cam.projection, cam.viewPos);
    if (cmd) { cmd->SetCullFace(true); cmd->SetRenderTarget(nullptr); }
}

void RenderPipeline::RenderForwardPlus(const IRenderSceneView& view) {
    if (!m_clusteredLightGrid) {
        RenderForward(view);
        return;
    }

    const CameraView cam = view.GetCamera();
    const LightParams light = view.GetLight();

    m_scratchPointLights.clear();
    m_scratchSpotLights.clear();
    view.ForEachPointLight([this](const PointLight& p) { m_scratchPointLights.push_back(p); });
    view.ForEachSpotLight([this](const SpotLight& s) { m_scratchSpotLights.push_back(s); });

    // Reinitialize clustered grid if camera planes changed
    if (m_clusteredLightGrid->GetNearPlane() != cam.nearPlane ||
        m_clusteredLightGrid->GetFarPlane() != cam.farPlane) {
        m_clusteredLightGrid->Initialize(m_width, m_height, cam.nearPlane, cam.farPlane);
    }

    // Build clustered light grid
    m_clusteredLightGrid->BuildLightGrid(m_scratchPointLights, m_scratchSpotLights,
                                         cam.view, cam.projection, cam.viewPos);

    // Upload lights to SSBOs (RHI)
    rhi::ICommandList* cmdFwd = GetRHICommandList();
    const size_t kMaxPointLights = 256;
    const size_t kMaxSpotLights = 64;
    const size_t kMaxLightIndices = 4 * 1024 * 1024;
    if (!m_scratchPointLights.empty() && m_device) {
        if (!m_pointLightBuffer) {
            rhi::BufferDesc bd;
            bd.sizeInBytes = kMaxPointLights * sizeof(PointLight);
            bd.usage = rhi::BufferUsage::Storage;
            bd.memoryHint = rhi::BufferMemoryHint::HostVisible;
            m_pointLightBuffer = m_device->CreateBuffer(bd, nullptr);
        }
        if (m_pointLightBuffer) {
            const size_t sz = m_scratchPointLights.size() * sizeof(PointLight);
            m_pointLightBuffer->Update(m_scratchPointLights.data(), 0, sz);
            if (cmdFwd) cmdFwd->BindStorageBuffer(m_pointLightBuffer, 3, 0, static_cast<uint32_t>(sz));
        }
    }
    if (!m_scratchSpotLights.empty() && m_device) {
        if (!m_spotLightBuffer) {
            rhi::BufferDesc bd;
            bd.sizeInBytes = kMaxSpotLights * sizeof(SpotLight);
            bd.usage = rhi::BufferUsage::Storage;
            bd.memoryHint = rhi::BufferMemoryHint::HostVisible;
            m_spotLightBuffer = m_device->CreateBuffer(bd, nullptr);
        }
        if (m_spotLightBuffer) {
            const size_t sz = m_scratchSpotLights.size() * sizeof(SpotLight);
            m_spotLightBuffer->Update(m_scratchSpotLights.data(), 0, sz);
            if (cmdFwd) cmdFwd->BindStorageBuffer(m_spotLightBuffer, 4, 0, static_cast<uint32_t>(sz));
        }
    }
    if (m_device && !m_lightIndexBuffer) {
        rhi::BufferDesc bd;
        bd.sizeInBytes = kMaxLightIndices;
        bd.usage = rhi::BufferUsage::Storage;
        bd.memoryHint = rhi::BufferMemoryHint::HostVisible;
        m_lightIndexBuffer = m_device->CreateBuffer(bd, nullptr);
    }
    m_clusteredLightGrid->UploadToGPU(m_lightIndexBuffer);
    if (cmdFwd && m_lightIndexBuffer) cmdFwd->BindStorageBuffer(m_lightIndexBuffer, 2);

    if (light.castShadows && m_shadowMap && m_shadowPass) {
        m_shadowPass->Render(*m_shadowMap, GetRHICommandList(),
            [&view](std::function<void(const Drawable&)> c) { view.ForEachOpaqueDrawable(c); },
            light.direction, cam.view, cam.projection, cam.viewPos, cam.nearPlane, cam.farPlane);
    }

    // Ensure forward+ shader exists (backend-owned loading by logical name)
    if (!m_forwardPlusShader || !m_forwardPlusShader->IsValid()) {
        m_forwardPlusShader = std::make_shared<Shader>();
        if (m_device)
            m_forwardPlusShader->Load(m_device.get(), "passes/forward/forwardplus", Mesh::GetVertexLayoutRHI());
    }

    if (!m_forwardPlusShader->IsValid()) {
        RenderForward(view);
        return;
    }

    if (!m_forwardPlusSkinnedShader || !m_forwardPlusSkinnedShader->IsValid()) {
        m_forwardPlusSkinnedShader = std::make_shared<Shader>();
        if (m_device)
            m_forwardPlusSkinnedShader->Load(m_device.get(), "passes/forward/forwardplus_skinned", Mesh::GetVertexLayoutRHI());
    }

    if (cmdFwd && m_hdrRenderTarget) {
        rhi::ClearValue cv = { 0.1f, 0.1f, 0.15f, 1.f };
        rhi::ClearAndSetRenderTargetWithViewport(cmdFwd, m_hdrRenderTarget, 0, 0, m_width, m_height, cv);
    }
    if (cmdFwd) { cmdFwd->SetDepthTest(true); cmdFwd->SetCullFace(false); }

    const int kShadowMapUnit = 5;
    if (light.castShadows && m_shadowMap && cmdFwd) {
        rhi::ITexture* shadowTex = m_shadowMap->GetDepthTextureRHI();
        if (shadowTex) cmdFwd->BindTexture(shadowTex, kShadowMapUnit);
    }

    auto setCommonForwardPlusUniforms = [&](Shader* sh) {
        sh->SetMat4("u_View", cam.view);
        sh->SetMat4("u_Projection", cam.projection);
        sh->SetVec3("u_ViewPos", cam.viewPos);
        sh->SetVec3("u_DirLightDir", light.direction);
        sh->SetVec3("u_DirLightColor", light.color);
        sh->SetFloat("u_DirLightIntensity", light.intensity);
        sh->SetVec3("u_Ambient", light.ambient);
        sh->SetFloat("u_Exposure", light.exposure);
        sh->SetInt("u_ScreenWidth", static_cast<int>(m_width));
        sh->SetInt("u_ScreenHeight", static_cast<int>(m_height));
        sh->SetInt("u_TileCountX", static_cast<int>(m_clusteredLightGrid->GetTileCountX()));
        sh->SetInt("u_TileCountY", static_cast<int>(m_clusteredLightGrid->GetTileCountY()));
        sh->SetInt("u_SliceCount", static_cast<int>(m_clusteredLightGrid->GetSliceCount()));
        sh->SetFloat("u_LogNear", m_clusteredLightGrid->GetLogNear());
        sh->SetFloat("u_LogFar", m_clusteredLightGrid->GetLogFar());
        sh->SetFloat("u_NearPlane", m_clusteredLightGrid->GetNearPlane());
        sh->SetFloat("u_FarPlane", m_clusteredLightGrid->GetFarPlane());
        sh->SetInt("u_PointLightCount", static_cast<int>(m_scratchPointLights.size()));
        sh->SetInt("u_SpotLightCount", static_cast<int>(m_scratchSpotLights.size()));
        if (light.castShadows && m_shadowMap) {
            sh->SetInt("u_ShadowMap", kShadowMapUnit);
            sh->SetMat4("u_LightSpaceMatrix", m_shadowMap->GetShadowMatrix());
            sh->SetFloat("u_ShadowBias", 0.005f);
            sh->SetInt("u_UseShadows", 1);
        } else {
            sh->SetInt("u_UseShadows", 0);
        }
    };

    const int kAlbedoTextureUnit = 0;
    const int kNormalTextureUnit = 1;
    const int kMetallicRoughnessTextureUnit = 2;
    const int kAOTextureUnit = 3;
    const int kEmissiveTextureUnit = 4;
    const math::Vec4 kDefaultUVTransform(0.0f, 0.0f, 1.0f, 1.0f);
    bool commonSetNonSkinned = false;
    bool commonSetSkinned = false;

    view.ForEachOpaqueDrawable([&](const Drawable& d) {
        if (!d.mesh) return;
        const auto material = d.materialInstance ? d.materialInstance->GetMaterial() : d.material;
        if (!material) return;

        const math::Mat4* bonePtr = d.bones;
        size_t boneCount = d.boneCount;
        auto collectBones = [&](const std::string& name, const math::Mat4* values, size_t count) {
            if (name == "u_Bones" && values && count > 0) { bonePtr = values; boneCount = count; }
        };
        if ((!bonePtr || boneCount == 0) && d.materialInstance) d.materialInstance->ForEachMat4Array(collectBones);
        else if (!bonePtr || boneCount == 0) material->ForEachMat4Array(collectBones);

        const bool useSkinned = (bonePtr && boneCount > 0 && m_forwardPlusSkinnedShader && m_forwardPlusSkinnedShader->IsValid());
        Shader* sh = useSkinned ? m_forwardPlusSkinnedShader.get() : m_forwardPlusShader.get();
        sh->Bind();

        if (useSkinned) {
            if (!commonSetSkinned) { setCommonForwardPlusUniforms(sh); commonSetSkinned = true; }
        } else {
            if (!commonSetNonSkinned) { setCommonForwardPlusUniforms(sh); commonSetNonSkinned = true; }
        }

        sh->SetMat4("u_Model", d.transform);
        sh->SetMat4("u_NormalMatrix", math::Mat4FromMat3(math::NormalMatrix(d.transform)));
        if (useSkinned) {
            boneCount = std::min(boneCount, kMaxBones);
            sh->SetMat4Array("u_Bones", bonePtr, boneCount);
        }

        const math::Vec4 uvTransform = d.materialInstance ? d.materialInstance->GetVec4("u_UVTransform", kDefaultUVTransform) : material->GetVec4("u_UVTransform", kDefaultUVTransform);
        const float uvRotation = d.materialInstance ? d.materialInstance->GetFloat("u_UVRotation", 0.0f) : material->GetFloat("u_UVRotation", 0.0f);
        const float texCoordIndex = d.materialInstance ? d.materialInstance->GetFloat("u_TexCoordIndex", 0.0f) : material->GetFloat("u_TexCoordIndex", 0.0f);
        const float flipV = d.materialInstance ? d.materialInstance->GetFloat("u_FlipV", 0.0f) : material->GetFloat("u_FlipV", 0.0f);
        sh->SetVec4("u_UVTransform", uvTransform);
        sh->SetFloat("u_UVRotation", uvRotation);
        sh->SetFloat("u_TexCoordIndex", texCoordIndex);
        sh->SetFloat("u_FlipV", flipV);

        const math::Vec3 albedo = d.materialInstance ? d.materialInstance->GetAlbedo() : material->GetAlbedo();
        const float metallic = d.materialInstance ? d.materialInstance->GetMetallic() : material->GetMetallic();
        const float roughness = d.materialInstance ? d.materialInstance->GetRoughness() : material->GetRoughness();
        const float ao = d.materialInstance ? d.materialInstance->GetAO() : material->GetAO();
        const math::Vec3 emission = d.materialInstance ? d.materialInstance->GetEmission() : material->GetEmission();
        const float alpha = d.materialInstance ? d.materialInstance->GetAlpha() : material->GetAlpha();
        sh->SetVec3("u_Albedo", albedo);
        sh->SetFloat("u_Metallic", metallic);
        sh->SetFloat("u_Roughness", roughness);
        sh->SetFloat("u_AO", ao);
        sh->SetVec3("u_Emission", emission);
        sh->SetFloat("u_Alpha", alpha);

        auto albedoTex = d.materialInstance ? d.materialInstance->GetTexture("u_AlbedoTexture") : material->GetTexture("u_AlbedoTexture");
        if (albedoTex && albedoTex->IsValid()) {
            albedoTex->Bind(kAlbedoTextureUnit);
            sh->SetInt("u_AlbedoTexture", kAlbedoTextureUnit);
            sh->SetInt("u_UseAlbedoTexture", 1);
        } else {
            sh->SetInt("u_UseAlbedoTexture", 0);
        }
        auto normalTex = d.materialInstance ? d.materialInstance->GetTexture("u_NormalTexture") : material->GetTexture("u_NormalTexture");
        if (normalTex && normalTex->IsValid()) {
            normalTex->Bind(kNormalTextureUnit);
            sh->SetInt("u_NormalTexture", kNormalTextureUnit);
            sh->SetInt("u_UseNormalTexture", 1);
        } else {
            sh->SetInt("u_UseNormalTexture", 0);
        }
        auto mrTex = d.materialInstance ? d.materialInstance->GetTexture("u_MetallicRoughnessTexture") : material->GetTexture("u_MetallicRoughnessTexture");
        if (mrTex && mrTex->IsValid()) {
            mrTex->Bind(kMetallicRoughnessTextureUnit);
            sh->SetInt("u_MetallicRoughnessTexture", kMetallicRoughnessTextureUnit);
            sh->SetInt("u_UseMetallicRoughnessTexture", 1);
        } else {
            sh->SetInt("u_UseMetallicRoughnessTexture", 0);
        }
        auto aoTex = d.materialInstance ? d.materialInstance->GetTexture("u_AOTexture") : material->GetTexture("u_AOTexture");
        if (aoTex && aoTex->IsValid()) {
            aoTex->Bind(kAOTextureUnit);
            sh->SetInt("u_AOTexture", kAOTextureUnit);
            sh->SetInt("u_UseAOTexture", 1);
        } else {
            sh->SetInt("u_UseAOTexture", 0);
        }
        auto emissiveTex = d.materialInstance ? d.materialInstance->GetTexture("u_EmissiveTexture") : material->GetTexture("u_EmissiveTexture");
        if (emissiveTex && emissiveTex->IsValid()) {
            emissiveTex->Bind(kEmissiveTextureUnit);
            sh->SetInt("u_EmissiveTexture", kEmissiveTextureUnit);
            sh->SetInt("u_UseEmissiveTexture", 1);
        } else {
            sh->SetInt("u_UseEmissiveTexture", 0);
        }

        d.mesh->Bind();
        d.mesh->Draw();
        d.mesh->Unbind();
    });

    m_forwardPlusShader->Unbind();
    if (m_forwardPlusSkinnedShader) m_forwardPlusSkinnedShader->Unbind();
    if (light.castShadows && m_shadowMap && cmdFwd) cmdFwd->UnbindTexture(kShadowMapUnit);
    m_scratchTransparent.clear();
    view.ForEachTransparentDrawable([this](const Drawable& d) { m_scratchTransparent.push_back(d); });
    TransparencyPass::Render(cmdFwd, m_scratchTransparent, cam.view, cam.projection, cam.viewPos);
    if (cmdFwd) { cmdFwd->SetCullFace(true); cmdFwd->SetRenderTarget(nullptr); }
}

void RenderPipeline::Render(const RenderScene& scene) {
    RenderSceneView view(scene);
    Render(view);
}

void RenderPipeline::Render(const IRenderSceneView& view) {
    if (m_width == 0 || m_height == 0) return;
    EnsureResources(m_width, m_height);
    if (!m_hdrRenderTarget || !m_gbuffer) return;

    rhi::ICommandList* cmd = GetRHICommandList();
    if (!cmd) return;  // No command list available
#ifndef NDEBUG
    m_rendering = true;
#endif
    cmd->Begin();

    const CameraView cam = view.GetCamera();
    static bool s_loggedScene = false;
    if (!s_loggedScene) {
        size_t opaqueCount = 0, transparentCount = 0;
        view.ForEachOpaqueDrawable([&](const Drawable&) { ++opaqueCount; });
        view.ForEachTransparentDrawable([&](const Drawable&) { ++transparentCount; });
        core::Log::InfoF("RenderPipeline::Render: opaque=%zu transparent=%zu camera valid=%d",
                         opaqueCount, transparentCount, (cam.viewPos.x != 0 || cam.viewPos.y != 0 || cam.viewPos.z != 0));
        s_loggedScene = true;
    }

    switch (m_mode) {
    case PipelineMode::Deferred:
        BuildAndExecuteFrameGraph(view);
        break;
    case PipelineMode::Forward:
        if (m_shadowMap && m_shadowPass) {
            m_shadowPass->Render(*m_shadowMap, GetRHICommandList(),
                [&view](std::function<void(const Drawable&)> c) { view.ForEachOpaqueDrawable(c); },
                view.GetLight().direction, cam.view, cam.projection, cam.viewPos, cam.nearPlane, cam.farPlane);
        }
        RenderForward(view);
        break;
    case PipelineMode::ForwardPlus:
        RenderForwardPlus(view);
        break;
    }

    if (view.GetSkybox()) {
        const bool useGbufferDepth = (m_mode == PipelineMode::Deferred);
        RenderSkybox(view, useGbufferDepth);
    }

    if (m_prePostCallback) {
        const RenderScene* sceneForCb = view.GetSceneForCallback();
        if (sceneForCb)
            m_prePostCallback(m_hdrRenderTarget, m_width, m_height, *sceneForCb);
    }

    if (eyelib::debug::GetPresets().debugDrawEnabled.load()) {
        rhi::IRenderTarget* target = m_hdrRenderTarget;
        DebugDrawRenderer::Render(cam, target, m_width, m_height);
    }
    
    if (m_postProcessor) {
        PostProcessor::Settings settings;
        math::Mat4 invView = glm::inverse(cam.view);
        math::Mat4 invProj = glm::inverse(cam.projection);
        auto skybox = view.GetSkybox();
        rhi::ITexture* skyboxTex = skybox && skybox->IsValid() ? skybox->GetTextureRHI() : nullptr;
        float time = core::Time::TotalTime();
        rhi::ITexture* hdrTex = GetHDRColorTextureRHI();
        rhi::ITexture* finalTexture = m_postProcessor->ApplyPostProcessing(
            hdrTex, *m_gbuffer, settings,
            cam.view, cam.projection,
            invView, invProj, cam.viewPos, skyboxTex, time);
        (void)finalTexture;
    }
    
    m_prevView = cam.view;
    m_prevProjection = cam.projection;

    if (cmd) cmd->End();
#ifndef NDEBUG
    m_rendering = false;
#endif
}

void RenderPipeline::BlitToScreen() {
    if (!m_device) return;
    // Clear swap chain to a known background (Diligent/Vulkan don't clear it elsewhere; avoids white/garbage)
    rhi::ICommandList* cmd = GetRHICommandList();
    if (cmd) {
        rhi::ClearValue bg = {};
        bg.color[0] = 0.12f; bg.color[1] = 0.12f; bg.color[2] = 0.15f; bg.color[3] = 1.f;
        cmd->ClearDefaultFramebuffer(bg);
    }
    if (m_postProcessor && m_postProcessor->GetOutputRenderTargetRHI() && m_postProcessor->HasValidOutputFrame()) {
        m_device->BlitToDefaultFramebuffer(m_postProcessor->GetOutputRenderTargetRHI());
    } else if (m_hdrRenderTarget) {
        m_device->BlitToDefaultFramebuffer(m_hdrRenderTarget);
    }
    m_device->Present();
}

bool RenderPipeline::HasValidOutput() const {
    return m_hdrRenderTarget != nullptr && m_gbuffer != nullptr;
}

void RenderPipeline::EnsureSkyboxShader() {
    if (m_skyboxShader && m_skyboxShader->IsValid()) return;
    if (!m_device) return;
    m_skyboxShader = std::make_shared<Shader>();
    m_skyboxShader->Load(m_device.get(), "passes/sky/skybox", Mesh::GetVertexLayoutRHI());
}

void RenderPipeline::RenderSkyboxImpl(const CameraView& camera, const std::shared_ptr<TextureCube>& skybox, bool useGbufferDepth) {
    if (!skybox || !skybox->IsValid()) return;
    EnsureSkyboxShader();
    if (!m_skyboxShader || !m_skyboxShader->IsValid()) return;

    rhi::ICommandList* cmdSky = GetRHICommandList();
    if (cmdSky && m_hdrRenderTarget) {
        cmdSky->SetRenderTarget(m_hdrRenderTarget);
        cmdSky->SetViewport(0, 0, m_width, m_height);
    }
    if (useGbufferDepth && m_gbuffer && m_device && m_hdrRenderTarget)
        m_device->BlitDepthOnly(m_gbuffer->GetRenderTargetRHI(), m_hdrRenderTarget);
    if (cmdSky && m_hdrRenderTarget)
        cmdSky->SetRenderTarget(m_hdrRenderTarget);

    if (cmdSky) { cmdSky->SetDepthTest(true); cmdSky->SetDepthFuncLess(false); cmdSky->SetDepthWrite(false); cmdSky->SetCullFace(false); }

    m_skyboxShader->Bind();
    m_skyboxShader->SetMat4("u_InverseViewMatrix", glm::inverse(camera.view));
    m_skyboxShader->SetMat4("u_InverseProjectionMatrix", glm::inverse(camera.projection));
    m_skyboxShader->SetInt("u_Skybox", 0);
    skybox->Bind(0);

    static FullscreenQuad quad;
    quad.Render();

    m_skyboxShader->Unbind();
    if (cmdSky) { cmdSky->SetDepthWrite(true); cmdSky->SetDepthFuncLess(true); cmdSky->SetCullFace(true); cmdSky->SetRenderTarget(nullptr); }
}

void RenderPipeline::RenderSkybox(const IRenderSceneView& view, bool useGbufferDepth) {
    RenderSkyboxImpl(view.GetCamera(), view.GetSkybox(), useGbufferDepth);
}

GBuffer* RenderPipeline::GetGBuffer() { return m_gbuffer.get(); }
ShadowMap* RenderPipeline::GetShadowMap() { return m_shadowMap.get(); }
PostProcessor* RenderPipeline::GetPostProcessor() { return m_postProcessor.get(); }

const std::vector<FrameGraph::PassInfo>& RenderPipeline::GetFrameGraphPasses() const {
    static const std::vector<FrameGraph::PassInfo> empty;
    return m_frameGraph ? m_frameGraph->GetPasses() : empty;
}

} // namespace render
} // namespace eyelib
