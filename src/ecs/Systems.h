#pragma once

#include "Scene.h"
#include "Components.h"
#include "rendering/Camera.h"
#include "rendering/Frustum.h"
#include "rendering/LODManager.h"
#include "rendering/GBuffer.h"
#include "rendering/ShadowMap.h"
#include <glm/glm.hpp>
#include <vector>
#include <memory>

namespace OpenEye {
namespace ECS {

// Transform system - updates world matrices and handles hierarchy
class TransformSystem {
public:
    static void Update(Scene& scene);
    static void MarkDirty(entt::registry& registry, entt::entity entity);
    static void MarkHierarchyDirty(entt::registry& registry, entt::entity entity);
};

// LOD system - updates LOD levels based on camera distance
class LODSystem {
public:
    // Update LOD for all entities with LOD component
    static void Update(Scene& scene, const glm::vec3& cameraPosition);
};

// Culling system - performs frustum culling on entities with AABB
class CullingSystem {
public:
    // Update AABBs from mesh bounds (call when mesh changes)
    static void UpdateAABBs(Scene& scene);
    
    // Perform frustum culling, mark entities as visible/invisible
    // Returns count of visible entities
    static size_t CullEntities(Scene& scene, const Rendering::Frustum& frustum);
    
    // Get list of visible entities (for rendering)
    static std::vector<entt::entity> GetVisibleEntities(const entt::registry& registry);
};

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
                                  const Rendering::Frustum& frustum);
    
    // Deferred rendering: lighting pass (reads from G-Buffer)
    static void RenderLightingPass(Rendering::GBuffer& gbuffer,
                                  const Rendering::Camera& camera,
                                  std::shared_ptr<Rendering::Shader> lightingShader,
                                  Rendering::ShadowMap* shadowMap = nullptr);
    
    // Shadow pass: render depth from light's perspective
    static void RenderShadowPass(Scene& scene, Rendering::ShadowMap& shadowMap,
                                uint32_t cascadeIndex,
                                const glm::vec3& lightDirection);
};

// Camera system - manages camera components and finds primary camera
class CameraSystem {
public:
    static entt::entity FindPrimaryCamera(entt::registry& registry);
    static Rendering::Camera* GetPrimaryCamera(entt::registry& registry);
};

} // namespace ECS
} // namespace OpenEye

