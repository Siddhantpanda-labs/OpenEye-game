#include "eyelib/render/RenderPipeline.h"
#include "api/render/RenderScene.h"
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
    if (!device) return false;
    m_width = width;
    m_height = height;
    m_device = std::move(device);
    rhi::SetDefaultDevice(m_device.get());
    m_gbuffer = std::make_unique<GBuffer>();
    if (!m_gbuffer->Initialize(width, height)) return false;
    m_shadowMap = std::make_unique<ShadowMap>();
    if (!m_shadowMap->Initialize(m_device.get(), 2048)) return false;
    m_gbufferPass = std::make_unique<GBufferPass>();
    m_shadowPass = std::make_unique<ShadowPass>();
    m_lightingPass = std::make_unique<LightingPass>();
    m_frameGraph = std::make_unique<FrameGraph>();
    m_frameGraph->SetDevice(m_device.get());
    m_frameGraph->SetFrameSize(width, height);
    m_clusteredLightGrid = std::make_unique<ClusteredLightGrid>();
    m_clusteredLightGrid->Initialize(width, height, 0.1f, 1000.0f);  // Default, will be updated per-frame
    m_postProcessor = std::make_unique<PostProcessor>();
    if (!m_postProcessor->Initialize(width, height)) return false;
    CreateHDRFramebuffer();

    // Initialize previous matrices (identity for first frame)
    m_prevView = math::Mat4(1.0f);
    m_prevProjection = math::Mat4(1.0f);

    return m_hdrRenderTarget != nullptr;
}

void RenderPipeline::Resize(uint32_t width, uint32_t height) {
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
    if (m_device && m_hdrRenderTarget) {
        m_device->DestroyRenderTarget(m_hdrRenderTarget);
        m_hdrRenderTarget = nullptr;
    }
}

rhi::ITexture* RenderPipeline::GetHDRColorTextureRHI() const {
    return m_hdrRenderTarget ? m_hdrRenderTarget->GetColorTexture(0) : nullptr;
}

void RenderPipeline::BuildAndExecuteFrameGraph(const RenderScene& scene) {
    if (!m_frameGraph || !m_gbuffer || !m_shadowMap || !m_gbufferPass || !m_shadowPass || !m_lightingPass) return;
    m_frameGraph->Clear();
    m_frameGraph->SetFrameSize(m_width, m_height);

    const auto& cam = scene.camera;
    const auto& light = scene.light;

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
            [this, &scene, &light, &cam, cmd](const FGPassResources&) {
                m_shadowPass->Render(*m_shadowMap, cmd, scene.opaque, light.direction,
                    cam.view, cam.projection, cam.viewPos, cam.nearPlane, cam.farPlane);
            },
            {}, { shadowTex }, true);
    }

    m_frameGraph->AddPass("GBuffer",
        [this, &scene, &cam, cmd](const FGPassResources&) {
            m_gbufferPass->Render(*m_gbuffer, cmd, scene.opaque, cam.view, cam.projection, m_prevView, m_prevProjection);
        },
        {}, { gbPos, gbNorm, gbAlb, gbPBR, gbVelocity, gbDepth }, true);

    std::vector<FGTextureHandle> lightingReads = { gbPos, gbNorm, gbAlb, gbPBR };
    if (light.castShadows) lightingReads.push_back(shadowTex);
    rhi::ClearValue clearBlack = { 0.f, 0.f, 0.f, 1.f };
    m_frameGraph->AddPass("Lighting",
        [this, &scene, &cam, cmd, clearBlack](const FGPassResources&) {
            if (cmd && m_hdrRenderTarget)
                rhi::ClearAndSetRenderTargetWithViewport(cmd, m_hdrRenderTarget, 0, 0, m_width, m_height, clearBlack);
            ShadowMap* shadowMapForLighting = scene.light.castShadows ? m_shadowMap.get() : nullptr;
            m_lightingPass->Render(*m_gbuffer, cmd, scene.light, cam.view, cam.projection, cam.viewPos, shadowMapForLighting,
                &scene.pointLights, &scene.spotLights);
        },
        lightingReads, { hdrColor }, true);

    m_frameGraph->AddPass("Transparency",
        [this, &scene, &cam, cmd](const FGPassResources&) {
            if (m_device && m_hdrRenderTarget)
                m_device->BlitDepthOnlyFromNativeFramebuffer(m_gbuffer->GetNativeFramebuffer(), m_hdrRenderTarget);
            if (cmd && m_hdrRenderTarget) {
                cmd->SetRenderTarget(m_hdrRenderTarget);
                cmd->SetViewport(0, 0, m_width, m_height);
            }
            TransparencyPass::Render(cmd, scene.transparent, cam.view, cam.projection, cam.viewPos);
        },
        {}, { hdrColor }, true);

    m_frameGraph->Compile();
    m_frameGraph->Execute(GetRHICommandList());
}

