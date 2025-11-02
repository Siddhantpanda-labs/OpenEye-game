#include "Systems.h"
#include "Scene.h"
#include "Components.h"
#include "core/log/Log.h"
#include "rendering/Frustum.h"
#include "rendering/LODManager.h"
#include "rendering/InstancedRenderer.h"
#include "rendering/GBuffer.h"
#include "rendering/FullscreenQuad.h"
#include "rendering/ShadowMap.h"
#include "rendering/resources/Shader.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <vector>
#include <cmath>
#include <unordered_map>
#include <memory>

namespace OpenEye {
namespace ECS {

void TransformSystem::Update(Scene& scene) {
    auto& registry = scene.GetRegistry();
    
    // Update all transforms (GetWorldMatrix handles dirty flag and caching)
    auto view = registry.view<Transform>();
    for (auto entity : view) {
        auto& transform = view.get<Transform>(entity);
        transform.GetWorldMatrix(registry);
    }
}

void TransformSystem::MarkDirty(entt::registry& registry, entt::entity entity) {
    if (!registry.valid(entity) || !registry.all_of<Transform>(entity)) {
        return;
    }
    
    auto& transform = registry.get<Transform>(entity);
    transform.dirty = true;
    
    // Mark all children as dirty too
    MarkHierarchyDirty(registry, entity);
}

void TransformSystem::MarkHierarchyDirty(entt::registry& registry, entt::entity entity) {
    // Find all children and mark them dirty
    auto view = registry.view<Transform>();
    for (auto child : view) {
        const auto& childTransform = view.get<Transform>(child);
        if (childTransform.parent == entity) {
            auto& mutableTransform = registry.get<Transform>(child);
            mutableTransform.dirty = true;
            MarkHierarchyDirty(registry, child);
        }
    }
}

void RenderSystem::Update(Scene& scene, const Rendering::Camera& camera,
                         const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix) {
    auto& registry = scene.GetRegistry();
    
    auto view = registry.view<Transform, Renderable>();
    
    // Set shared lighting uniforms once (optimization)
    // Note: This assumes all entities use similar shaders with same uniforms
    // In Phase 4, we'll handle this in deferred rendering
    
    for (auto entity : view) {
        auto [transform, renderable] = view.get<Transform, Renderable>(entity);
        
        if (!renderable.visible || !renderable.mesh || !renderable.material) {
            continue;
        }
        
        // Get world matrix (handles hierarchy and caching)
        glm::mat4 modelMatrix = transform.GetWorldMatrix(registry);
        
        // Bind material and set matrices
        renderable.material->Bind();
        auto shader = renderable.material->GetShader();
        
        shader->SetMat4("u_Model", modelMatrix);
        shader->SetMat4("u_View", viewMatrix);
        shader->SetMat4("u_Projection", projectionMatrix);
        shader->SetVec3("u_ViewPos", camera.GetPosition());
        
        // Set lighting uniforms (if not already set by material)
        shader->SetVec3("u_LightDir", glm::normalize(glm::vec3(1.0f, 1.0f, 1.0f)));
        shader->SetVec3("u_LightColor", glm::vec3(1.0f, 1.0f, 1.0f));
        
        // Render mesh
        renderable.mesh->Bind();
        renderable.mesh->Draw();
        renderable.mesh->Unbind();
        
        shader->Unbind();
    }
}

entt::entity CameraSystem::FindPrimaryCamera(entt::registry& registry) {
    auto view = registry.view<CameraComponent>();
    
    for (auto entity : view) {
        const auto& cameraComp = view.get<CameraComponent>(entity);
        if (cameraComp.isPrimary) {
            return entity;
        }
    }
    
    // Return first camera if no primary found
    if (!view.empty()) {
        return *view.begin();
    }
    
    return entt::null;
}

Rendering::Camera* CameraSystem::GetPrimaryCamera(entt::registry& registry) {
    entt::entity cameraEntity = FindPrimaryCamera(registry);
    if (cameraEntity != entt::null && registry.valid(cameraEntity)) {
        if (registry.all_of<CameraComponent>(cameraEntity)) {
            return &registry.get<CameraComponent>(cameraEntity).camera;
        }
    }
    return nullptr;
}

void CullingSystem::UpdateAABBs(Scene& scene) {
    auto& registry = scene.GetRegistry();
    auto view = registry.view<Transform, Renderable, AABB>();
    
    for (auto entity : view) {
        auto [transform, renderable, aabb] = view.get<Transform, Renderable, AABB>(entity);
        
        if (!renderable.mesh || renderable.mesh->GetVertexCount() == 0) {
            continue;
        }
        
        // Get mesh bounds in local space
        glm::vec3 localMin, localMax;
        renderable.mesh->CalculateBounds(localMin, localMax);
        
        // Transform to world space using transform
        glm::mat4 worldMatrix = transform.GetWorldMatrix(registry);
        
        // Transform AABB corners to world space
        glm::vec3 corners[8] = {
            glm::vec3(localMin.x, localMin.y, localMin.z),
            glm::vec3(localMax.x, localMin.y, localMin.z),
            glm::vec3(localMin.x, localMax.y, localMin.z),
            glm::vec3(localMax.x, localMax.y, localMin.z),
            glm::vec3(localMin.x, localMin.y, localMax.z),
            glm::vec3(localMax.x, localMin.y, localMax.z),
            glm::vec3(localMin.x, localMax.y, localMax.z),
            glm::vec3(localMax.x, localMax.y, localMax.z)
        };
        
        glm::vec3 worldMin = glm::vec3(worldMatrix * glm::vec4(corners[0], 1.0f));
        glm::vec3 worldMax = worldMin;
        
        for (int i = 1; i < 8; i++) {
            glm::vec3 worldCorner = glm::vec3(worldMatrix * glm::vec4(corners[i], 1.0f));
            worldMin = glm::min(worldMin, worldCorner);
            worldMax = glm::max(worldMax, worldCorner);
        }
        
        aabb.min = worldMin;
        aabb.max = worldMax;
    }
}

size_t CullingSystem::CullEntities(Scene& scene, const Rendering::Frustum& frustum) {
    auto& registry = scene.GetRegistry();
    auto view = registry.view<Transform, AABB, Renderable>();
    
    size_t visibleCount = 0;
    
    for (auto entity : view) {
        auto [transform, aabb, renderable] = view.get<Transform, AABB, Renderable>(entity);
        
        // Skip if already marked invisible by renderable component
        if (!renderable.visible) {
            aabb.isVisible = false;
            continue;
        }
        
        // Test AABB against frustum
        int result = frustum.TestAABB(aabb.min, aabb.max);
        aabb.isVisible = (result > 0); // 1 = intersect, 2 = inside
        
        if (aabb.isVisible) {
            visibleCount++;
        }
    }
    
    return visibleCount;
}

std::vector<entt::entity> CullingSystem::GetVisibleEntities(const entt::registry& registry) {
    std::vector<entt::entity> visible;
    auto view = registry.view<AABB, Renderable>();
    
    for (auto entity : view) {
        const auto [aabb, renderable] = view.get<AABB, Renderable>(entity);
        if (aabb.isVisible && renderable.visible && renderable.mesh && renderable.material) {
            visible.push_back(entity);
        }
    }
    
    return visible;
}

void RenderSystem::UpdateWithCulling(Scene& scene, const Rendering::Camera& camera,
                                   const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix,
                                   const Rendering::Frustum& frustum) {
    auto& registry = scene.GetRegistry();
    
    // Perform culling first
    CullingSystem::CullEntities(scene, frustum);
    
    // Get visible entities
    auto visibleEntities = CullingSystem::GetVisibleEntities(registry);
    
    // Render only visible entities
    for (auto entity : visibleEntities) {
        if (!registry.all_of<Transform, Renderable>(entity)) {
            continue;
        }
        
        const auto& transform = registry.get<Transform>(entity);
        const auto& renderable = registry.get<Renderable>(entity);
        
        // Get world matrix
        glm::mat4 modelMatrix = transform.GetWorldMatrix(registry);
        
        // Bind material and set matrices
        renderable.material->Bind();
        auto shader = renderable.material->GetShader();
        
        shader->SetMat4("u_Model", modelMatrix);
        shader->SetMat4("u_View", viewMatrix);
        shader->SetMat4("u_Projection", projectionMatrix);
        shader->SetVec3("u_ViewPos", camera.GetPosition());
        
        // Set lighting uniforms
        shader->SetVec3("u_LightDir", glm::normalize(glm::vec3(1.0f, 1.0f, 1.0f)));
        shader->SetVec3("u_LightColor", glm::vec3(1.0f, 1.0f, 1.0f));
        
        // Render mesh
        renderable.mesh->Bind();
        renderable.mesh->Draw();
        renderable.mesh->Unbind();
        
        shader->Unbind();
    }
}

void LODSystem::Update(Scene& scene, const glm::vec3& cameraPosition) {
    auto& registry = scene.GetRegistry();
    auto view = registry.view<Transform, LOD, Renderable>();
    
    for (auto entity : view) {
        auto [transform, lod, renderable] = view.get<Transform, LOD, Renderable>(entity);
        
        // Get entity world position
        glm::mat4 worldMatrix = transform.GetWorldMatrix(registry);
        glm::vec3 worldPos = glm::vec3(worldMatrix[3]); // Extract position from matrix
        
        // Calculate distance
        float distance = Rendering::LODManager::CalculateDistance(cameraPosition, worldPos);
        
        // Update LOD
        size_t maxLOD = renderable.lodMeshes.empty() ? 0 : renderable.lodMeshes.size();
        size_t newLOD = Rendering::LODManager::UpdateLOD(lod.settings, distance, maxLOD);
        
        // Update active LOD
        renderable.activeLOD = newLOD;
        
        // Update mesh reference if LOD meshes are available
        if (!renderable.lodMeshes.empty() && newLOD < renderable.lodMeshes.size()) {
            renderable.mesh = renderable.lodMeshes[newLOD];
        }
    }
}

void RenderSystem::UpdateWithInstancing(Scene& scene, const Rendering::Camera& camera,
                                       const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix,
                                       const Rendering::Frustum& frustum,
                                       bool useInstancing,
                                       size_t minInstancesForBatch) {
    auto& registry = scene.GetRegistry();
    
    // Perform culling first
    CullingSystem::CullEntities(scene, frustum);
    auto visibleEntities = CullingSystem::GetVisibleEntities(registry);
    
    if (visibleEntities.empty()) {
        return;
    }
    
    if (useInstancing) {
        // Group visible entities by mesh/material for batching
        struct BatchKey {
            void* meshPtr;
            void* materialPtr;
            
            bool operator==(const BatchKey& other) const {
                return meshPtr == other.meshPtr && materialPtr == other.materialPtr;
            }
        };
        
        struct BatchKeyHash {
            size_t operator()(const BatchKey& key) const {
                return std::hash<void*>()(key.meshPtr) ^ (std::hash<void*>()(key.materialPtr) << 1);
            }
        };
        
        std::unordered_map<BatchKey, std::vector<entt::entity>, BatchKeyHash> batches;
        
        // Group entities
        for (auto entity : visibleEntities) {
            if (!registry.all_of<Transform, Renderable>(entity)) {
                continue;
            }
            
            const auto& renderable = registry.get<Renderable>(entity);
            if (!renderable.mesh || !renderable.material) {
                continue;
            }
            
            BatchKey key;
            key.meshPtr = renderable.mesh.get();
            key.materialPtr = renderable.material.get();
            batches[key].push_back(entity);
        }
        
        // Use instancing for batches with enough instances
        Rendering::InstancedRenderer instancedRenderer;
        
        size_t instancedCount = 0;
        size_t individualCount = 0;
        
        for (const auto& [key, entities] : batches) {
            if (entities.size() >= minInstancesForBatch) {
                // Batch with instancing
                for (auto entity : entities) {
                    const auto& transform = registry.get<Transform>(entity);
                    const auto& renderable = registry.get<Renderable>(entity);
                    
                    glm::mat4 modelMatrix = transform.GetWorldMatrix(registry);
                    instancedRenderer.AddInstance(renderable.mesh, renderable.material, modelMatrix);
                    instancedCount++;
                }
            } else {
                // Render individually (too few for batching)
                for (auto entity : entities) {
                    const auto& transform = registry.get<Transform>(entity);
                    const auto& renderable = registry.get<Renderable>(entity);
                    
                    glm::mat4 modelMatrix = transform.GetWorldMatrix(registry);
                    
                    renderable.material->Bind();
                    auto shader = renderable.material->GetShader();
                    
                    shader->SetMat4("u_Model", modelMatrix);
                    shader->SetMat4("u_View", viewMatrix);
                    shader->SetMat4("u_Projection", projectionMatrix);
                    shader->SetVec3("u_ViewPos", camera.GetPosition());
                    shader->SetVec3("u_LightDir", glm::normalize(glm::vec3(1.0f, 1.0f, 1.0f)));
                    shader->SetVec3("u_LightColor", glm::vec3(1.0f, 1.0f, 1.0f));
                    
                    renderable.mesh->Bind();
                    renderable.mesh->Draw();
                    renderable.mesh->Unbind();
                    
                    shader->Unbind();
                    individualCount++;
                }
            }
        }
        
        // Render all instanced batches
        if (instancedCount > 0) {
            instancedRenderer.RenderInstances(viewMatrix, projectionMatrix, camera.GetPosition());
        }
        
        // Optional: Log batching stats (can be removed in production)
        // OE_INFO_F("Rendered %zu instanced, %zu individual", instancedCount, individualCount);
    } else {
        // No instancing - render individually
        UpdateWithCulling(scene, camera, viewMatrix, projectionMatrix, frustum);
    }
}

void RenderSystem::RenderGeometryPass(Scene& scene, Rendering::GBuffer& gbuffer,
                                     const Rendering::Camera& camera,
                                     const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix,
                                     const Rendering::Frustum& frustum) {
    auto& registry = scene.GetRegistry();
    
    // Perform culling first
    CullingSystem::CullEntities(scene, frustum);
    auto visibleEntities = CullingSystem::GetVisibleEntities(registry);
    
    // Debug: If no visible entities found, try rendering all entities with Renderable (bypass culling temporarily)
    if (visibleEntities.empty()) {
        OE_WARN("No visible entities from culling - rendering all entities with Renderable component");
        // Get all entities with Renderable (bypass culling)
        auto allRenderableView = registry.view<Transform, Renderable>();
        visibleEntities.clear();
        for (auto entity : allRenderableView) {
            const auto& renderable = allRenderableView.get<Renderable>(entity);
            if (renderable.visible && renderable.mesh && renderable.material) {
                visibleEntities.push_back(entity);
            }
        }
    }
    
    // Bind G-Buffer for writing
    gbuffer.BindForGeometryPass();
    
    // Ensure depth testing is enabled
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    
    // Load geometry pass shader (GBuffer shader)
    static std::shared_ptr<Rendering::Shader> gbufferShader = nullptr;
    if (!gbufferShader) {
        gbufferShader = std::make_shared<Rendering::Shader>();
        if (!gbufferShader->LoadFromFile("assets/shaders/passes/deferred/gbuffer.vert", "assets/shaders/passes/deferred/gbuffer.frag")) {
            OE_ERROR("Failed to load G-Buffer shader");
            return;
        }
    }
    
    gbufferShader->Bind();
    
    // Debug log removed to reduce spam
    
    // Render all visible entities
    size_t renderedCount = 0;
    for (auto entity : visibleEntities) {
        if (!registry.all_of<Transform, Renderable>(entity)) {
            continue;
        }
        
        const auto& transform = registry.get<Transform>(entity);
        const auto& renderable = registry.get<Renderable>(entity);
        
        if (!renderable.mesh || !renderable.material) {
            continue;
        }
        
        // Get world matrix
        glm::mat4 modelMatrix = transform.GetWorldMatrix(registry);
        gbufferShader->SetMat4("u_Model", modelMatrix);
        
        // Set material properties
        gbufferShader->SetVec3("u_Albedo", renderable.material->GetAlbedo());
        gbufferShader->SetFloat("u_Metallic", renderable.material->GetMetallic());
        gbufferShader->SetFloat("u_Roughness", renderable.material->GetRoughness());
        
        // Bind textures if available
        auto albedoTexture = renderable.material->GetTexture("u_AlbedoTexture");
        if (albedoTexture) {
            albedoTexture->Bind(10); // Use texture unit 10 for geometry pass textures
            gbufferShader->SetInt("u_AlbedoTexture", 10);
            gbufferShader->SetBool("u_HasAlbedoTexture", true);
        } else {
            gbufferShader->SetBool("u_HasAlbedoTexture", false);
        }
        
        // Render mesh
        renderable.mesh->Bind();
        renderable.mesh->Draw();
        renderable.mesh->Unbind();
        
        renderedCount++;
    }
    
    // Debug count log removed
    
    gbufferShader->Unbind();
    gbuffer.Unbind();
}

void RenderSystem::RenderLightingPass(Scene& scene,
                                     Rendering::GBuffer& gbuffer,
                                     const Rendering::Camera& camera,
                                     std::shared_ptr<Rendering::Shader> lightingShader,
                                     Rendering::ShadowMap* shadowMap,
                                     int debugMode,
                                     const glm::vec3& paramLightDir,
                                     const glm::vec3& paramLightColor,
                                     float paramLightIntensity,
                                     const glm::vec3& paramAmbient,
                                     float paramExposure,
                                     Rendering::SkyboxRenderer* skyboxRenderer) {
    auto& registry = scene.GetRegistry();
    if (!lightingShader) {
        OE_ERROR("RenderLightingPass: Invalid lighting shader");
        return;
    }
    
    // IMPORTANT: Bind default framebuffer (0) for output to screen
    // This ensures we render the lighting result to the screen, not back to G-Buffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, gbuffer.GetWidth(), gbuffer.GetHeight());
    
    // Clear the default framebuffer
    glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // Disable depth testing for fullscreen quad (we're compositing, not depth-testing)
    glDisable(GL_DEPTH_TEST);
    
    // Bind G-Buffer textures for reading
    gbuffer.BindForLightingPass();
    
    // Bind shadow maps if provided
    if (shadowMap) {
        shadowMap->BindTextures(5); // Start at texture unit 5
    }
    
    // Bind background from skybox pass (if available)
    if (skyboxRenderer && skyboxRenderer->GetColorTexture() != 0) {
        glActiveTexture(GL_TEXTURE0 + 9);
        glBindTexture(GL_TEXTURE_2D, skyboxRenderer->GetColorTexture());
    }

    // Use lighting shader (MUST bind before setting uniforms)
    lightingShader->Bind();
    
    // Set G-Buffer texture uniforms
    lightingShader->SetInt("g_Position", 0);
    lightingShader->SetInt("g_Normal", 1);
    lightingShader->SetInt("g_Albedo", 2);
    lightingShader->SetInt("g_PBRParams", 3);
    
    // Directional light, exposure, ambient now via LightUniforms UBO
    lightingShader->SetInt("u_DebugMode", debugMode);
    if (skyboxRenderer && skyboxRenderer->GetColorTexture() != 0) {
        lightingShader->SetInt("u_Background", 9);
    }
    
    // Collect point lights from scene
    std::vector<glm::vec3> pointLightPositions;
    std::vector<glm::vec3> pointLightColors;
    std::vector<float> pointLightIntensities;
    std::vector<float> pointLightRanges;
    
    auto lightView = registry.view<Transform, Light>();
    for (auto entity : lightView) {
        auto [transform, light] = lightView.get<Transform, Light>(entity);
        
        if (light.type == Light::Type::Point) {
            glm::vec3 worldPos = transform.GetWorldMatrix(registry) * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
            pointLightPositions.push_back(worldPos);
            pointLightColors.push_back(light.color);
            pointLightIntensities.push_back(light.intensity);
            pointLightRanges.push_back(light.range);
        }
    }
    
    // Set point lights (limit to MAX_POINT_LIGHTS)
    size_t pointLightCount = std::min(pointLightPositions.size(), size_t(32));
    lightingShader->SetInt("u_PointLightCount", static_cast<int>(pointLightCount));
    
    for (size_t i = 0; i < pointLightCount; i++) {
        std::string posName = "u_PointLightPositions[" + std::to_string(i) + "]";
        std::string colorName = "u_PointLightColors[" + std::to_string(i) + "]";
        std::string intensityName = "u_PointLightIntensities[" + std::to_string(i) + "]";
        std::string rangeName = "u_PointLightRanges[" + std::to_string(i) + "]";
        
        lightingShader->SetVec3(posName, pointLightPositions[i]);
        lightingShader->SetVec3(colorName, pointLightColors[i]);
        lightingShader->SetFloat(intensityName, pointLightIntensities[i]);
        lightingShader->SetFloat(rangeName, pointLightRanges[i]);
    }
    
    // Set shadow map uniforms if shadow map is provided
    if (shadowMap) {
        uint32_t cascadeCount = shadowMap->GetCascadeCount();
        lightingShader->SetInt("u_ShadowCascadeCount", static_cast<int>(cascadeCount));
        
        for (uint32_t i = 0; i < cascadeCount; i++) {
            std::string uniformName = "u_ShadowMaps[" + std::to_string(i) + "]";
            lightingShader->SetInt(uniformName, 5 + i);
            
            uniformName = "u_ShadowMatrices[" + std::to_string(i) + "]";
            lightingShader->SetMat4(uniformName, shadowMap->GetShadowMatrix(i));
        }
        
        // Set cascade splits
        const auto& splits = shadowMap->GetSplitDistances();
        for (size_t i = 0; i < splits.size() && i < 5; i++) {
            std::string uniformName = "u_CascadeSplits[" + std::to_string(i) + "]";
            lightingShader->SetFloat(uniformName, splits[i]);
        }
    } else {
        // No shadows
        lightingShader->SetInt("u_ShadowCascadeCount", 0);
    }
    
    // Render fullscreen quad
    static Rendering::FullscreenQuad fullscreenQuad;
    fullscreenQuad.Render();
    
    // Re-enable depth testing for future passes
    glEnable(GL_DEPTH_TEST);
    
    lightingShader->Unbind();
    
    // Unbind textures (optional, but clean)
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void RenderSystem::RenderShadowPass(Scene& scene, Rendering::ShadowMap& shadowMap,
                                   uint32_t cascadeIndex,
                                   const glm::vec3& lightDirection) {
    auto& registry = scene.GetRegistry();
    
    // Bind shadow map FBO
    shadowMap.BindForShadowPass(cascadeIndex);
    
    // Load shadow shader
    static std::shared_ptr<Rendering::Shader> shadowShader = nullptr;
    if (!shadowShader) {
        shadowShader = std::make_shared<Rendering::Shader>();
        if (!shadowShader->LoadFromFile("assets/shaders/shadow.vert", "assets/shaders/shadow.frag")) {
            OE_ERROR("Failed to load shadow shader");
            shadowMap.Unbind();
            return;
        }
    }
    
    shadowShader->Bind();
    
    // Get shadow matrix for this cascade
    glm::mat4 lightSpaceMatrix = shadowMap.GetShadowMatrix(cascadeIndex);
    shadowShader->SetMat4("u_LightSpaceMatrix", lightSpaceMatrix);
    
    // Render all opaque entities (no culling for shadow pass - render everything)
    auto view = registry.view<Transform, Renderable>();
    for (auto entity : view) {
        const auto& transform = view.get<Transform>(entity);
        const auto& renderable = view.get<Renderable>(entity);
        
        if (!renderable.visible || !renderable.mesh) {
            continue;
        }
        
        glm::mat4 modelMatrix = transform.GetWorldMatrix(registry);
        shadowShader->SetMat4("u_Model", modelMatrix);
        
        renderable.mesh->Bind();
        renderable.mesh->Draw();
        renderable.mesh->Unbind();
    }
    
    shadowShader->Unbind();
    shadowMap.Unbind();
}

} // namespace ECS
} // namespace OpenEye

