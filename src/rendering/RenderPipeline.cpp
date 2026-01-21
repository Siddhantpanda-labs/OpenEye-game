#include "RenderPipeline.h"
#include "GBuffer.h"
#include "ShadowMap.h"
#include "FullscreenQuad.h"
#include "SkyboxRenderer.h"
#include "IBL.h"
#include "PostProcessor.h"
#include "passes/ShadowPass.h"
#include "passes/GBufferPass.h"
#include "passes/LightingPass.h"
#include "passes/TransparencyPass.h"
#include "resources/Shader.h"
#include "resources/Cubemap.h"
#include "framegraph/FrameGraph.h"
#include "../ecs/Scene.h"
#include "../ecs/Systems.h"
#include "../ecs/systems/SoftBodySystem.h"
#include "Camera.h"
#include "core/log/Log.h"
#include <glad/glad.h>

namespace OpenEye {
namespace Rendering {

RenderPipeline::RenderPipeline() = default;

RenderPipeline::~RenderPipeline() {
    DestroyHDRFramebuffer();
    if (m_frameGraph) delete m_frameGraph;
}

bool RenderPipeline::Initialize(uint32_t width, uint32_t height) {
    m_width = width; m_height = height;
    m_gbuffer = std::make_unique<GBuffer>();
    if (!m_gbuffer->Initialize(width, height)) return false;
    m_shadowMap = std::make_unique<ShadowMap>();
    m_shadowMap->Initialize(2048, 4);
    m_lightingShader = std::make_shared<Shader>();
    m_lightingShader->LoadFromFile("assets/shaders/passes/deferred/lighting.vert", "assets/shaders/passes/deferred/lighting.frag");
    m_skyboxRenderer = std::make_unique<SkyboxRenderer>();
    m_skyboxRenderer->Initialize(width, height);
    m_skybox = std::make_unique<Cubemap>();
    m_ibl = std::make_unique<IBL>();
    m_postProcessor = std::make_unique<PostProcessor>();
    if (!m_postProcessor->Initialize(width, height)) {
        OE_WARN("RenderPipeline: Failed to initialize PostProcessor");
    }
    // Create UBOs (std140 slabs)
    m_frameUBO.Create(1024, 0); // 1KB safe slab for std140 alignment
    m_lightUBO.Create(256, 1);
    CreateHDRFramebuffer();
    m_frameGraph = new FrameGraph();
    return true;
}

void RenderPipeline::Resize(uint32_t width, uint32_t height) {
    if (width == 0 || height == 0) {
        OE_WARN_F("RenderPipeline::Resize: Ignoring invalid size %ux%u", width, height);
        return; // Ignore invalid sizes
    }
    if (width == m_width && height == m_height) {
        return; // No change needed
    }
    
    OE_INFO_F("RenderPipeline::Resize: %ux%u -> %ux%u", m_width, m_height, width, height);
    
    // Update dimensions first
    m_width = width; 
    m_height = height;
    
    // Resize all components
    if (m_gbuffer) {
        m_gbuffer->Resize(width, height);
    }
    if (m_skyboxRenderer) {
        m_skyboxRenderer->Resize(width, height);
    }
    if (m_postProcessor) {
        m_postProcessor->Resize(width, height);
    }
    
    // Recreate HDR framebuffer with new size
    // IMPORTANT: This must only be called when NOT rendering (deferred in GameApplication)
    DestroyHDRFramebuffer();
    CreateHDRFramebuffer();
    
    // Verify HDR framebuffer was created successfully
    if (m_hdrFBO == 0) {
        OE_ERROR("RenderPipeline::Resize: Failed to create HDR framebuffer!");
    }
    
    // Ensure viewport is set correctly
    glViewport(0, 0, width, height);
}

void RenderPipeline::EnsureResources(uint32_t width, uint32_t height) {
    if (!m_gbuffer) Initialize(width, height);
    Resize(width, height);
}

void RenderPipeline::RenderFrame(ECS::Scene& scene, const Camera& camera, const LightingParams& params, const RenderOptions& options) {
    // Ensure resources are initialized
    EnsureResources(m_width, m_height);
    
    // Safety check: Ensure HDR framebuffer is valid before rendering
    if (m_hdrFBO == 0) {
        OE_ERROR("RenderPipeline::RenderFrame: HDR framebuffer not initialized! Attempting to recreate...");
        CreateHDRFramebuffer();
        if (m_hdrFBO == 0) {
            OE_ERROR("RenderPipeline::RenderFrame: Failed to create HDR framebuffer! Aborting render.");
            return;
        }
    }

    // Update UBOs (Frame + Light) first – used by all passes
    struct FrameBlock {
        glm::mat4 view;
        glm::mat4 proj;
        glm::mat4 invView;
        glm::mat4 invProj;
        glm::vec4 viewPos_time;
        float splits[5]; // MAX_CASCADES+1
    } frame;
    frame.view = camera.GetViewMatrix();
    frame.proj = camera.GetProjectionMatrix();
    frame.invView = glm::inverse(frame.view);
    frame.invProj = glm::inverse(frame.proj);
    frame.viewPos_time = glm::vec4(camera.GetPosition(), 0.0f);
    for (int i = 0; i < 5; ++i) frame.splits[i] = 0.0f;
    if (m_shadowMap) {
        const auto& splits = m_shadowMap->GetSplitDistances();
        for (size_t i = 0; i < splits.size() && i < 5; ++i) frame.splits[i] = splits[i];
    }
    m_frameUBO.Update(&frame, sizeof(FrameBlock));
    m_frameUBO.Bind();

    struct LightBlock {
        glm::vec4 dir_intensity;
        glm::vec4 color_exposure;
        glm::vec4 ambient;
    } light;
    light.dir_intensity = glm::vec4(glm::normalize(params.lightDir), params.lightIntensity);
    light.color_exposure = glm::vec4(params.lightColor, params.exposure);
    light.ambient = glm::vec4(params.ambient, 0.0f);
    m_lightUBO.Update(&light, sizeof(LightBlock));
    m_lightUBO.Bind();

    BuildFrameGraph(scene, camera, params, options);
    m_frameGraph->compile();
    m_frameGraph->execute();
}

void RenderPipeline::BuildFrameGraph(ECS::Scene& scene, const Camera& camera, const LightingParams& params, const RenderOptions& options) {
    m_frameGraph->clear();
    m_frameGraph->setFrameSize(m_width, m_height);

    // Shadow pass
    m_frameGraph->addPass("Shadow", [this, &scene, &camera, params](const FGPassResources&) {
        glm::vec3 lightDir = glm::normalize(params.lightDir);
        m_shadowMap->UpdateMatrices(lightDir, camera.GetViewMatrix(), camera.GetProjectionMatrix(), camera.GetPosition(), 0.1f, 100.0f);
        ShadowPass sp;
        for (uint32_t i = 0; i < m_shadowMap->GetCascadeCount(); ++i) sp.Execute(scene, *m_shadowMap, i, lightDir);
    }, {}, {}, true);

    // GBuffer pass
    m_frameGraph->addPass("GBuffer", [this, &scene, &camera, options](const FGPassResources&) {
        // Ensure viewport is set correctly
        glViewport(0, 0, m_width, m_height);
        Frustum frustum; frustum.ExtractFromMatrix(camera.GetProjectionMatrix() * camera.GetViewMatrix());
        GBufferPass gp; gp.Execute(scene, *m_gbuffer, camera, camera.GetViewMatrix(), camera.GetProjectionMatrix(), frustum, options.enableCulling, options.enableLOD, options.enableInstancing, static_cast<size_t>(options.minInstancesForBatch));
    }, {}, {}, true);

    // Skybox pass (render solid sky color)
    m_frameGraph->addPass("Skybox", [this](const FGPassResources&) {
        // Ensure viewport is set correctly
        glViewport(0, 0, m_width, m_height);
        if (m_skyboxRenderer) m_skyboxRenderer->RenderSolidColor(m_skyColor);
    }, {}, {}, true);

    // Lighting pass - render to HDR texture
    m_frameGraph->addPass("Lighting", [this, &scene, &camera, params](const FGPassResources&) {
        // Validate HDR framebuffer exists
        if (m_hdrFBO == 0) {
            OE_ERROR("RenderPipeline: HDR framebuffer not initialized!");
            return;
        }
        
        // Bind HDR framebuffer
        glBindFramebuffer(GL_FRAMEBUFFER, m_hdrFBO);
        glViewport(0, 0, m_width, m_height);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        // Copy depth from GBuffer to HDR framebuffer for proper occlusion
        glBindFramebuffer(GL_READ_FRAMEBUFFER, m_gbuffer->GetFBO());
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_hdrFBO);
        glBlitFramebuffer(0, 0, m_width, m_height, 0, 0, m_width, m_height, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER, m_hdrFBO);
        
        LightingPass lp; lp.Execute(scene, *m_gbuffer, camera, m_lightingShader, m_shadowMap.get(), params,
            m_skyboxRenderer.get(), // Always pass skyboxRenderer (it has solid color even without cubemap)
            m_ibl && m_ibl->IsValid() ? m_ibl.get() : nullptr);
    }, {}, {}, true);
    