void RenderPipeline::BuildAndExecuteFrameGraph(const api::RenderScene& scene) {
    if (!m_frameGraph || !m_gbuffer || !m_shadowMap || !m_gbufferPass || !m_shadowPass || !m_lightingPass) return;
    m_frameGraph->Clear();
    m_frameGraph->SetFrameSize(m_width, m_height);

    const auto& cam = scene.camera;
    const auto& light = scene.light;

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
            [this, &scene, &light, &cam, cmd](const FGPassResources&) {
                m_shadowPass->Render(*m_shadowMap, cmd, std::span<const api::Drawable>(scene.opaque.data(), scene.opaque.size()), light.direction,
                    cam.view, cam.projection, cam.viewPos, cam.nearPlane, cam.farPlane);
            },
            {}, { shadowTex }, true);
    }

    m_frameGraph->AddPass("GBuffer",
        [this, &scene, &cam, cmd](const FGPassResources&) {
            m_gbufferPass->Render(*m_gbuffer, cmd, std::span<const api::Drawable>(scene.opaque.data(), scene.opaque.size()), cam.view, cam.projection, m_prevView, m_prevProjection);
        },
        {}, { gbPos, gbNorm, gbAlb, gbPBR, gbVelocity, gbDepth }, true);

    std::vector<FGTextureHandle> lightingReads = { gbPos, gbNorm, gbAlb, gbPBR };
    if (light.castShadows) lightingReads.push_back(shadowTex);
    rhi::ClearValue clearBlack = { 0.f, 0.f, 0.f, 1.f };
    m_frameGraph->AddPass("Lighting",
        [this, &scene, &cam, cmd, clearBlack](const FGPassResources&) {
            if (cmd && m_hdrRenderTarget)
                rhi::ClearAndSetRenderTargetWithViewport(cmd, m_hdrRenderTarget, 0, 0, m_width, m_height, clearBlack);
            ShadowMap* shadowMapForLighting = scene.light.castShadows ? m_shadowMap.get() : nullptr;
            m_lightingPass->Render(*m_gbuffer, cmd, scene.light, cam.view, cam.projection, cam.viewPos, shadowMapForLighting,
                std::span<const api::render::PointLight>(scene.pointLights.data(), scene.pointLights.size()),
                std::span<const api::render::SpotLight>(scene.spotLights.data(), scene.spotLights.size()));
        },
        lightingReads, { hdrColor }, true);

    m_frameGraph->AddPass("Transparency",
        [this, &scene, &cam, cmd](const FGPassResources&) {
            if (m_device && m_hdrRenderTarget)
                m_device->BlitDepthOnlyFromNativeFramebuffer(m_gbuffer->GetNativeFramebuffer(), m_hdrRenderTarget);
            if (cmd && m_hdrRenderTarget) {
                cmd->SetRenderTarget(m_hdrRenderTarget);
                cmd->SetViewport(0, 0, m_width, m_height);
            }
            TransparencyPass::Render(cmd, std::span<const api::Drawable>(scene.transparent.data(), scene.transparent.size()), cam.view, cam.projection, cam.viewPos);
        },
        {}, { hdrColor }, true);

    m_frameGraph->Compile();
    m_frameGraph->Execute(GetRHICommandList());
}

void RenderPipeline::RenderForward(const RenderScene& scene) {
    m_frameGraph->Clear();
    m_frameGraph->SetFrameSize(m_width, m_height);
    rhi::ICommandList* cmd = GetRHICommandList();
    if (cmd && m_hdrRenderTarget) {
        rhi::ClearValue cv = { 0.1f, 0.1f, 0.15f, 1.f };
        rhi::ClearAndSetRenderTargetWithViewport(cmd, m_hdrRenderTarget, 0, 0, m_width, m_height, cv);
    }
    if (cmd) { cmd->SetDepthTest(true); cmd->SetCullFace(false); }

    const int kShadowMapUnit = 5;
    if (m_shadowMap && cmd) {
        rhi::ITexture* shadowTex = m_shadowMap->GetDepthTextureRHI();
        if (shadowTex) cmd->BindTexture(shadowTex, kShadowMapUnit);
    }

    const int kMaxPointLights = 8;
    const int kMaxSpotLights = 4;
    const int numPoint = static_cast<int>(std::min(scene.pointLights.size(), size_t(kMaxPointLights)));
    const int numSpot = static_cast<int>(std::min(scene.spotLights.size(), size_t(kMaxSpotLights)));

    for (const auto& d : scene.opaque) {
        if (!d.mesh || !d.material) continue;
        if (d.materialInstance) {
            d.materialInstance->Bind();
        } else {
            d.material->Bind();
        }
        Shader* sh = (d.materialInstance ? d.materialInstance->GetMaterial() : d.material)->GetShader().get();
        if (sh) {
            sh->SetMat4("u_Model", d.transform);
            sh->SetMat4("u_NormalMatrix", math::Mat4FromMat3(math::NormalMatrix(d.transform)));
            sh->SetMat4("u_View", scene.camera.view);
            sh->SetMat4("u_Projection", scene.camera.projection);
            sh->SetVec3("u_ViewPos", scene.camera.viewPos);
            sh->SetVec3("u_LightDir", scene.light.direction);
            sh->SetVec3("u_LightColor", scene.light.color);
            sh->SetFloat("u_LightIntensity", scene.light.intensity);
            sh->SetVec3("u_Ambient", scene.light.ambient);
            sh->SetFloat("u_Exposure", scene.light.exposure);
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
                const auto& p = scene.pointLights[static_cast<size_t>(i)];
                sh->SetVec3("u_PointLightPos[" + std::to_string(i) + "]", p.position);
                sh->SetVec3("u_PointLightColor[" + std::to_string(i) + "]", p.color);
                sh->SetFloat("u_PointLightIntensity[" + std::to_string(i) + "]", p.intensity);
                sh->SetFloat("u_PointLightRadius[" + std::to_string(i) + "]", p.radius);
            }
            sh->SetInt("u_SpotLightCount", numSpot);
            for (int i = 0; i < numSpot; ++i) {
                const auto& s = scene.spotLights[static_cast<size_t>(i)];
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
        }
    }

    if (m_shadowMap && cmd) cmd->UnbindTexture(kShadowMapUnit);

    TransparencyPass::Render(cmd, scene.transparent, scene.camera.view, scene.camera.projection, scene.camera.viewPos);
    if (cmd) { cmd->SetCullFace(true); cmd->SetRenderTarget(nullptr); }
}

