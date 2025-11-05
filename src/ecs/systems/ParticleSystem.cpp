#include "ParticleSystem.h"
#include "../Scene.h"
#include "../Components.h"
#include "../../particles/ParticleRenderer.h"
#include "../../particles/ParticleEmitter.h"
#include "../../rendering/Camera.h"
#include "../../core/log/Log.h"
#include <memory>

using namespace OpenEye::Core;

namespace OpenEye {
namespace ECS {

bool ParticleSystem::s_rendererInitialized = false;
std::unique_ptr<OpenEye::Particles::ParticleRenderer> ParticleSystem::s_renderer = nullptr;

void ParticleSystem::Update(Scene& scene, float deltaTime) {
    auto& registry = scene.GetRegistry();
    
    // Update all particle emitters
    auto view = registry.view<Transform, ParticleEmitterComponent>();
    
    for (auto entity : view) {
        auto& emitterComp = view.get<ParticleEmitterComponent>(entity);
        auto& transform = view.get<Transform>(entity);
        
        if (!emitterComp.emitter) {
            continue;
        }
        
        // Update emitter position from transform
        emitterComp.emitter->SetPosition(transform.position);
        
        // Update emitter
        emitterComp.emitter->Update(deltaTime);
    }
}

void ParticleSystem::Render(Scene& scene, const Rendering::Camera& camera,
                           const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix,
                           GLuint sceneDepthTexture,
                           std::uint32_t viewportWidth,
                           std::uint32_t viewportHeight) {
    if (!s_rendererInitialized) {
        InitializeRenderer();
    }
    
    if (!s_renderer) {
        return;
    }
    
    auto& registry = scene.GetRegistry();
    
    // Collect all emitters
    std::vector<std::shared_ptr<OpenEye::Particles::ParticleEmitter>> emitters;
    auto view = registry.view<ParticleEmitterComponent>();
    
    for (auto entity : view) {
        auto& emitterComp = view.get<ParticleEmitterComponent>(entity);
        if (emitterComp.emitter) {
            emitters.push_back(emitterComp.emitter);
        }
    }
    
    if (emitters.empty()) {
        return;
    }
    
    // Render all particles
    s_renderer->Render(emitters, camera, viewMatrix, projectionMatrix, sceneDepthTexture, viewportWidth, viewportHeight);
}

void ParticleSystem::InitializeRenderer() {
    if (s_rendererInitialized) {
        return;
    }
    
    s_renderer = std::make_unique<OpenEye::Particles::ParticleRenderer>();
    if (!s_renderer->Initialize()) {
        Core::Log::Error("ParticleSystem: Failed to initialize particle renderer");
        s_renderer.reset();
        return;
    }
    
    s_rendererInitialized = true;
    Core::Log::Info("ParticleSystem: Renderer initialized");
}

} // namespace ECS
} // namespace OpenEye

