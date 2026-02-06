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
#include "eyelib/core/Time.h"
#include <algorithm>
#include <glad/glad.h>
#include <glm/gtc/matrix_inverse.hpp>
#include <span>
#include <string>

namespace eyelib {
namespace render {

RenderPipeline::RenderPipeline() = default;

RenderPipeline::~RenderPipeline() {
    Shutdown();
}

void RenderPipeline::Shutdown() {
    DestroyHDRFramebuffer();
    if (m_pointLightSSBO) { glDeleteBuffers(1, &m_pointLightSSBO); m_pointLightSSBO = 0; }
    if (m_spotLightSSBO) { glDeleteBuffers(1, &m_spotLightSSBO); m_spotLightSSBO = 0; }
    if (m_lightIndexSSBO) { glDeleteBuffers(1, &m_lightIndexSSBO); m_lightIndexSSBO = 0; }
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
    m_width = m_height = 0;
}

bool RenderPipeline::Initialize(uint32_t width, uint32_t height) {
    m_width = width;
    m_height = height;
    m_gbuffer = std::make_unique<GBuffer>();
    if (!m_gbuffer->Initialize(width, height)) return false;
    m_shadowMap = std::make_unique<ShadowMap>();
    if (!m_shadowMap->Initialize(2048)) return false;
    m_gbufferPass = std::make_unique<GBufferPass>();
    m_shadowPass = std::make_unique<ShadowPass>();
    m_lightingPass = std::make_unique<LightingPass>();
    m_frameGraph = std::make_unique<FrameGraph>();
    m_frameGraph->SetFrameSize(width, height);
    m_clusteredLightGrid = std::make_unique<ClusteredLightGrid>();
    m_clusteredLightGrid->Initialize(width, height, 0.1f, 1000.0f);  // Default, will be updated per-frame
    m_postProcessor = std::make_unique<PostProcessor>();
    if (!m_postProcessor->Initialize(width, height)) return false;
    CreateHDRFramebuffer();
    
    // Initialize previous matrices (identity for first frame)
    m_prevView = math::Mat4(1.0f);
    m_prevProjection = math::Mat4(1.0f);
    
    return m_hdrFBO != 0;
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
    glViewport(0, 0, width, height);
}

void RenderPipeline::SetFrameSize(unsigned w, unsigned h) {
    Resize(static_cast<uint32_t>(w), static_cast<uint32_t>(h));
}

void RenderPipeline::EnsureResources(uint32_t width, uint32_t height) {
    if (!m_gbuffer) Initialize(width, height);
    else Resize(width, height);
}

void RenderPipeline::CreateHDRFramebuffer() {
    if (m_width == 0 || m_height == 0) return;
    DestroyHDRFramebuffer();

    glGenFramebuffers(1, &m_hdrFBO);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, m_hdrFBO);

    glGenTextures(1, &m_hdrColor);
    glBindTexture(GL_TEXTURE_2D, m_hdrColor);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, m_width, m_height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_hdrColor, 0);

    glGenTextures(1, &m_hdrDepth);
    glBindTexture(GL_TEXTURE_2D, m_hdrDepth);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, m_width, m_height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_hdrDepth, 0);

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderPipeline::DestroyHDRFramebuffer() {
    if (m_hdrFBO) { glDeleteFramebuffers(1, &m_hdrFBO); m_hdrFBO = 0; }
    if (m_hdrColor) { glDeleteTextures(1, &m_hdrColor); m_hdrColor = 0; }
    if (m_hdrDepth) { glDeleteTextures(1, &m_hdrDepth); m_hdrDepth = 0; }
}