void RenderPipeline::RenderForward(const api::RenderScene& scene) {
    m_frameGraph->Clear();
    m_frameGraph->SetFrameSize(m_width, m_height);
    rhi::ICommandList* cmd = GetRHICommandList();
    if (cmd && m_hdrRenderTarget) {
        rhi::ClearValue cv = { 0.1f, 0.1f, 0.15f, 1.f };
        rhi::ClearAndSetRenderTargetWithViewport(cmd, m_hdrRenderTarget, 0, 0, m_width, m_height, cv);
    }
    if (cmd) { cmd->SetDepthTest(true); cmd->SetCullFace(false); }

    const int kShadowMapUnit = 5;
    if (m_shadowMap && cmd) {
        rhi::ITexture* shadowTex = m_shadowMap->GetDepthTextureRHI();
        if (shadowTex) cmd->BindTexture(shadowTex, kShadowMapUnit);
    }

    const int kMaxPointLights = 8;
    const int kMaxSpotLights = 4;
    const int numPoint = static_cast<int>(std::min(scene.pointLights.size(), size_t(kMaxPointLights)));
    const int numSpot = static_cast<int>(std::min(scene.spotLights.size(), size_t(kMaxSpotLights)));

    const int kAlbedoTextureUnit = 0, kNormalTextureUnit = 1, kMRTextureUnit = 2, kAOTextureUnit = 3, kEmissiveTextureUnit = 4;
    for (const auto& apiD : scene.opaque) {
        Mesh* mesh = apiD.mesh ? apiD.mesh->GetInternal().get() : nullptr;
        Material* material = apiD.material ? apiD.material->GetInternal().get() : nullptr;
        if (!mesh || !material) continue;
        Shader* sh = material->GetShader().get();
        if (!sh) continue;
        sh->Bind();
        sh->SetMat4("u_Model", apiD.transform);
        sh->SetMat4("u_NormalMatrix", math::Mat4FromMat3(math::NormalMatrix(apiD.transform)));
        sh->SetMat4("u_View", scene.camera.view);
        sh->SetMat4("u_Projection", scene.camera.projection);
        sh->SetVec3("u_ViewPos", scene.camera.viewPos);
        sh->SetVec3("u_LightDir", scene.light.direction);
        sh->SetVec3("u_LightColor", scene.light.color);
        sh->SetFloat("u_LightIntensity", scene.light.intensity);
        sh->SetVec3("u_Ambient", scene.light.ambient);
        sh->SetFloat("u_Exposure", scene.light.exposure);
        if (m_shadowMap) {
            sh->SetInt("u_ShadowMap", kShadowMapUnit);
            sh->SetMat4("u_LightSpaceMatrix", m_shadowMap->GetShadowMatrix());
            sh->SetFloat("u_ShadowBias", 0.005f);
            sh->SetInt("u_UseShadows", 1);
        } else {
            sh->SetInt("u_UseShadows", 0);
        }
        if (apiD.bones && apiD.boneCount > 0) {
            const size_t boneCount = std::min(apiD.boneCount, kMaxBones);
            sh->SetMat4Array("u_Bones", apiD.bones, boneCount);
        }
        sh->SetInt("u_PointLightCount", numPoint);
        for (int i = 0; i < numPoint; ++i) {
            const auto& p = scene.pointLights[static_cast<size_t>(i)];
            sh->SetVec3("u_PointLightPos[" + std::to_string(i) + "]", p.position);
            sh->SetVec3("u_PointLightColor[" + std::to_string(i) + "]", p.color);
            sh->SetFloat("u_PointLightIntensity[" + std::to_string(i) + "]", p.intensity);
            sh->SetFloat("u_PointLightRadius[" + std::to_string(i) + "]", p.radius);
        }
        sh->SetInt("u_SpotLightCount", numSpot);
        for (int i = 0; i < numSpot; ++i) {
            const auto& s = scene.spotLights[static_cast<size_t>(i)];
            sh->SetVec3("u_SpotLightPos[" + std::to_string(i) + "]", s.position);
            sh->SetVec3("u_SpotLightDir[" + std::to_string(i) + "]", s.direction);
            sh->SetVec3("u_SpotLightColor[" + std::to_string(i) + "]", s.color);
            sh->SetFloat("u_SpotLightIntensity[" + std::to_string(i) + "]", s.intensity);
            sh->SetFloat("u_SpotLightRange[" + std::to_string(i) + "]", s.range);
            sh->SetFloat("u_SpotLightInnerCos[" + std::to_string(i) + "]", s.innerConeCos);
            sh->SetFloat("u_SpotLightOuterCos[" + std::to_string(i) + "]", s.outerConeCos);
        }
        const auto& p = apiD.params;
        sh->SetVec3("u_Albedo", p.albedo);
        sh->SetFloat("u_Metallic", p.metallic);
        sh->SetFloat("u_Roughness", p.roughness);
        sh->SetFloat("u_AO", p.ao);
        sh->SetVec3("u_Emission", p.emission);
        sh->SetFloat("u_Alpha", p.alpha);
        sh->SetVec4("u_UVTransform", p.uvTransform);
        sh->SetFloat("u_UVRotation", p.uvRotation);
        sh->SetFloat("u_TexCoordIndex", p.texCoordIndex);
        sh->SetFloat("u_FlipV", p.flipV);
        auto bindTex = [sh](int unit, const std::shared_ptr<api::Texture>& tex, const char* name, const char* useName) {
            if (tex && tex->IsValid()) {
                tex->GetInternal()->Bind(unit);
                sh->SetInt(name, unit);
                if (useName) sh->SetInt(useName, 1);
            } else if (useName) { sh->SetInt(useName, 0); }
        };
        bindTex(kAlbedoTextureUnit, p.albedoTexture, "u_AlbedoTexture", "u_UseAlbedoTexture");
        bindTex(kNormalTextureUnit, p.normalTexture, "u_NormalTexture", "u_UseNormalTexture");
        bindTex(kMRTextureUnit, p.metallicRoughnessTexture, "u_MetallicRoughnessTexture", "u_UseMetallicRoughnessTexture");
        bindTex(kAOTextureUnit, p.aoTexture, "u_AOTexture", "u_UseAOTexture");
        bindTex(kEmissiveTextureUnit, p.emissiveTexture, "u_EmissiveTexture", "u_UseEmissiveTexture");
        mesh->Bind();
        mesh->Draw();
        mesh->Unbind();
        sh->Unbind();
    }

    if (m_shadowMap && cmd) cmd->UnbindTexture(kShadowMapUnit);

    TransparencyPass::Render(cmd, std::span<const api::Drawable>(scene.transparent.data(), scene.transparent.size()), scene.camera.view, scene.camera.projection, scene.camera.viewPos);
    if (cmd) { cmd->SetCullFace(true); cmd->SetRenderTarget(nullptr); }
}

