#pragma once

#include "../Scene.h"
#include "../Components.h"

// Forward declarations
namespace OpenEye {
namespace Physics {
    class PhysicsWorld;
}
namespace Rendering {
    class Mesh;
}
}
class btSoftBody;

namespace OpenEye {
namespace ECS {

class SoftBodySystem {
public:
    static void Update(Scene& scene, Physics::PhysicsWorld* physicsWorld, float deltaTime);
    static void SyncMesh(Scene& scene);
    static void RenderWireframes(Scene& scene, const Rendering::Camera& camera, const glm::mat4& view, const glm::mat4& proj);
    
private:
    static void ApplyWind(btSoftBody* softBody, const glm::vec3& windDir, float windStrength, float deltaTime);
    static void CreateWireframeBox();
    static void RenderWireframeBox(const glm::mat4& view, const glm::mat4& proj, const glm::mat4& model, const glm::vec3& size);
    
    // Static wireframe resources (shared across all soft bodies)
    static GLuint s_wireframeVAO;
    static GLuint s_wireframeVBO;
    static std::shared_ptr<Rendering::Shader> s_wireframeShader;
    static bool s_wireframeInitialized;
};

} // namespace ECS
} // namespace OpenEye