void RenderPipeline::BuildAndExecuteFrameGraph(const RenderScene& scene) {
    if (!m_frameGraph || !m_gbuffer || !m_shadowMap || !m_gbufferPass || !m_shadowPass || !m_lightingPass) return;
    m_frameGraph->Clear();
    m_frameGraph->SetFrameSize(m_width, m_height);

    const auto& cam = scene.camera;
    const auto& light = scene.light;

    auto hdrColor = m_frameGraph->ImportTexture("HDR", m_hdrColor, m_width, m_height);
    auto hdrDepth = m_frameGraph->ImportTexture("HDRDepth", m_hdrDepth, m_width, m_height);
    auto gbPos = m_frameGraph->ImportTexture("GBufferPos", m_gbuffer->GetPositionTexture(), m_width, m_height);
    auto gbNorm = m_frameGraph->ImportTexture("GBufferNorm", m_gbuffer->GetNormalTexture(), m_width, m_height);
    auto gbAlb = m_frameGraph->ImportTexture("GBufferAlb", m_gbuffer->GetAlbedoTexture(), m_width, m_height);
    auto gbPBR = m_frameGraph->ImportTexture("GBufferPBR", m_gbuffer->GetPBRParamsTexture(), m_width, m_height);
    auto gbVelocity = m_frameGraph->ImportTexture("GBufferVelocity", m_gbuffer->GetVelocityTexture(), m_width, m_height);
    auto gbDepth = m_frameGraph->ImportTexture("GBufferDepth", m_gbuffer->GetDepthTexture(), m_width, m_height);
    auto shadowTex = m_frameGraph->ImportTexture("Shadow", m_shadowMap->GetDepthTexture(), m_shadowMap->GetResolution(), m_shadowMap->GetResolution());

    if (light.castShadows) {
        m_frameGraph->AddPass("Shadow",
            [this, &scene, &light, &cam](const FGPassResources&) {
                m_shadowPass->Render(*m_shadowMap, scene.opaque, light.direction,
                    cam.view, cam.projection, cam.viewPos, cam.nearPlane, cam.farPlane);
            },
            {}, { shadowTex }, true);
    }

    m_frameGraph->AddPass("GBuffer",
        [this, &scene, &cam](const FGPassResources&) {
            m_gbufferPass->Render(*m_gbuffer, scene.opaque, cam.view, cam.projection, m_prevView, m_prevProjection);
        },
        {}, { gbPos, gbNorm, gbAlb, gbPBR, gbVelocity, gbDepth }, true);

    std::vector<FGTextureHandle> lightingReads = { gbPos, gbNorm, gbAlb, gbPBR };
    if (light.castShadows) lightingReads.push_back(shadowTex);
    m_frameGraph->AddPass("Lighting",
        [this, &scene, &cam](const FGPassResources&) {
            glBindFramebuffer(GL_FRAMEBUFFER, m_hdrFBO);
            glViewport(0, 0, m_width, m_height);
            glClearColor(0, 0, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            ShadowMap* shadowMapForLighting = scene.light.castShadows ? m_shadowMap.get() : nullptr;
            m_lightingPass->Render(*m_gbuffer, scene.light, cam.view, cam.projection, cam.viewPos, shadowMapForLighting,
                &scene.pointLights, &scene.spotLights);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        },
        lightingReads, { hdrColor }, true);

    m_frameGraph->AddPass("Transparency",
        [this, &scene, &cam](const FGPassResources&) {
            glBindFramebuffer(GL_READ_FRAMEBUFFER, m_gbuffer->GetFBO());
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_hdrFBO);
            glBlitFramebuffer(0, 0, static_cast<GLint>(m_gbuffer->GetWidth()), static_cast<GLint>(m_gbuffer->GetHeight()),
                              0, 0, static_cast<GLint>(m_width), static_cast<GLint>(m_height), GL_DEPTH_BUFFER_BIT, GL_NEAREST);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
            glBindFramebuffer(GL_FRAMEBUFFER, m_hdrFBO);
            glViewport(0, 0, m_width, m_height);
            TransparencyPass::Render(scene.transparent, cam.view, cam.projection, cam.viewPos);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        },
        {}, { hdrColor }, true);

    m_frameGraph->Compile();
    m_frameGraph->Execute();
}