void RenderPipeline::RenderForwardPlus(const RenderScene& scene) {
    if (!m_clusteredLightGrid) {
        RenderForward(scene);
        return;
    }

    // Reinitialize clustered grid if camera planes changed
    if (m_clusteredLightGrid->GetNearPlane() != scene.camera.nearPlane ||
        m_clusteredLightGrid->GetFarPlane() != scene.camera.farPlane) {
        m_clusteredLightGrid->Initialize(m_width, m_height, scene.camera.nearPlane, scene.camera.farPlane);
    }

    // Build clustered light grid
    m_clusteredLightGrid->BuildLightGrid(scene.pointLights, scene.spotLights,
                                         scene.camera.view, scene.camera.projection, scene.camera.viewPos);

    // Upload lights to SSBOs (RHI)
    rhi::ICommandList* cmdFwd = GetRHICommandList();
    const size_t kMaxPointLights = 256;
    const size_t kMaxSpotLights = 64;
    const size_t kMaxLightIndices = 4 * 1024 * 1024;
    if (!scene.pointLights.empty() && m_device) {
        if (!m_pointLightBuffer) {
            rhi::BufferDesc bd;
            bd.sizeInBytes = kMaxPointLights * sizeof(PointLight);
            bd.usage = rhi::BufferUsage::Storage;
            bd.memoryHint = rhi::BufferMemoryHint::HostVisible;
            m_pointLightBuffer = m_device->CreateBuffer(bd, nullptr);
        }
        if (m_pointLightBuffer) {
            const size_t sz = scene.pointLights.size() * sizeof(PointLight);
            m_pointLightBuffer->Update(scene.pointLights.data(), 0, sz);
            if (cmdFwd) cmdFwd->BindStorageBuffer(m_pointLightBuffer, 3, 0, static_cast<uint32_t>(sz));
        }
    }
    if (!scene.spotLights.empty() && m_device) {
        if (!m_spotLightBuffer) {
            rhi::BufferDesc bd;
            bd.sizeInBytes = kMaxSpotLights * sizeof(SpotLight);
            bd.usage = rhi::BufferUsage::Storage;
            bd.memoryHint = rhi::BufferMemoryHint::HostVisible;
            m_spotLightBuffer = m_device->CreateBuffer(bd, nullptr);
        }
        if (m_spotLightBuffer) {
            const size_t sz = scene.spotLights.size() * sizeof(SpotLight);
            m_spotLightBuffer->Update(scene.spotLights.data(), 0, sz);
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

    if (scene.light.castShadows && m_shadowMap && m_shadowPass) {
        m_shadowPass->Render(*m_shadowMap, GetRHICommandList(), scene.opaque, scene.light.direction,
            scene.camera.view, scene.camera.projection, scene.camera.viewPos,
            scene.camera.nearPlane, scene.camera.farPlane);
    }

    // Ensure forward+ shader exists
    if (!m_forwardPlusShader || !m_forwardPlusShader->IsValid()) {
        m_forwardPlusShader = std::make_shared<Shader>();
        m_forwardPlusShader->LoadFromFile("assets/shaders/passes/forward/forwardplus.vert", "assets/shaders/passes/forward/forwardplus.frag");
    }

    if (!m_forwardPlusShader->IsValid()) {
        RenderForward(scene);
        return;
    }

    if (!m_forwardPlusSkinnedShader || !m_forwardPlusSkinnedShader->IsValid()) {
        m_forwardPlusSkinnedShader = std::make_shared<Shader>();
        m_forwardPlusSkinnedShader->LoadFromFile("assets/shaders/passes/forward/forwardplus_skinned.vert", "assets/shaders/passes/forward/forwardplus.frag");
    }

    if (cmdFwd && m_hdrRenderTarget) {
        rhi::ClearValue cv = { 0.1f, 0.1f, 0.15f, 1.f };
        rhi::ClearAndSetRenderTargetWithViewport(cmdFwd, m_hdrRenderTarget, 0, 0, m_width, m_height, cv);
    }
    if (cmdFwd) { cmdFwd->SetDepthTest(true); cmdFwd->SetCullFace(false); }

    const int kShadowMapUnit = 5;
    if (scene.light.castShadows && m_shadowMap && cmdFwd) {
        rhi::ITexture* shadowTex = m_shadowMap->GetDepthTextureRHI();
        if (shadowTex) cmdFwd->BindTexture(shadowTex, kShadowMapUnit);
    }

    auto setCommonForwardPlusUniforms = [&](Shader* sh) {
        sh->SetMat4("u_View", scene.camera.view);
        sh->SetMat4("u_Projection", scene.camera.projection);
        sh->SetVec3("u_ViewPos", scene.camera.viewPos);
        sh->SetVec3("u_DirLightDir", scene.light.direction);
        sh->SetVec3("u_DirLightColor", scene.light.color);
        sh->SetFloat("u_DirLightIntensity", scene.light.intensity);
        sh->SetVec3("u_Ambient", scene.light.ambient);
        sh->SetFloat("u_Exposure", scene.light.exposure);
        sh->SetInt("u_ScreenWidth", static_cast<int>(m_width));
        sh->SetInt("u_ScreenHeight", static_cast<int>(m_height));
        sh->SetInt("u_TileCountX", static_cast<int>(m_clusteredLightGrid->GetTileCountX()));
        sh->SetInt("u_TileCountY", static_cast<int>(m_clusteredLightGrid->GetTileCountY()));
        sh->SetInt("u_SliceCount", static_cast<int>(m_clusteredLightGrid->GetSliceCount()));
        sh->SetFloat("u_LogNear", m_clusteredLightGrid->GetLogNear());
        sh->SetFloat("u_LogFar", m_clusteredLightGrid->GetLogFar());
        sh->SetFloat("u_NearPlane", m_clusteredLightGrid->GetNearPlane());
        sh->SetFloat("u_FarPlane", m_clusteredLightGrid->GetFarPlane());
        sh->SetInt("u_PointLightCount", static_cast<int>(scene.pointLights.size()));
        sh->SetInt("u_SpotLightCount", static_cast<int>(scene.spotLights.size()));
        if (scene.light.castShadows && m_shadowMap) {
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

    for (const auto& d : scene.opaque) {
        if (!d.mesh) continue;
        const auto material = d.materialInstance ? d.materialInstance->GetMaterial() : d.material;
        if (!material) continue;

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
    }

    m_forwardPlusShader->Unbind();
    if (m_forwardPlusSkinnedShader) m_forwardPlusSkinnedShader->Unbind();
    if (scene.light.castShadows && m_shadowMap && cmdFwd) cmdFwd->UnbindTexture(kShadowMapUnit);
    TransparencyPass::Render(cmdFwd, scene.transparent, scene.camera.view, scene.camera.projection, scene.camera.viewPos);
    if (cmdFwd) { cmdFwd->SetCullFace(true); cmdFwd->SetRenderTarget(nullptr); }
}

void RenderPipeline::RenderForwardPlus(const api::RenderScene& scene) {
    if (!m_clusteredLightGrid) {
        RenderForward(scene);
        return;
    }

    if (m_clusteredLightGrid->GetNearPlane() != scene.camera.nearPlane ||
        m_clusteredLightGrid->GetFarPlane() != scene.camera.farPlane) {
        m_clusteredLightGrid->Initialize(m_width, m_height, scene.camera.nearPlane, scene.camera.farPlane);
    }

    m_scratchPointLights.clear();
    m_scratchPointLights.reserve(scene.pointLights.size());
    for (const auto& p : scene.pointLights) {
        PointLight lp;
        lp.position = p.position;
        lp.color = p.color;
        lp.intensity = p.intensity;
        lp.radius = p.radius;
        m_scratchPointLights.push_back(lp);
    }
    m_scratchSpotLights.clear();
    m_scratchSpotLights.reserve(scene.spotLights.size());
    for (const auto& s : scene.spotLights) {
        SpotLight ls;
        ls.position = s.position;
        ls.direction = s.direction;
        ls.color = s.color;
        ls.intensity = s.intensity;
        ls.range = s.range;
        ls.innerConeCos = s.innerConeCos;
        ls.outerConeCos = s.outerConeCos;
        m_scratchSpotLights.push_back(ls);
    }

    m_clusteredLightGrid->BuildLightGrid(m_scratchPointLights, m_scratchSpotLights,
                                         scene.camera.view, scene.camera.projection, scene.camera.viewPos);

    rhi::ICommandList* cmdFwdApi = GetRHICommandList();
    const size_t kMaxPointLightsApi = 256;
    const size_t kMaxSpotLightsApi = 64;
    const size_t kMaxLightIndicesApi = 4 * 1024 * 1024;
    if (!m_scratchPointLights.empty() && m_device) {
        if (!m_pointLightBuffer) {
            rhi::BufferDesc bd;
            bd.sizeInBytes = kMaxPointLightsApi * sizeof(PointLight);
            bd.usage = rhi::BufferUsage::Storage;
            bd.memoryHint = rhi::BufferMemoryHint::HostVisible;
            m_pointLightBuffer = m_device->CreateBuffer(bd, nullptr);
        }
        if (m_pointLightBuffer) {
            const size_t sz = m_scratchPointLights.size() * sizeof(PointLight);
            m_pointLightBuffer->Update(m_scratchPointLights.data(), 0, sz);
            if (cmdFwdApi) cmdFwdApi->BindStorageBuffer(m_pointLightBuffer, 3, 0, static_cast<uint32_t>(sz));
        }
    }
    if (!m_scratchSpotLights.empty() && m_device) {
        if (!m_spotLightBuffer) {
            rhi::BufferDesc bd;
            bd.sizeInBytes = kMaxSpotLightsApi * sizeof(SpotLight);
            bd.usage = rhi::BufferUsage::Storage;
            bd.memoryHint = rhi::BufferMemoryHint::HostVisible;
            m_spotLightBuffer = m_device->CreateBuffer(bd, nullptr);
        }
        if (m_spotLightBuffer) {
            const size_t sz = m_scratchSpotLights.size() * sizeof(SpotLight);
            m_spotLightBuffer->Update(m_scratchSpotLights.data(), 0, sz);
            if (cmdFwdApi) cmdFwdApi->BindStorageBuffer(m_spotLightBuffer, 4, 0, static_cast<uint32_t>(sz));
        }
    }
    if (m_device && !m_lightIndexBuffer) {
        rhi::BufferDesc bd;
        bd.sizeInBytes = kMaxLightIndicesApi;
        bd.usage = rhi::BufferUsage::Storage;
        bd.memoryHint = rhi::BufferMemoryHint::HostVisible;
        m_lightIndexBuffer = m_device->CreateBuffer(bd, nullptr);
    }
    m_clusteredLightGrid->UploadToGPU(m_lightIndexBuffer);
    if (cmdFwdApi && m_lightIndexBuffer) cmdFwdApi->BindStorageBuffer(m_lightIndexBuffer, 2);

    if (scene.light.castShadows && m_shadowMap && m_shadowPass) {
        m_shadowPass->Render(*m_shadowMap, GetRHICommandList(), std::span<const api::Drawable>(scene.opaque.data(), scene.opaque.size()), scene.light.direction,
            scene.camera.view, scene.camera.projection, scene.camera.viewPos,
            scene.camera.nearPlane, scene.camera.farPlane);
    }

    if (!m_forwardPlusShader || !m_forwardPlusShader->IsValid()) {
        m_forwardPlusShader = std::make_shared<Shader>();
        m_forwardPlusShader->LoadFromFile("assets/shaders/passes/forward/forwardplus.vert", "assets/shaders/passes/forward/forwardplus.frag");
    }
    if (!m_forwardPlusShader->IsValid()) {
        RenderForward(scene);
        return;
    }
    if (!m_forwardPlusSkinnedShader || !m_forwardPlusSkinnedShader->IsValid()) {
        m_forwardPlusSkinnedShader = std::make_shared<Shader>();
        m_forwardPlusSkinnedShader->LoadFromFile("assets/shaders/passes/forward/forwardplus_skinned.vert", "assets/shaders/passes/forward/forwardplus.frag");
    }

    if (cmdFwdApi && m_hdrRenderTarget) {
        rhi::ClearValue cv = { 0.1f, 0.1f, 0.15f, 1.f };
        rhi::ClearAndSetRenderTargetWithViewport(cmdFwdApi, m_hdrRenderTarget, 0, 0, m_width, m_height, cv);
    }
    if (cmdFwdApi) { cmdFwdApi->SetDepthTest(true); cmdFwdApi->SetCullFace(false); }

    const int kShadowMapUnit = 5;
    if (scene.light.castShadows && m_shadowMap && cmdFwdApi) {
        rhi::ITexture* shadowTex = m_shadowMap->GetDepthTextureRHI();
        if (shadowTex) cmdFwdApi->BindTexture(shadowTex, kShadowMapUnit);
    }

    auto setCommon = [&](Shader* sh) {
        sh->SetMat4("u_View", scene.camera.view);
        sh->SetMat4("u_Projection", scene.camera.projection);
        sh->SetVec3("u_ViewPos", scene.camera.viewPos);
        sh->SetVec3("u_DirLightDir", scene.light.direction);
        sh->SetVec3("u_DirLightColor", scene.light.color);
        sh->SetFloat("u_DirLightIntensity", scene.light.intensity);
        sh->SetVec3("u_Ambient", scene.light.ambient);
        sh->SetFloat("u_Exposure", scene.light.exposure);
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
        if (scene.light.castShadows && m_shadowMap) {
            sh->SetInt("u_ShadowMap", kShadowMapUnit);
            sh->SetMat4("u_LightSpaceMatrix", m_shadowMap->GetShadowMatrix());
            sh->SetFloat("u_ShadowBias", 0.005f);
            sh->SetInt("u_UseShadows", 1);
        } else {
            sh->SetInt("u_UseShadows", 0);
        }
    };

    const int kAlbedoTextureUnit = 0, kNormalTextureUnit = 1, kMetallicRoughnessTextureUnit = 2, kAOTextureUnit = 3, kEmissiveTextureUnit = 4;
    bool commonSetNonSkinned = false, commonSetSkinned = false;

    for (const auto& apiD : scene.opaque) {
        Mesh* mesh = apiD.mesh ? apiD.mesh->GetInternal().get() : nullptr;
        Material* material = apiD.material ? apiD.material->GetInternal().get() : nullptr;
        if (!mesh || !material) continue;

        const bool useSkinned = (apiD.bones && apiD.boneCount > 0 && m_forwardPlusSkinnedShader && m_forwardPlusSkinnedShader->IsValid());
        Shader* sh = useSkinned ? m_forwardPlusSkinnedShader.get() : m_forwardPlusShader.get();
        sh->Bind();
        if (useSkinned) { if (!commonSetSkinned) { setCommon(sh); commonSetSkinned = true; } }
        else { if (!commonSetNonSkinned) { setCommon(sh); commonSetNonSkinned = true; } }

        sh->SetMat4("u_Model", apiD.transform);
        sh->SetMat4("u_NormalMatrix", math::Mat4FromMat3(math::NormalMatrix(apiD.transform)));
        if (useSkinned) {
            const size_t boneCount = std::min(apiD.boneCount, kMaxBones);
            sh->SetMat4Array("u_Bones", apiD.bones, boneCount);
        }

        const auto& p = apiD.params;
        sh->SetVec4("u_UVTransform", p.uvTransform);
        sh->SetFloat("u_UVRotation", p.uvRotation);
        sh->SetFloat("u_TexCoordIndex", p.texCoordIndex);
        sh->SetFloat("u_FlipV", p.flipV);
        sh->SetVec3("u_Albedo", p.albedo);
        sh->SetFloat("u_Metallic", p.metallic);
        sh->SetFloat("u_Roughness", p.roughness);
        sh->SetFloat("u_AO", p.ao);
        sh->SetVec3("u_Emission", p.emission);
        sh->SetFloat("u_Alpha", p.alpha);

        auto bindTex = [sh](int unit, const std::shared_ptr<api::Texture>& tex, const char* name, const char* useName) {
            if (tex && tex->IsValid()) {
                tex->GetInternal()->Bind(unit);
                sh->SetInt(name, unit);
                if (useName) sh->SetInt(useName, 1);
            } else if (useName) { sh->SetInt(useName, 0); }
        };
        bindTex(kAlbedoTextureUnit, p.albedoTexture, "u_AlbedoTexture", "u_UseAlbedoTexture");
        bindTex(kNormalTextureUnit, p.normalTexture, "u_NormalTexture", "u_UseNormalTexture");
        bindTex(kMetallicRoughnessTextureUnit, p.metallicRoughnessTexture, "u_MetallicRoughnessTexture", "u_UseMetallicRoughnessTexture");
        bindTex(kAOTextureUnit, p.aoTexture, "u_AOTexture", "u_UseAOTexture");
        bindTex(kEmissiveTextureUnit, p.emissiveTexture, "u_EmissiveTexture", "u_UseEmissiveTexture");

        mesh->Bind();
        mesh->Draw();
        mesh->Unbind();
    }

    m_forwardPlusShader->Unbind();
    if (m_forwardPlusSkinnedShader) m_forwardPlusSkinnedShader->Unbind();
    if (scene.light.castShadows && m_shadowMap && cmdFwdApi) cmdFwdApi->UnbindTexture(kShadowMapUnit);
    TransparencyPass::Render(cmdFwdApi, std::span<const api::Drawable>(scene.transparent.data(), scene.transparent.size()), scene.camera.view, scene.camera.projection, scene.camera.viewPos);
    if (cmdFwdApi) { cmdFwdApi->SetCullFace(true); cmdFwdApi->SetRenderTarget(nullptr); }
}

void RenderPipeline::Render(const api::RenderScene& scene) {
    if (m_width == 0 || m_height == 0) return;
    EnsureResources(m_width, m_height);
    if (!m_hdrRenderTarget || !m_gbuffer || !m_gbuffer->GetNativeFramebuffer()) return;

    rhi::ICommandList* cmd = GetRHICommandList();
    if (!cmd) return;  // No command list available
    cmd->Begin();

    // Debug: log api scene contents (only first frame to avoid spam)
    static bool s_loggedApiScene = false;
    if (!s_loggedApiScene) {
        core::Log::InfoF("RenderPipeline::Render(api): scene.opaque=%zu scene.transparent=%zu",
                         scene.opaque.size(), scene.transparent.size());
        s_loggedApiScene = true;
    }

    switch (m_mode) {
    case PipelineMode::Deferred:
        BuildAndExecuteFrameGraph(scene);
        break;
    case PipelineMode::Forward:
        if (m_shadowMap && m_shadowPass) {
            m_shadowPass->Render(*m_shadowMap, GetRHICommandList(), std::span<const api::Drawable>(scene.opaque.data(), scene.opaque.size()), scene.light.direction,
                scene.camera.view, scene.camera.projection, scene.camera.viewPos,
                scene.camera.nearPlane, scene.camera.farPlane);
        }
        RenderForward(scene);
        break;
    case PipelineMode::ForwardPlus:
        RenderForwardPlus(scene);
        break;
    }

    if (scene.skybox) {
        const bool useGbufferDepth = (m_mode == PipelineMode::Deferred);
        RenderSkybox(scene, useGbufferDepth);
    }

    // Pre-post callback expects internal RenderScene; not invoked for api path
    if (eyelib::debug::GetPresets().debugDrawEnabled.load()) {
        CameraView cam;
        cam.view = scene.camera.view;
        cam.projection = scene.camera.projection;
        cam.viewPos = scene.camera.viewPos;
        cam.nearPlane = scene.camera.nearPlane;
        cam.farPlane = scene.camera.farPlane;
        rhi::IRenderTarget* target = m_hdrRenderTarget;
        DebugDrawRenderer::Render(cam, target, m_width, m_height);
    }

    if (m_postProcessor) {
        PostProcessor::Settings settings;
        math::Mat4 invView = glm::inverse(scene.camera.view);
        math::Mat4 invProj = glm::inverse(scene.camera.projection);
        rhi::ITexture* skyboxTex = scene.skybox && scene.skybox->IsValid() ? scene.skybox->GetInternal()->GetTextureRHI() : nullptr;
        float time = core::Time::TotalTime();
        rhi::ITexture* hdrTex = GetHDRColorTextureRHI();
        (void)m_postProcessor->ApplyPostProcessing(
            hdrTex, *m_gbuffer, settings,
            scene.camera.view, scene.camera.projection,
            invView, invProj, scene.camera.viewPos, skyboxTex, time);
    }

    m_prevView = scene.camera.view;
    m_prevProjection = scene.camera.projection;

    cmd->End();
}

void RenderPipeline::Render(const RenderScene& scene) {
    if (m_width == 0 || m_height == 0) return;
    EnsureResources(m_width, m_height);
    if (!m_hdrRenderTarget || !m_gbuffer || !m_gbuffer->GetNativeFramebuffer()) return;

    rhi::ICommandList* cmd = GetRHICommandList();
    if (!cmd) return;  // No command list available
    cmd->Begin();

    // Debug: log scene contents (only first frame to avoid spam)
    static bool s_loggedScene = false;
    if (!s_loggedScene) {
        core::Log::InfoF("RenderPipeline::Render: scene.opaque=%zu scene.transparent=%zu camera valid=%d",
                         scene.opaque.size(), scene.transparent.size(), (scene.camera.viewPos.x != 0 || scene.camera.viewPos.y != 0 || scene.camera.viewPos.z != 0));
        s_loggedScene = true;
    }

    switch (m_mode) {
    case PipelineMode::Deferred:
        BuildAndExecuteFrameGraph(scene);
        break;
    case PipelineMode::Forward:
        if (m_shadowMap && m_shadowPass) {
            m_shadowPass->Render(*m_shadowMap, GetRHICommandList(), scene.opaque, scene.light.direction,
                scene.camera.view, scene.camera.projection, scene.camera.viewPos,
                scene.camera.nearPlane, scene.camera.farPlane);
        }
        RenderForward(scene);
        break;
    case PipelineMode::ForwardPlus:
        RenderForwardPlus(scene);
        break;
    }

    if (scene.skybox) {
        const bool useGbufferDepth = (m_mode == PipelineMode::Deferred);
        RenderSkybox(scene, useGbufferDepth);
    }

    if (m_prePostCallback) {
        m_prePostCallback(m_hdrRenderTarget, m_width, m_height, scene);
    }

    if (eyelib::debug::GetPresets().debugDrawEnabled.load()) {
        rhi::IRenderTarget* target = m_hdrRenderTarget;
        DebugDrawRenderer::Render(scene.camera, target, m_width, m_height);
    }
    
    // Apply post-processing if enabled
    if (m_postProcessor) {
        PostProcessor::Settings settings;  // Default settings - can be customized via GetPostProcessor()
        math::Mat4 invView = glm::inverse(scene.camera.view);
        math::Mat4 invProj = glm::inverse(scene.camera.projection);
        rhi::ITexture* skyboxTex = scene.skybox && scene.skybox->IsValid() ? scene.skybox->GetTextureRHI() : nullptr;
        float time = core::Time::TotalTime();  // Get time from engine time system
        rhi::ITexture* hdrTex = GetHDRColorTextureRHI();
        rhi::ITexture* finalTexture = m_postProcessor->ApplyPostProcessing(
            hdrTex, *m_gbuffer, settings,
            scene.camera.view, scene.camera.projection,
            invView, invProj, scene.camera.viewPos, skyboxTex, time);
        // finalTexture is the post-processed output (can be used for final blit to screen)
        (void)finalTexture;  // For now, just apply post-processing
    }
    
    // Store current frame matrices for next frame's velocity calculation
    m_prevView = scene.camera.view;
    m_prevProjection = scene.camera.projection;

    if (cmd) cmd->End();
}

void RenderPipeline::BlitToScreen() {
    if (!m_device) return;
    if (m_postProcessor && m_postProcessor->GetOutputRenderTargetRHI() && m_postProcessor->HasValidOutputFrame()) {
        m_device->BlitToDefaultFramebuffer(m_postProcessor->GetOutputRenderTargetRHI());
        return;
    }
    if (m_hdrRenderTarget)
        m_device->BlitToDefaultFramebuffer(m_hdrRenderTarget);
}

bool RenderPipeline::HasValidOutput() const {
    return m_hdrRenderTarget != nullptr && m_gbuffer && m_gbuffer->GetNativeFramebuffer();
}

void RenderPipeline::EnsureSkyboxShader() {
    if (m_skyboxShader && m_skyboxShader->IsValid()) return;
    m_skyboxShader = std::make_shared<Shader>();
    m_skyboxShader->LoadFromFile("assets/shaders/passes/sky/skybox.vert", "assets/shaders/passes/sky/skybox.frag");
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
        m_device->BlitDepthOnlyFromNativeFramebuffer(m_gbuffer->GetNativeFramebuffer(), m_hdrRenderTarget);
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

void RenderPipeline::RenderSkybox(const RenderScene& scene, bool useGbufferDepth) {
    RenderSkyboxImpl(scene.camera, scene.skybox, useGbufferDepth);
}

void RenderPipeline::RenderSkybox(const api::RenderScene& scene, bool useGbufferDepth) {
    std::shared_ptr<TextureCube> internalSkybox = scene.skybox ? scene.skybox->GetInternal() : nullptr;
    CameraView cam;
    cam.view = scene.camera.view;
    cam.projection = scene.camera.projection;
    cam.viewPos = scene.camera.viewPos;
    cam.nearPlane = scene.camera.nearPlane;
    cam.farPlane = scene.camera.farPlane;
    RenderSkyboxImpl(cam, internalSkybox, useGbufferDepth);
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
