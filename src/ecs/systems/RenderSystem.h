#pragma once

#include "../Scene.h"
#include "../Components.h"
#include "rendering/Camera.h"
#include "rendering/Frustum.h"
#include "rendering/GBuffer.h"
#include "rendering/ShadowMap.h"
#include "rendering/SkyboxRenderer.h"
#include "rendering/IBL.h"
#include "rendering/resources/Shader.h"
#include <glm/glm.hpp>
#include <memory>

namespace OpenEye {
namespace ECS {

// Render system - renders all entities with Renderable component
class RenderSystem {
public:
    static void Update(Scene& scene, const Rendering::Camera& camera, 
                      const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix);
    
    // Render with visibility culling (uses CullingSystem results)
    static void UpdateWithCulling(Scene& scene, const Rendering::Camera& camera,
                                 const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix,
                                 const Rendering::Frustum& frustum);
    
    // Render with instancing (groups by mesh/material and batches)
    static void UpdateWithInstancing(Scene& scene, const Rendering::Camera& camera,
                                    const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix,
                                    const Rendering::Frustum& frustum,
                                    bool useInstancing = true,
                                    size_t minInstancesForBatch = 3); // Only batch if 3+ instances
    
    // Deferred rendering: geometry pass to G-Buffer
    static void RenderGeometryPass(Scene& scene, Rendering::GBuffer& gbuffer,
                                  const Rendering::Camera& camera,
                                  const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix,
                                  const Rendering::Frustum& frustum,
                                  bool enableCulling,
                                  bool enableLOD,
                                  bool enableInstancing,
                                  size_t minInstancesForBatch);
    
    // Deferred rendering: lighting pass (reads from G-Buffer)
    static void RenderLightingPass(Scene& scene,
                                  Rendering::GBuffer& gbuffer,
                                  const Rendering::Camera& camera,
                                  std::shared_ptr<Rendering::Shader> lightingShader,
                                  Rendering::ShadowMap* shadowMap = nullptr,
                                  int debugMode = 0,
                                  const glm::vec3& lightDir = glm::vec3(0.5f, 1.0f, 0.3f),
                                  const glm::vec3& lightColor = glm::vec3(1.0f, 0.95f, 0.9f),
                                  float lightIntensity = 3.0f,
                                  const glm::vec3& ambientLight = glm::vec3(0.08f),
                                  float exposure = 1.0f,
                                  Rendering::SkyboxRenderer* skyboxRenderer = nullptr,
                                  Rendering::IBL* ibl = nullptr);
    
    // Shadow pass: render depth from light's perspective
    static void RenderShadowPass(Scene& scene, Rendering::ShadowMap& shadowMap,
                                uint32_t cascadeIndex,
                                const glm::vec3& lightDirection);
};

} // namespace ECS
} // namespace OpenEye

