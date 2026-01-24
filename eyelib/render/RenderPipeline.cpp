#include "eyelib/render/RenderPipeline.h"
#include "eyelib/render/GBuffer.h"
#include "eyelib/render/ShadowMap.h"
#include "eyelib/render/ClusteredLightGrid.h"
#include "eyelib/render/PostProcessor.h"
#include "eyelib/render/passes/GBufferPass.h"
#include "eyelib/render/passes/ShadowPass.h"
#include "eyelib/render/passes/LightingPass.h"
#include "eyelib/render/passes/TransparencyPass.h"
#include "eyelib/render/framegraph/FrameGraph.h"
#include "eyelib/render/resources/Material.h"
#include "eyelib/render/resources/Mesh.h"
#include "eyelib/render/resources/Shader.h"
#include "eyelib/core/Time.h"
#include <glad/glad.h>
#include <glm/gtc/matrix_inverse.hpp>

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

    m_frameGraph->AddPass("Shadow",
        [this, &scene, &light, &cam](const FGPassResources&) {
            m_shadowPass->Render(*m_shadowMap, scene.opaque, light.direction,
                cam.view, cam.projection, cam.viewPos, cam.nearPlane, cam.farPlane);
        },
        {}, { shadowTex }, true);

    m_frameGraph->AddPass("GBuffer",
        [this, &scene, &cam](const FGPassResources&) {
            m_gbufferPass->Render(*m_gbuffer, scene.opaque, cam.view, cam.projection, m_prevView, m_prevProjection);
        },
        {}, { gbPos, gbNorm, gbAlb, gbPBR, gbVelocity, gbDepth }, true);

    m_frameGraph->AddPass("Lighting",
        [this, &scene, &cam](const FGPassResources&) {
            glBindFramebuffer(GL_FRAMEBUFFER, m_hdrFBO);
            glViewport(0, 0, m_width, m_height);
            glClearColor(0, 0, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            m_lightingPass->Render(*m_gbuffer, scene.light, cam.view, cam.projection, cam.viewPos);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        },
        { gbPos, gbNorm, gbAlb, gbPBR }, { hdrColor }, true);

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

void RenderPipeline::RenderForward(const RenderScene& scene) {
    m_frameGraph->Clear();
    m_frameGraph->SetFrameSize(m_width, m_height);
    glBindFramebuffer(GL_FRAMEBUFFER, m_hdrFBO);
    glViewport(0, 0, m_width, m_height);
    glClearColor(0.1f, 0.1f, 0.15f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    for (const auto& d : scene.opaque) {
        if (!d.mesh || !d.material) continue;
        d.material->Bind();
        Shader* sh = d.material->GetShader().get();
        if (sh) {
            sh->SetMat4("u_Model", d.transform);
            sh->SetMat4("u_View", scene.camera.view);
            sh->SetMat4("u_Projection", scene.camera.projection);
            sh->SetVec3("u_ViewPos", scene.camera.viewPos);
            d.mesh->Bind();
            d.mesh->Draw();
            d.mesh->Unbind();
            sh->Unbind();
        }
    }
    TransparencyPass::Render(scene.transparent, scene.camera.view, scene.camera.projection, scene.camera.viewPos);
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

    // Ensure forward+ shader exists
    if (!m_forwardPlusShader || !m_forwardPlusShader->IsValid()) {
        m_forwardPlusShader = std::make_shared<Shader>();
        m_forwardPlusShader->LoadFromFile("assets/shaders/passes/forward/forwardplus.vert", "assets/shaders/passes/forward/forwardplus.frag");
    }

    if (!m_forwardPlusShader->IsValid()) {
        RenderForward(scene);
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, m_hdrFBO);
    glViewport(0, 0, m_width, m_height);
    glClearColor(0.1f, 0.1f, 0.15f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

    m_forwardPlusShader->Bind();
    m_forwardPlusShader->SetMat4("u_View", scene.camera.view);
    m_forwardPlusShader->SetMat4("u_Projection", scene.camera.projection);
    m_forwardPlusShader->SetVec3("u_ViewPos", scene.camera.viewPos);
    m_forwardPlusShader->SetVec3("u_DirLightDir", scene.light.direction);
    m_forwardPlusShader->SetVec3("u_DirLightColor", scene.light.color);
    m_forwardPlusShader->SetFloat("u_DirLightIntensity", scene.light.intensity);
    m_forwardPlusShader->SetVec3("u_Ambient", scene.light.ambient);
    m_forwardPlusShader->SetFloat("u_Exposure", scene.light.exposure);
    m_forwardPlusShader->SetInt("u_ScreenWidth", static_cast<int>(m_width));
    m_forwardPlusShader->SetInt("u_ScreenHeight", static_cast<int>(m_height));
    m_forwardPlusShader->SetInt("u_TileCountX", static_cast<int>(m_clusteredLightGrid->GetTileCountX()));
    m_forwardPlusShader->SetInt("u_TileCountY", static_cast<int>(m_clusteredLightGrid->GetTileCountY()));
    m_forwardPlusShader->SetInt("u_SliceCount", static_cast<int>(m_clusteredLightGrid->GetSliceCount()));
    m_forwardPlusShader->SetFloat("u_LogNear", m_clusteredLightGrid->GetLogNear());
    m_forwardPlusShader->SetFloat("u_LogFar", m_clusteredLightGrid->GetLogFar());
    m_forwardPlusShader->SetFloat("u_NearPlane", m_clusteredLightGrid->GetNearPlane());
    m_forwardPlusShader->SetFloat("u_FarPlane", m_clusteredLightGrid->GetFarPlane());
    m_forwardPlusShader->SetInt("u_PointLightCount", static_cast<int>(scene.pointLights.size()));
    m_forwardPlusShader->SetInt("u_SpotLightCount", static_cast<int>(scene.spotLights.size()));

    for (const auto& d : scene.opaque) {
        if (!d.mesh || !d.material) continue;
        m_forwardPlusShader->SetMat4("u_Model", d.transform);
        m_forwardPlusShader->SetVec3("u_Albedo", d.material->GetAlbedo());
        m_forwardPlusShader->SetFloat("u_Metallic", d.material->GetMetallic());
        m_forwardPlusShader->SetFloat("u_Roughness", d.material->GetRoughness());
        d.mesh->Bind();
        d.mesh->Draw();
        d.mesh->Unbind();
    }

    m_forwardPlusShader->Unbind();
    TransparencyPass::Render(scene.transparent, scene.camera.view, scene.camera.projection, scene.camera.viewPos);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderPipeline::Render(const RenderScene& scene) {
    EnsureResources(m_width, m_height);
    if (m_hdrFBO == 0 || !m_gbuffer || !m_gbuffer->GetFBO()) return;

    switch (m_mode) {
    case PipelineMode::Deferred:
        BuildAndExecuteFrameGraph(scene);
        break;
    case PipelineMode::Forward:
        RenderForward(scene);
        break;
    case PipelineMode::ForwardPlus:
        RenderForwardPlus(scene);
        break;
    }
    
    // Apply post-processing if enabled
    if (m_postProcessor) {
        PostProcessor::Settings settings;  // Default settings - can be customized via GetPostProcessor()
        math::Mat4 invView = glm::inverse(scene.camera.view);
        math::Mat4 invProj = glm::inverse(scene.camera.projection);
        unsigned int skyboxTex = 0;  // TODO: Get from scene or RenderPipeline
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

GBuffer* RenderPipeline::GetGBuffer() { return m_gbuffer.get(); }
ShadowMap* RenderPipeline::GetShadowMap() { return m_shadowMap.get(); }
PostProcessor* RenderPipeline::GetPostProcessor() { return m_postProcessor.get(); }

const std::vector<FrameGraph::PassInfo>& RenderPipeline::GetFrameGraphPasses() const {
    static const std::vector<FrameGraph::PassInfo> empty;
    return m_frameGraph ? m_frameGraph->GetPasses() : empty;
}

} // namespace render
} // namespace eyelib
