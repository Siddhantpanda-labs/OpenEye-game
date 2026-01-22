#pragma once

#include "core/api/All.h"
#include "ecs/api/All.h"
#include "rendering/api/All.h"
#include "physics/api/All.h"  // Use API, not direct PhysicsWorld
#include <memory>
#include <string>

namespace OpenMeta {

class GameApplication {
public:
    GameApplication();
    ~GameApplication();

    bool Initialize();
    void Run(); // Full game loop (for release builds)
    void Update(float deltaTime); // Single frame update (for editor builds)
    void Render(); // Single frame render
    void Shutdown();
    
    // Get scene for editor panels
    OpenEye::ECS::Scene* GetScene() { return m_scene.get(); }
    OpenEye::Core::Window* GetWindow() { return m_window.get(); }
    OpenEye::Rendering::RenderPipeline* GetRenderPipeline() { return m_renderPipeline.get(); }

private:
    void LoadScene();
    void CreateDefaultScene();
    OpenEye::Rendering::Camera* GetMainCamera();
    void SetupInputCallbacks();
    void UpdateCameraControls(float deltaTime);
    std::shared_ptr<OpenEye::Rendering::Mesh> CreateMeshFromCloth(class btSoftBody* cloth, int widthSegments, int heightSegments);
    std::shared_ptr<OpenEye::Rendering::Mesh> CreateCylinderMesh(float radius, float height, int segments = 32);
    std::shared_ptr<OpenEye::Rendering::Mesh> CreateSphereMesh(float radius, int segments = 24, int rings = 16);
    std::shared_ptr<OpenEye::Rendering::Mesh> CreateMeshFromSoftBody(class btSoftBody* body, glm::vec3& outOrigin);

    std::unique_ptr<OpenEye::Core::Window> m_window;
    std::unique_ptr<OpenEye::ECS::Scene> m_scene;
    std::unique_ptr<OpenEye::Rendering::RenderPipeline> m_renderPipeline;
    std::unique_ptr<OpenEye::Physics::API::PhysicsWorld> m_physicsWorld;
    
    bool m_isRunning = false;
    std::string m_scenePath = "resources/scenes/default.json";
    
    // Render options
    bool m_enableCulling = true;
    bool m_enableLOD = true;
    bool m_enableInstancing = true;
    int m_minInstancesForBatch = 4;
    
    // Camera controls
    bool m_cameraLocked = false;
    float m_cameraSpeed = 5.0f;
    float m_mouseSensitivity = 0.1f;
    glm::vec2 m_lastMousePos = glm::vec2(0.0f);
    bool m_firstMouse = true;
    float m_cameraYaw = -90.0f;   // Initial yaw (looking along -Z)
    float m_cameraPitch = 0.0f;   // Initial pitch (horizontal)
    
    // Deferred resize (to avoid resizing during rendering)
    bool m_pendingResize = false;
    int m_pendingWidth = 0;
    int m_pendingHeight = 0;
};

} // namespace OpenMeta