    // Transparency pass - render transparent objects on top of deferred lighting
    m_frameGraph->addPass("Transparency", [this, &scene, &camera](const FGPassResources&) {
        // Keep HDR framebuffer bound
        glBindFramebuffer(GL_FRAMEBUFFER, m_hdrFBO);
        glViewport(0, 0, m_width, m_height);
        
        Frustum frustum;
        frustum.ExtractFromMatrix(camera.GetProjectionMatrix() * camera.GetViewMatrix());
        TransparencyPass tp;
        tp.Execute(scene, camera, frustum);
        // Render GPU particles (flipbook + soft particles) after other transparent objects
        ECS::ParticleSystem::Render(scene,
                                    camera,
                                    camera.GetViewMatrix(),
                                    camera.GetProjectionMatrix(),
                                    m_gbuffer->GetDepthTexture(),
                                    m_width,
                                    m_height);
        
        // Render fluid volumes (smoke, liquid)
        ECS::FluidSystem::Render(scene, camera, camera.GetViewMatrix(), camera.GetProjectionMatrix());
        
        // Render soft body wireframe boxes (debug visualization)
        ECS::SoftBodySystem::RenderWireframes(scene, camera, camera.GetViewMatrix(), camera.GetProjectionMatrix());
    }, {}, {}, true);
    
