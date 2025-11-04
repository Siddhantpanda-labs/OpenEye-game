#pragma once

#include "../Scene.h"
#include "../Components.h"
#include "../../rendering/Camera.h"
#include <memory>

// Forward declaration
namespace OpenEye {
namespace Particles {
    class ParticleRenderer;
}
}

namespace OpenEye {
namespace ECS {

/**
 * @brief Particle system - updates and renders particle emitters.
 */
class ParticleSystem {
public:
    /**
     * @brief Update all particle emitters in the scene.
     * @param scene ECS scene
     * @param deltaTime Time since last frame
     */
    static void Update(Scene& scene, float deltaTime);
    
    /**
     * @brief Render all particle emitters.
     * @param scene ECS scene
     * @param camera Camera for rendering
     * @param viewMatrix View matrix
     * @param projectionMatrix Projection matrix
     */
    static void Render(Scene& scene, const Rendering::Camera& camera,
                      const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix);

private:
    static bool s_rendererInitialized;
    static std::unique_ptr<OpenEye::Particles::ParticleRenderer> s_renderer;
    static void InitializeRenderer();
};

} // namespace ECS
} // namespace OpenEye

