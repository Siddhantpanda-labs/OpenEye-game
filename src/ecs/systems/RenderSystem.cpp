#include "RenderSystem.h"
#include "../Scene.h"
#include "../Components.h"
#include "CullingSystem.h"
#include "LODSystem.h"
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
#include <glad/glad.h>

namespace OpenEye {
namespace ECS {

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
                                     const Rendering::Frustum& frustum,
                                     bool enableCulling,
                                     bool enableLOD,
                                     bool enableInstancing,
                                     size_t minInstancesForBatch) {
    auto& registry = scene.GetRegistry();
    
    // Optional LOD update
    if (enableLOD) {
        LODSystem::Update(scene, camera.GetPosition());
    }

    // Build visible list
    std::vector<entt::entity> visibleEntities;
    if (enableCulling) {
        CullingSystem::CullEntities(scene, frustum);
        visibleEntities = CullingSystem::GetVisibleEntities(registry);
        if (visibleEntities.empty()) {
            // Fallback to all renderables if culling produced none (debug)
            auto allRenderableView = registry.view<Transform, Renderable>();
            for (auto entity : allRenderableView) {
                const auto& renderable = allRenderableView.get<Renderable>(entity);
                if (renderable.visible && renderable.mesh && renderable.material) {
                    visibleEntities.push_back(entity);
                }
            }
        }
    } else {
        auto allRenderableView = registry.view<Transform, Renderable>();
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
    
    // Render entities (instanced or individual)
    if (enableInstancing) {
        // Group by mesh/material
        struct BatchKey { void* meshPtr; void* materialPtr; bool operator==(const BatchKey& o) const { return meshPtr==o.meshPtr && materialPtr==o.materialPtr; } };
        struct BatchKeyHash { size_t operator()(const BatchKey& k) const { return std::hash<void*>()(k.meshPtr) ^ (std::hash<void*>()(k.materialPtr)<<1); } };
        std::unordered_map<BatchKey, std::vector<glm::mat4>, BatchKeyHash> batches;
        for (auto entity : visibleEntities) {
            const auto& transform = registry.get<Transform>(entity);
            const auto& renderable = registry.get<Renderable>(entity);
            if (!renderable.mesh || !renderable.material) continue;
            BatchKey key{renderable.mesh.get(), renderable.material.get()};
            batches[key].push_back(transform.GetWorldMatrix(registry));
        }
        gbufferShader->SetBool("u_IsInstanced", true);
        for (const auto& [key, models] : batches) {
            auto meshPtr = static_cast<Rendering::Mesh*>(key.meshPtr);
            auto materialPtr = static_cast<Rendering::Material*>(key.materialPtr);
            if (!meshPtr || !materialPtr) continue;
            // Set material once
            gbufferShader->SetVec3("u_Albedo", materialPtr->GetAlbedo());
            gbufferShader->SetFloat("u_Metallic", materialPtr->GetMetallic());
            gbufferShader->SetFloat("u_Roughness", materialPtr->GetRoughness());
            gbufferShader->SetFloat("u_AO", materialPtr->GetAO());
            
            int texUnit = 10;
            auto albedoTex = materialPtr->GetTexture("u_AlbedoTexture");
            if (albedoTex) { albedoTex->Bind(texUnit); gbufferShader->SetInt("u_AlbedoTexture", texUnit); gbufferShader->SetBool("u_HasAlbedoTexture", true); texUnit++; }
            else { gbufferShader->SetBool("u_HasAlbedoTexture", false); }
            
            auto normalTex = materialPtr->GetTexture("u_NormalTexture");
            if (normalTex) { normalTex->Bind(texUnit); gbufferShader->SetInt("u_NormalTexture", texUnit); gbufferShader->SetBool("u_HasNormalTexture", true); texUnit++; }
            else { gbufferShader->SetBool("u_HasNormalTexture", false); }
            
            auto metallicRoughTex = materialPtr->GetTexture("u_MetallicRoughnessTexture");
            if (metallicRoughTex) { metallicRoughTex->Bind(texUnit); gbufferShader->SetInt("u_MetallicRoughnessTexture", texUnit); gbufferShader->SetBool("u_HasMetallicRoughnessTexture", true); texUnit++; }
            else {
                gbufferShader->SetBool("u_HasMetallicRoughnessTexture", false);
                auto metallicTex = materialPtr->GetTexture("u_MetallicTexture");
                if (metallicTex) { metallicTex->Bind(texUnit); gbufferShader->SetInt("u_MetallicTexture", texUnit); gbufferShader->SetBool("u_HasMetallicTexture", true); texUnit++; }
                else { gbufferShader->SetBool("u_HasMetallicTexture", false); }
                
                auto roughnessTex = materialPtr->GetTexture("u_RoughnessTexture");
                if (roughnessTex) { roughnessTex->Bind(texUnit); gbufferShader->SetInt("u_RoughnessTexture", texUnit); gbufferShader->SetBool("u_HasRoughnessTexture", true); texUnit++; }
                else { gbufferShader->SetBool("u_HasRoughnessTexture", false); }
            }
            
            auto aoTex = materialPtr->GetTexture("u_AOTexture");
            if (aoTex) { aoTex->Bind(texUnit); gbufferShader->SetInt("u_AOTexture", texUnit); gbufferShader->SetBool("u_HasAOTexture", true); texUnit++; }
            else { gbufferShader->SetBool("u_HasAOTexture", false); }
            
            // Create instance buffer
            GLuint instanceVBO = 0; glGenBuffers(1, &instanceVBO);
            glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
            glBufferData(GL_ARRAY_BUFFER, models.size()*sizeof(glm::mat4), models.data(), GL_DYNAMIC_DRAW);
            
            // Setup instance attributes on mesh VAO
            meshPtr->Bind();
            GLuint vao = meshPtr->GetVAO();
            glBindVertexArray(vao);
            glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
            // locations 5..8 as mat4
            std::size_t baseOffset = 0;
            glEnableVertexAttribArray(5);
            glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), reinterpret_cast<void*>(baseOffset + 0*sizeof(glm::vec4)));
            glVertexAttribDivisor(5, 1);
            glEnableVertexAttribArray(6);
            glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), reinterpret_cast<void*>(baseOffset + 1*sizeof(glm::vec4)));
            glVertexAttribDivisor(6, 1);
            glEnableVertexAttribArray(7);
            glVertexAttribPointer(7, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), reinterpret_cast<void*>(baseOffset + 2*sizeof(glm::vec4)));
            glVertexAttribDivisor(7, 1);
            glEnableVertexAttribArray(8);
            glVertexAttribPointer(8, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), reinterpret_cast<void*>(baseOffset + 3*sizeof(glm::vec4)));
            glVertexAttribDivisor(8, 1);
            
            // Draw instanced
            GLsizei instanceCount = static_cast<GLsizei>(models.size());
            GLsizei indexCount = static_cast<GLsizei>(meshPtr->GetIndexCount());
            if (indexCount > 0) {
                glDrawElementsInstanced(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0, instanceCount);
            } else {
                GLsizei vertexCount = static_cast<GLsizei>(meshPtr->GetVertexCount());
                glDrawArraysInstanced(GL_TRIANGLES, 0, vertexCount, instanceCount);
            }
            
            // Disable instance attributes and cleanup
            glDisableVertexAttribArray(5);
            glDisableVertexAttribArray(6);
            glDisableVertexAttribArray(7);
            glDisableVertexAttribArray(8);
            meshPtr->Unbind();
            glDeleteBuffers(1, &instanceVBO);
        }
    } else {
        gbufferShader->SetBool("u_IsInstanced", false);
        for (auto entity : visibleEntities) {
            if (!registry.all_of<Transform, Renderable>(entity)) continue;
            const auto& transform = registry.get<Transform>(entity);
            const auto& renderable = registry.get<Renderable>(entity);
            if (!renderable.mesh || !renderable.material) continue;

            glm::mat4 modelMatrix = transform.GetWorldMatrix(registry);
            gbufferShader->SetMat4("u_Model", modelMatrix);
            gbufferShader->SetVec3("u_Albedo", renderable.material->GetAlbedo());
            gbufferShader->SetFloat("u_Metallic", renderable.material->GetMetallic());
            gbufferShader->SetFloat("u_Roughness", renderable.material->GetRoughness());
            gbufferShader->SetFloat("u_AO", renderable.material->GetAO());
            
            // Bind PBR textures (texture unit 10+)
            int texUnit = 10;
            auto albedoTex = renderable.material->GetTexture("u_AlbedoTexture");
            if (albedoTex) { albedoTex->Bind(texUnit); gbufferShader->SetInt("u_AlbedoTexture", texUnit); gbufferShader->SetBool("u_HasAlbedoTexture", true); texUnit++; }
            else { gbufferShader->SetBool("u_HasAlbedoTexture", false); }
            
            auto normalTex = renderable.material->GetTexture("u_NormalTexture");
            if (normalTex) { normalTex->Bind(texUnit); gbufferShader->SetInt("u_NormalTexture", texUnit); gbufferShader->SetBool("u_HasNormalTexture", true); texUnit++; }
            else { gbufferShader->SetBool("u_HasNormalTexture", false); }
            
            auto metallicRoughTex = renderable.material->GetTexture("u_MetallicRoughnessTexture");
            if (metallicRoughTex) { metallicRoughTex->Bind(texUnit); gbufferShader->SetInt("u_MetallicRoughnessTexture", texUnit); gbufferShader->SetBool("u_HasMetallicRoughnessTexture", true); texUnit++; }
            else {
                gbufferShader->SetBool("u_HasMetallicRoughnessTexture", false);
                auto metallicTex = renderable.material->GetTexture("u_MetallicTexture");
                if (metallicTex) { metallicTex->Bind(texUnit); gbufferShader->SetInt("u_MetallicTexture", texUnit); gbufferShader->SetBool("u_HasMetallicTexture", true); texUnit++; }
                else { gbufferShader->SetBool("u_HasMetallicTexture", false); }
                
                auto roughnessTex = renderable.material->GetTexture("u_RoughnessTexture");
                if (roughnessTex) { roughnessTex->Bind(texUnit); gbufferShader->SetInt("u_RoughnessTexture", texUnit); gbufferShader->SetBool("u_HasRoughnessTexture", true); texUnit++; }
                else { gbufferShader->SetBool("u_HasRoughnessTexture", false); }
            }
            
            auto aoTex = renderable.material->GetTexture("u_AOTexture");
            if (aoTex) { aoTex->Bind(texUnit); gbufferShader->SetInt("u_AOTexture", texUnit); gbufferShader->SetBool("u_HasAOTexture", true); texUnit++; }
            else { gbufferShader->SetBool("u_HasAOTexture", false); }
            renderable.mesh->Bind();
            renderable.mesh->Draw();
            renderable.mesh->Unbind();
        }
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
                                     Rendering::SkyboxRenderer* skyboxRenderer,
                                     Rendering::IBL* ibl) {
    auto& registry = scene.GetRegistry();
    if (!lightingShader) {
        OE_ERROR("RenderLightingPass: Invalid lighting shader");
        return;
    }
    
    // IMPORTANT: Assume framebuffer is already bound (for HDR rendering or screen)
    // If caller wants screen output, they should bind framebuffer 0 before calling
    // For HDR post-processing, the HDR FBO should be bound
    glViewport(0, 0, gbuffer.GetWidth(), gbuffer.GetHeight());
    
    // Clear is handled by caller (if needed)
    
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
    
    // Bind IBL textures if available
    if (ibl && ibl->IsValid()) {
        ibl->BindForLighting(11, 12, 13);
        lightingShader->SetInt("u_IrradianceMap", 11);
        lightingShader->SetInt("u_PrefilterMap", 12);
        lightingShader->SetInt("u_BRDFLUT", 13);
        lightingShader->SetBool("u_UseIBL", true);
    } else {
        lightingShader->SetBool("u_UseIBL", false);
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