    // Post-processing pass - apply effects and blit to screen
    m_frameGraph->addPass("PostProcessing", [this, params, options](const FGPassResources&) {
        PostProcessor::Settings ppSettings;
        ppSettings.exposure = params.exposure;
        ppSettings.useACES = true;
        ppSettings.enableBloom = options.enableBloom;
        ppSettings.enableBlur = options.enableBlur;
        ppSettings.enableChromaticAberration = options.enableChromaticAberration;
        ppSettings.enableFXAA = options.enableFXAA;
        ppSettings.blurIntensity = options.blurIntensity;
        ppSettings.blurIterations = options.blurIterations;
        ppSettings.chromaticAberrationIntensity = options.chromaticAberrationIntensity;
        ppSettings.enableSSAO = options.enableSSAO;
        ppSettings.ssaoRadius = options.ssaoRadius;
        ppSettings.ssaoBias = options.ssaoBias;
        ppSettings.ssaoSamples = options.ssaoSamples;
        ppSettings.enableSSR = options.enableSSR;
        ppSettings.ssrStepSize = options.ssrStepSize;
        ppSettings.ssrMaxSteps = options.ssrMaxSteps;
        ppSettings.ssrThickness = options.ssrThickness;
        ppSettings.enableMotionBlur = options.enableMotionBlur;
        ppSettings.motionBlurIntensity = options.motionBlurIntensity;
        ppSettings.motionBlurSamples = options.motionBlurSamples;
        ppSettings.enableVignette = options.enableVignette;
        ppSettings.vignetteIntensity = options.vignetteIntensity;
        ppSettings.vignetteRadius = options.vignetteRadius;
        ppSettings.vignetteSoftness = options.vignetteSoftness;
        ppSettings.enableFilmGrain = options.enableFilmGrain;
        ppSettings.filmGrainIntensity = options.filmGrainIntensity;
        
        GLuint finalTexture = m_postProcessor->ApplyPostProcessing(m_hdrTexture, *m_gbuffer, ppSettings);
        
        // Render final post-processed texture to screen
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, m_width, m_height);
        glDisable(GL_DEPTH_TEST);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        
        // Use simple copy shader to render final texture to screen
        static std::shared_ptr<Shader> copyShader = nullptr;
        if (!copyShader) {
            copyShader = std::make_shared<Shader>();
            copyShader->LoadFromFile("assets/shaders/passes/post/copy.vert", "assets/shaders/passes/post/copy.frag");
        }
        
        if (copyShader && copyShader->IsValid() && finalTexture != 0) {
            copyShader->Bind();
            copyShader->SetInt("u_Texture", 0);
            
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, finalTexture);
            
            static FullscreenQuad quad;
            quad.Render();
            
            copyShader->Unbind();
        }
        
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }, {}, {}, true);
}

void RenderPipeline::CreateHDRFramebuffer() {
    if (m_width == 0 || m_height == 0) {
        OE_ERROR("RenderPipeline: Cannot create HDR framebuffer with zero dimensions");
        return;
    }
    
    glGenFramebuffers(1, &m_hdrFBO);
    glGenTextures(1, &m_hdrTexture);
    glGenTextures(1, &m_hdrDepthTexture);
    
    glBindFramebuffer(GL_FRAMEBUFFER, m_hdrFBO);
    
    // Color texture
    glBindTexture(GL_TEXTURE_2D, m_hdrTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, m_width, m_height, 0, GL_RGB, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_hdrTexture, 0);
    
    // Depth texture for proper occlusion
    glBindTexture(GL_TEXTURE_2D, m_hdrDepthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, m_width, m_height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_hdrDepthTexture, 0);
    
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        OE_ERROR_F("RenderPipeline: HDR FBO incomplete: 0x%x (size: %ux%u)", status, m_width, m_height);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return;
    }
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderPipeline::DestroyHDRFramebuffer() {
    if (m_hdrFBO) {
        glDeleteFramebuffers(1, &m_hdrFBO);
        m_hdrFBO = 0;
    }
    if (m_hdrTexture) {
        glDeleteTextures(1, &m_hdrTexture);
        m_hdrTexture = 0;
    }
    if (m_hdrDepthTexture) {
        glDeleteTextures(1, &m_hdrDepthTexture);
        m_hdrDepthTexture = 0;
    }
}

const std::vector<FrameGraph::PassInfo>& RenderPipeline::GetFrameGraphPasses() const {
    if (!m_frameGraph) {
        static std::vector<FrameGraph::PassInfo> empty;
        return empty;
    }
    return m_frameGraph->getPasses();
}

} // namespace Rendering
} // namespace OpenEye


