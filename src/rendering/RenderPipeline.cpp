#include "RenderPipeline.h"
#include "GBuffer.h"
#include "ShadowMap.h"
#include "FullscreenQuad.h"
#include "SkyboxRenderer.h"
#include "IBL.h"
#include "passes/ShadowPass.h"
#include "passes/GBufferPass.h"
#include "passes/LightingPass.h"
#include "resources/Shader.h"
#include "resources/Cubemap.h"
#include "framegraph/FrameGraph.h"
#include "../ecs/Scene.h"
#include "../ecs/Systems.h"
#include "Camera.h"

namespace OpenEye {
namespace Rendering {

RenderPipeline::RenderPipeline() = default;

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
    // Create UBOs (std140 slabs)
    m_frameUBO.Create(1024, 0); // 1KB safe slab for std140 alignment
    m_lightUBO.Create(256, 1);
    m_frameGraph = new FrameGraph();
    return true;
}

void RenderPipeline::Resize(uint32_t width, uint32_t height) {
    if (width == m_width && height == m_height) return;
    m_width = width; m_height = height;
    if (m_gbuffer) m_gbuffer->Resize(width, height);
    if (m_skyboxRenderer) m_skyboxRenderer->Resize(width, height);
}

void RenderPipeline::EnsureResources(uint32_t width, uint32_t height) {
    if (!m_gbuffer) Initialize(width, height);
    Resize(width, height);
}

void RenderPipeline::RenderFrame(ECS::Scene& scene, const Camera& camera, const LightingParams& params, const RenderOptions& options) {
    EnsureResources(m_width, m_height);

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
        Frustum frustum; frustum.ExtractFromMatrix(camera.GetProjectionMatrix() * camera.GetViewMatrix());
        GBufferPass gp; gp.Execute(scene, *m_gbuffer, camera, camera.GetViewMatrix(), camera.GetProjectionMatrix(), frustum, options.enableCulling, options.enableLOD, options.enableInstancing, static_cast<size_t>(options.minInstancesForBatch));
    }, {}, {}, true);

    // Skybox pass
    m_frameGraph->addPass("Skybox", [this, &camera](const FGPassResources&) {
        if (m_skybox && m_skybox->IsValid()) m_skyboxRenderer->Render(camera, *m_skybox);
    }, {}, {}, true);

    // Lighting pass
    m_frameGraph->addPass("Lighting", [this, &scene, &camera, params](const FGPassResources&) {
        LightingPass lp; lp.Execute(scene, *m_gbuffer, camera, m_lightingShader, m_shadowMap.get(), params,
            m_skybox && m_skybox->IsValid() ? m_skyboxRenderer.get() : nullptr,
            m_ibl && m_ibl->IsValid() ? m_ibl.get() : nullptr);
    }, {}, {}, true);
}

const std::vector<FrameGraph::PassInfo>& RenderPipeline::GetFrameGraphPasses() const { return m_frameGraph->getPasses(); }

} // namespace Rendering
} // namespace OpenEye