void RenderPipeline::BuildAndExecuteFrameGraph(const api::RenderScene& scene) {
    if (!m_frameGraph || !m_gbuffer || !m_shadowMap || !m_gbufferPass || !m_shadowPass || !m_lightingPass) return;
    m_frameGraph->Clear();
    m_frameGraph->SetFrameSize(m_width, m_height);

    const auto& cam = scene.camera;
    const auto& light = scene.light;

    auto hdrColor = m_frameGraph->ImportTexture("HDR", m_hdrColor, m_width, m_height);
    auto hdrDepth = m_frameGraph->ImportTexture("HDRDepth", m_hdrDepth, m_width, m_height);
    auto gbPos = m_frameGraph->ImportTexture("GBufferPos", m_gbuffer->GetPositionTexture(), m_width, m_height);
    auto gbNorm = m_frameGraph->ImportTexture("GBufferNorm", m_gbuffer->GetNormalTexture(), m_width, m_height);
    auto gbAlb = m_frameGraph->ImportTexture("GBufferAlb", m_gbuffer->GetAlbedoTexture(), m_width, m_height);
    auto gbPBR = m_frameGraph->ImportTexture("GBufferPBR", m_gbuffer->GetPBRParamsTexture(), m_width, m_height);
    auto gbVelocity = m_frameGraph->ImportTexture("GBufferVelocity", m_gbuffer->GetVelocityTexture(), m_width, m_height);
    auto gbDepth = m_frameGraph->ImportTexture("GBufferDepth", m_gbuffer->GetDepthTexture(), m_width, m_height);
    auto shadowTex = m_frameGraph->ImportTexture("Shadow", m_shadowMap->GetDepthTexture(), m_shadowMap->GetResolution(), m_shadowMap->GetResolution());

    if (light.castShadows) {
        m_frameGraph->AddPass("Shadow",
            [this, &scene, &light, &cam](const FGPassResources&) {
                m_shadowPass->Render(*m_shadowMap, std::span<const api::Drawable>(scene.opaque.data(), scene.opaque.size()), light.direction,
                    cam.view, cam.projection, cam.viewPos, cam.nearPlane, cam.farPlane);
            },
            {}, { shadowTex }, true);
    }

    m_frameGraph->AddPass("GBuffer",
        [this, &scene, &cam](const FGPassResources&) {
            m_gbufferPass->Render(*m_gbuffer, std::span<const api::Drawable>(scene.opaque.data(), scene.opaque.size()), cam.view, cam.projection, m_prevView, m_prevProjection);
        },
        {}, { gbPos, gbNorm, gbAlb, gbPBR, gbVelocity, gbDepth }, true);

    std::vector<FGTextureHandle> lightingReads = { gbPos, gbNorm, gbAlb, gbPBR };
    if (light.castShadows) lightingReads.push_back(shadowTex);
    m_frameGraph->AddPass("Lighting",
        [this, &scene, &cam](const FGPassResources&) {
            glBindFramebuffer(GL_FRAMEBUFFER, m_hdrFBO);
            glViewport(0, 0, m_width, m_height);
            glClearColor(0, 0, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            ShadowMap* shadowMapForLighting = scene.light.castShadows ? m_shadowMap.get() : nullptr;
            m_lightingPass->Render(*m_gbuffer, scene.light, cam.view, cam.projection, cam.viewPos, shadowMapForLighting,
                std::span<const api::render::PointLight>(scene.pointLights.data(), scene.pointLights.size()),
                std::span<const api::render::SpotLight>(scene.spotLights.data(), scene.spotLights.size()));
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        },
        lightingReads, { hdrColor }, true);

    m_frameGraph->AddPass("Transparency",
        [this, &scene, &cam](const FGPassResources&) {
            glBindFramebuffer(GL_READ_FRAMEBUFFER, m_gbuffer->GetFBO());
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_hdrFBO);
            glBlitFramebuffer(0, 0, static_cast<GLint>(m_gbuffer->GetWidth()), static_cast<GLint>(m_gbuffer->GetHeight()),
                              0, 0, static_cast<GLint>(m_width), static_cast<GLint>(m_height), GL_DEPTH_BUFFER_BIT, GL_NEAREST);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
            glBindFramebuffer(GL_FRAMEBUFFER, m_hdrFBO);
            glViewport(0, 0, m_width, m_height);
            TransparencyPass::Render(std::span<const api::Drawable>(scene.transparent.data(), scene.transparent.size()), cam.view, cam.projection, cam.viewPos);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        },
        {}, { hdrColor }, true);

    m_frameGraph->Compile();
    m_frameGraph->Execute();
}

