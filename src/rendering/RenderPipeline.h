#pragma once

#include <memory>
#include <glm/glm.hpp>
#include "resources/UniformBuffer.h"
#include "framegraph/FrameGraph.h"

namespace OpenEye {
namespace ECS { class Scene; }
namespace Rendering { class Camera; class GBuffer; class ShadowMap; class Shader; class Cubemap; class SkyboxRenderer; class IBL; }
}

namespace OpenEye {
namespace Rendering {

struct LightingParams {
    glm::vec3 lightDir = glm::vec3(0.5f, 1.0f, 0.3f);
    glm::vec3 lightColor = glm::vec3(1.0f, 0.95f, 0.9f);
    float lightIntensity = 3.0f;
    glm::vec3 ambient = glm::vec3(0.08f);
    float exposure = 1.0f;
    int debugMode = 0; // 0 lighting, 1 albedo, 2 normal, 3 position, 4 pbr
};

struct RenderOptions {
    bool enableCulling = true;
    bool enableLOD = true;
    bool enableInstancing = false;
    int  minInstancesForBatch = 3;
};

class RenderPipeline {
public:
    RenderPipeline();
    ~RenderPipeline() = default;

    bool Initialize(uint32_t width, uint32_t height);
    void Resize(uint32_t width, uint32_t height);

    // Execute full frame: Shadow → GBuffer → Skybox → Lighting
    void RenderFrame(ECS::Scene& scene, const Camera& camera, const LightingParams& params, const RenderOptions& options);
    const std::vector<FrameGraph::PassInfo>& GetFrameGraphPasses() const;

    // Expose skybox for loading
    SkyboxRenderer* GetSkyboxRenderer() { return m_skyboxRenderer.get(); }
    Cubemap* GetSkyboxCubemap() { return m_skybox.get(); }
    GBuffer* GetGBuffer() { return m_gbuffer.get(); }
    IBL* GetIBL() { return m_ibl.get(); }

private:
    void EnsureResources(uint32_t width, uint32_t height);
    void BuildFrameGraph(ECS::Scene& scene, const Camera& camera, const LightingParams& params, const RenderOptions& options);

    std::unique_ptr<GBuffer> m_gbuffer;
    std::unique_ptr<ShadowMap> m_shadowMap;
    std::shared_ptr<Shader> m_lightingShader;
    std::unique_ptr<SkyboxRenderer> m_skyboxRenderer;
    std::unique_ptr<Cubemap> m_skybox;
    std::unique_ptr<IBL> m_ibl;
    UniformBuffer m_frameUBO; // binding 0
    UniformBuffer m_lightUBO; // binding 1

    uint32_t m_width = 0;
    uint32_t m_height = 0;

    // FrameGraph instance
    FrameGraph* m_frameGraph = nullptr;
};

} // namespace Rendering
} // namespace OpenEye