void RenderPipeline::RenderForward(const RenderScene& scene) {
    m_frameGraph->Clear();
    m_frameGraph->SetFrameSize(m_width, m_height);
    glBindFramebuffer(GL_FRAMEBUFFER, m_hdrFBO);
    glViewport(0, 0, m_width, m_height);
    glClearColor(0.1f, 0.1f, 0.15f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE); // Disable culling to render both front and back faces

    const int kShadowMapUnit = 5;
    if (m_shadowMap) {
        glActiveTexture(GL_TEXTURE0 + kShadowMapUnit);
        glBindTexture(GL_TEXTURE_2D, m_shadowMap->GetDepthTexture());
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

    if (m_shadowMap) {
        glActiveTexture(GL_TEXTURE0 + kShadowMapUnit);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    TransparencyPass::Render(scene.transparent, scene.camera.view, scene.camera.projection, scene.camera.viewPos);
    glEnable(GL_CULL_FACE); // Re-enable culling for other passes
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderPipeline::RenderForward(const api::RenderScene& scene) {
    m_frameGraph->Clear();
    m_frameGraph->SetFrameSize(m_width, m_height);
    glBindFramebuffer(GL_FRAMEBUFFER, m_hdrFBO);
    glViewport(0, 0, m_width, m_height);
    glClearColor(0.1f, 0.1f, 0.15f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    const int kShadowMapUnit = 5;
    if (m_shadowMap) {
        glActiveTexture(GL_TEXTURE0 + kShadowMapUnit);
        glBindTexture(GL_TEXTURE_2D, m_shadowMap->GetDepthTexture());
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
            sh->SetMat4Array("u_Bones", apiD.bones, apiD.boneCount);
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

    if (m_shadowMap) {
        glActiveTexture(GL_TEXTURE0 + kShadowMapUnit);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    TransparencyPass::Render(std::span<const api::Drawable>(scene.transparent.data(), scene.transparent.size()), scene.camera.view, scene.camera.projection, scene.camera.viewPos);
    glEnable(GL_CULL_FACE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
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

    // Upload lights to SSBOs (std140 layout)
    if (!scene.pointLights.empty()) {
        if (m_pointLightSSBO == 0) {
            glGenBuffers(1, &m_pointLightSSBO);
        }
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_pointLightSSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER, static_cast<GLsizeiptr>(scene.pointLights.size() * sizeof(PointLight)),
                     scene.pointLights.data(), GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, m_pointLightSSBO);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }
    if (!scene.spotLights.empty()) {
        if (m_spotLightSSBO == 0) {
            glGenBuffers(1, &m_spotLightSSBO);
        }
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_spotLightSSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER, static_cast<GLsizeiptr>(scene.spotLights.size() * sizeof(SpotLight)),
                     scene.spotLights.data(), GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, m_spotLightSSBO);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }

    // Upload clustered light grid
    m_clusteredLightGrid->UploadToGPU(m_lightIndexSSBO, m_lightIndexSSBO);

    if (scene.light.castShadows && m_shadowMap && m_shadowPass) {
        m_shadowPass->Render(*m_shadowMap, scene.opaque, scene.light.direction,
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

    glBindFramebuffer(GL_FRAMEBUFFER, m_hdrFBO);
    glViewport(0, 0, m_width, m_height);
    glClearColor(0.1f, 0.1f, 0.15f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE); // Match Forward: render both sides so orientation matches

    const int kShadowMapUnit = 5;
    if (scene.light.castShadows && m_shadowMap) {
        glActiveTexture(GL_TEXTURE0 + kShadowMapUnit);
        glBindTexture(GL_TEXTURE_2D, m_shadowMap->GetDepthTexture());
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
        if (useSkinned) sh->SetMat4Array("u_Bones", bonePtr, boneCount);

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
    glEnable(GL_CULL_FACE);
    TransparencyPass::Render(scene.transparent, scene.camera.view, scene.camera.projection, scene.camera.viewPos);
    if (scene.light.castShadows && m_shadowMap) {
        glActiveTexture(GL_TEXTURE0 + kShadowMapUnit);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
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

    if (!m_scratchPointLights.empty()) {
        if (m_pointLightSSBO == 0) glGenBuffers(1, &m_pointLightSSBO);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_pointLightSSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER, static_cast<GLsizeiptr>(m_scratchPointLights.size() * sizeof(PointLight)),
                     m_scratchPointLights.data(), GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, m_pointLightSSBO);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }
    if (!m_scratchSpotLights.empty()) {
        if (m_spotLightSSBO == 0) glGenBuffers(1, &m_spotLightSSBO);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_spotLightSSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER, static_cast<GLsizeiptr>(m_scratchSpotLights.size() * sizeof(SpotLight)),
                     m_scratchSpotLights.data(), GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, m_spotLightSSBO);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }

    m_clusteredLightGrid->UploadToGPU(m_lightIndexSSBO, m_lightIndexSSBO);

    if (scene.light.castShadows && m_shadowMap && m_shadowPass) {
        m_shadowPass->Render(*m_shadowMap, std::span<const api::Drawable>(scene.opaque.data(), scene.opaque.size()), scene.light.direction,
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

    glBindFramebuffer(GL_FRAMEBUFFER, m_hdrFBO);
    glViewport(0, 0, m_width, m_height);
    glClearColor(0.1f, 0.1f, 0.15f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    const int kShadowMapUnit = 5;
    if (scene.light.castShadows && m_shadowMap) {
        glActiveTexture(GL_TEXTURE0 + kShadowMapUnit);
        glBindTexture(GL_TEXTURE_2D, m_shadowMap->GetDepthTexture());
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
        if (useSkinned) sh->SetMat4Array("u_Bones", apiD.bones, apiD.boneCount);

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
    glEnable(GL_CULL_FACE);
    TransparencyPass::Render(std::span<const api::Drawable>(scene.transparent.data(), scene.transparent.size()), scene.camera.view, scene.camera.projection, scene.camera.viewPos);
    if (scene.light.castShadows && m_shadowMap) {
        glActiveTexture(GL_TEXTURE0 + kShadowMapUnit);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderPipeline::Render(const api::RenderScene& scene) {
    if (m_width == 0 || m_height == 0) return;
    EnsureResources(m_width, m_height);
    if (m_hdrFBO == 0 || !m_gbuffer || !m_gbuffer->GetFBO()) return;

    switch (m_mode) {
    case PipelineMode::Deferred:
        BuildAndExecuteFrameGraph(scene);
        break;
    case PipelineMode::Forward:
        if (m_shadowMap && m_shadowPass) {
            m_shadowPass->Render(*m_shadowMap, scene.opaque, scene.light.direction,
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
        DebugDrawRenderer::Render(cam, m_hdrFBO, m_width, m_height);
    }

    if (m_postProcessor) {
        PostProcessor::Settings settings;
        math::Mat4 invView = glm::inverse(scene.camera.view);
        math::Mat4 invProj = glm::inverse(scene.camera.projection);
        unsigned int skyboxTex = scene.skybox && scene.skybox->IsValid() ? scene.skybox->GetInternal()->Id() : 0;
        float time = core::Time::TotalTime();
        unsigned int velocityTex = m_gbuffer->GetVelocityTexture();
        (void)m_postProcessor->ApplyPostProcessing(
            m_hdrColor, *m_gbuffer, settings,
            scene.camera.view, scene.camera.projection,
            invView, invProj, scene.camera.viewPos, skyboxTex, time);
    }

    m_prevView = scene.camera.view;
    m_prevProjection = scene.camera.projection;
}

void RenderPipeline::Render(const RenderScene& scene) {
    if (m_width == 0 || m_height == 0) return;
    EnsureResources(m_width, m_height);
    if (m_hdrFBO == 0 || !m_gbuffer || !m_gbuffer->GetFBO()) return;

    switch (m_mode) {
    case PipelineMode::Deferred:
        BuildAndExecuteFrameGraph(scene);
        break;
    case PipelineMode::Forward:
        if (m_shadowMap && m_shadowPass) {
            m_shadowPass->Render(*m_shadowMap, scene.opaque, scene.light.direction,
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
        m_prePostCallback(m_hdrFBO, m_width, m_height, scene);
    }

    if (eyelib::debug::GetPresets().debugDrawEnabled.load()) {
        DebugDrawRenderer::Render(scene.camera, m_hdrFBO, m_width, m_height);
    }
    
    // Apply post-processing if enabled
    if (m_postProcessor) {
        PostProcessor::Settings settings;  // Default settings - can be customized via GetPostProcessor()
        math::Mat4 invView = glm::inverse(scene.camera.view);
        math::Mat4 invProj = glm::inverse(scene.camera.projection);
        unsigned int skyboxTex = scene.skybox && scene.skybox->IsValid() ? scene.skybox->Id() : 0;
        float time = core::Time::TotalTime();  // Get time from engine time system
        unsigned int velocityTex = m_gbuffer->GetVelocityTexture();
        unsigned int finalTexture = m_postProcessor->ApplyPostProcessing(
            m_hdrColor, *m_gbuffer, settings,
            scene.camera.view, scene.camera.projection,
            invView, invProj, scene.camera.viewPos, skyboxTex, time);
        // finalTexture is the post-processed output (can be used for final blit to screen)
        (void)finalTexture;  // For now, just apply post-processing
    }
    
    // Store current frame matrices for next frame's velocity calculation
    m_prevView = scene.camera.view;
    m_prevProjection = scene.camera.projection;
}

void RenderPipeline::BlitToScreen() {
    if (m_postProcessor && m_postProcessor->GetOutputTexture() != 0 && m_postProcessor->HasValidOutputFrame()) {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, m_postProcessor->GetOutputFBO());
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glBlitFramebuffer(0, 0, static_cast<GLint>(m_width), static_cast<GLint>(m_height),
                          0, 0, static_cast<GLint>(m_width), static_cast<GLint>(m_height),
                          GL_COLOR_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, static_cast<GLsizei>(m_width), static_cast<GLsizei>(m_height));
        return;
    }
    if (m_hdrFBO == 0) return;
    glBindFramebuffer(GL_READ_FRAMEBUFFER, m_hdrFBO);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBlitFramebuffer(0, 0, static_cast<GLint>(m_width), static_cast<GLint>(m_height),
                      0, 0, static_cast<GLint>(m_width), static_cast<GLint>(m_height),
                      GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, static_cast<GLsizei>(m_width), static_cast<GLsizei>(m_height));
}

bool RenderPipeline::HasValidOutput() const {
    return m_hdrFBO != 0 && m_gbuffer && m_gbuffer->GetFBO();
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

    glBindFramebuffer(GL_FRAMEBUFFER, m_hdrFBO);
    glViewport(0, 0, m_width, m_height);

    if (useGbufferDepth && m_gbuffer) {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, m_gbuffer->GetFBO());
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_hdrFBO);
        glBlitFramebuffer(0, 0, static_cast<GLint>(m_gbuffer->GetWidth()), static_cast<GLint>(m_gbuffer->GetHeight()),
                          0, 0, static_cast<GLint>(m_width), static_cast<GLint>(m_height), GL_DEPTH_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, m_hdrFBO);
    }

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);

    m_skyboxShader->Bind();
    m_skyboxShader->SetMat4("u_InverseViewMatrix", glm::inverse(camera.view));
    m_skyboxShader->SetMat4("u_InverseProjectionMatrix", glm::inverse(camera.projection));
    m_skyboxShader->SetInt("u_Skybox", 0);
    skybox->Bind(0);

    static FullscreenQuad quad;
    quad.Render();

    m_skyboxShader->Unbind();
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
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
