#include "core/window/Window.h"
#include "core/log/Log.h"
#include "core/time/Time.h"
#include "core/input/Input.h"
#include "core/filesystem/ShaderFileWatcher.h"
#include "ecs/Scene.h"
#include "ecs/Components.h"
#include "ecs/Systems.h"
#include "rendering/resources/Shader.h"
#include "rendering/resources/Mesh.h"
#include "rendering/resources/Material.h"
#include "rendering/resources/MeshLoader.h"
#include "rendering/Camera.h"
#include "rendering/Frustum.h"
#include "rendering/GBuffer.h"
#include "rendering/ShadowMap.h"
#include "rendering/resources/Shader.h"
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

using namespace OpenEye::Core;
using namespace OpenEye::Rendering;
using namespace OpenEye::ECS;

int main() {
    // Initialize logging
    Log::Initialize("editor.log");
    Log::SetLevel(LogLevel::Info);
    OE_INFO("=== OpenEye Editor Starting ===");
    
    // Initialize shader file watcher (syncs shaders from root to executable directory)
    // Copy from: assets/shaders (project root)
    // Copy to: assets/shaders (relative to executable, i.e., build/bin/Debug/assets/shaders)
    ShaderFileWatcher::Initialize("assets/shaders", "assets/shaders");
    
    // Initialize window
    Window window;
    WindowConfig config;
    config.title = "OpenEye Editor";
    config.width = 1920;
    config.height = 1080;
    config.vsync = true;
    config.debugContext = true;
    
    if (!window.Initialize(config)) {
        OE_FATAL("Failed to initialize window");
        Log::Shutdown();
        return -1;
    }
    
    // Initialize time
    Time::Initialize();
    
    OE_INFO("Creating ECS Scene...");
    Scene scene;
    
    OE_INFO("Loading shaders and creating resources...");
    auto shader = std::make_shared<Shader>();
    if (!shader->LoadFromFile("assets/shaders/basic.vert", "assets/shaders/basic.frag")) {
        OE_ERROR("Failed to load shader");
        Log::Shutdown();
        return -1;
    }
    
    auto mesh = MeshLoader::CreateCube();
    auto material = std::make_shared<Material>(shader);
    material->SetAlbedo(glm::vec3(0.8f, 0.3f, 0.3f));
    material->SetMetallic(0.0f);
    material->SetRoughness(0.5f);
    
    OE_INFO("Creating entities...");
    // Create rotating cube entity
    Entity cubeEntity = scene.CreateEntity("RotatingCube");
    auto& initialCubeTransform = scene.GetRegistry().get<Transform>(cubeEntity);
    initialCubeTransform.position = glm::vec3(0.0f, 0.0f, 0.0f);
    
    scene.GetRegistry().emplace<Renderable>(cubeEntity, mesh, material);
    scene.GetRegistry().emplace<AABB>(cubeEntity); // AABB for culling (future)
    
    // Create a few more test cubes
    for (int i = 0; i < 3; i++) {
        Entity testCube = scene.CreateEntity("TestCube_" + std::to_string(i));
        auto& transform = scene.GetRegistry().get<Transform>(testCube);
        transform.position = glm::vec3((i - 1) * 2.0f, 0.0f, -2.0f);
        
        auto testMaterial = std::make_shared<Material>(shader);
        float hue = i / 3.0f;
        testMaterial->SetAlbedo(glm::vec3(
            0.5f + 0.5f * cos(hue * 6.28f),
            0.5f + 0.5f * cos((hue + 0.33f) * 6.28f),
            0.5f + 0.5f * cos((hue + 0.66f) * 6.28f)
        ));
        
        scene.GetRegistry().emplace<Renderable>(testCube, mesh, testMaterial);
        scene.GetRegistry().emplace<AABB>(testCube);
    }
    
    // Count entities with Transform component (all entities should have this)
    size_t entityCount = scene.GetRegistry().view<Transform>().size();
    OE_INFO_F("Created %zu entities in scene", entityCount);
    
    // Camera setup (using standalone camera for now, will integrate with ECS later)
    Camera camera;
    camera.SetPosition(glm::vec3(0.0f, 0.0f, 5.0f));
    camera.SetPerspective(45.0f, window.GetAspectRatio(), 0.1f, 100.0f);
    camera.SetMovementSpeed(5.0f);
    camera.SetMouseSensitivity(0.1f);
    
    bool rightMouseButtonDown = false;
    double lastMouseX = 0.0;
    double lastMouseY = 0.0;
    bool firstMouse = true;
    
    // Setup input callbacks (after camera is created)
    window.SetKeyCallback([&](int key, int scancode, int action, int mods) {
        Input::OnKeyEvent(key, scancode, action, mods);
        
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
            window.SetShouldClose(true);
        }
        
        // Camera keyboard input
        if (action == GLFW_PRESS || action == GLFW_RELEASE) {
            camera.ProcessKeyboard(key, action == GLFW_PRESS);
        }
    });
    
    window.SetMouseButtonCallback([&](int button, int action, int mods) {
        Input::OnMouseButtonEvent(button, action, mods);
        
        if (button == GLFW_MOUSE_BUTTON_RIGHT) {
            rightMouseButtonDown = (action == GLFW_PRESS);
            if (rightMouseButtonDown) {
                window.SetMouseLocked(true);
                firstMouse = true;
            } else {
                window.SetMouseLocked(false);
            }
        }
    });
    
    window.SetMouseMoveCallback([&](double x, double y) {
        Input::OnMouseMoveEvent(x, y);
        
        if (rightMouseButtonDown) {
            if (firstMouse) {
                lastMouseX = x;
                lastMouseY = y;
                firstMouse = false;
            }
            
            double deltaX = x - lastMouseX;
            double deltaY = y - lastMouseY;
            
            camera.ProcessMouseMovement(static_cast<float>(deltaX), static_cast<float>(deltaY), true);
            
            lastMouseX = x;
            lastMouseY = y;
        }
    });
    
    window.SetScrollCallback([&](double xoffset, double yoffset) {
        Input::OnScrollEvent(xoffset, yoffset);
        camera.ProcessMouseScroll(static_cast<float>(yoffset));
    });
    
    OE_INFO("Editor initialized successfully");
    OE_INFO("Controls: WASD (move), QE (up/down), Right-click drag (rotate), Scroll (zoom)");
    OE_INFO("Press ESC to exit");
    
    // Enable depth testing
    glEnable(GL_DEPTH_TEST);
    
    // Main loop
    while (!window.ShouldClose()) {
        Time::Update();
        Input::Update();
        
        window.PollEvents();
        
        // Update camera
        float deltaTime = Time::DeltaTime();
        camera.Update(deltaTime);
        
        // Update projection if window was resized
        camera.SetPerspective(camera.GetFOV(), window.GetAspectRatio(), 0.1f, 100.0f);
        
        // Update ECS systems
        TransformSystem::Update(scene);
        
        // Rotate the main cube
        float time = Time::TotalTime();
        auto& cubeTransform = scene.GetRegistry().get<Transform>(cubeEntity);
        cubeTransform.rotation = glm::angleAxis(time * 1.0f, glm::vec3(0.0f, 1.0f, 0.0f));
        TransformSystem::MarkDirty(scene.GetRegistry(), cubeEntity);
        
        // Clear screen
        window.Clear(0.1f, 0.1f, 0.15f, 1.0f);
        
        // Update AABBs when transforms change
        // Note: Should be done when transforms change, but for now update every frame
        // In production, only update when transform is dirty
        CullingSystem::UpdateAABBs(scene);
        
        // Extract frustum for culling
        Frustum frustum;
        glm::mat4 viewProj = camera.GetProjectionMatrix() * camera.GetViewMatrix();
        frustum.ExtractFromMatrix(viewProj);
        
        // Toggle to compare: forward (basic) vs deferred
        static const bool useDeferred = false;
        if (!useDeferred) {
            // Forward path: draw the main cube directly, bypassing culling/ECS
            glm::mat4 view = camera.GetViewMatrix();
            glm::mat4 proj = camera.GetProjectionMatrix();
            
            // Use the material/shader created for the cube
            material->Bind();
            auto forwardShader = material->GetShader();
            if (forwardShader) {
                // Model matrix from ECS transform
                glm::mat4 model = cubeTransform.GetWorldMatrix(scene.GetRegistry());
                forwardShader->SetMat4("u_Model", model);
                forwardShader->SetMat4("u_View", view);
                forwardShader->SetMat4("u_Projection", proj);
                forwardShader->SetVec3("u_ViewPos", camera.GetPosition());
                forwardShader->SetVec3("u_LightDir", glm::normalize(glm::vec3(1.0f, 1.0f, 1.0f)));
                forwardShader->SetVec3("u_LightColor", glm::vec3(1.0f));
            }
            
            mesh->Bind();
            mesh->Draw();
            mesh->Unbind();
            
            if (forwardShader) {
                forwardShader->Unbind();
            }
            
            // Optionally render others via ECS forward path (without culling)
            // RenderSystem::Update(scene, camera, view, proj);
        } else {
            // Deferred rendering pipeline
            static GBuffer gbuffer;
            static bool gbufferInitialized = false;
            if (!gbufferInitialized) {
                gbuffer.Initialize(static_cast<uint32_t>(window.GetWidth()), 
                                  static_cast<uint32_t>(window.GetHeight()));
                gbufferInitialized = true;
            }
            
            // Resize G-Buffer if window size changed
            if (gbuffer.GetWidth() != static_cast<uint32_t>(window.GetWidth()) || 
                gbuffer.GetHeight() != static_cast<uint32_t>(window.GetHeight())) {
                gbuffer.Resize(static_cast<uint32_t>(window.GetWidth()), 
                             static_cast<uint32_t>(window.GetHeight()));
            }
            
            // Shadow pass: render depth from light's perspective
            static ShadowMap shadowMap;
            static bool shadowMapInitialized = false;
            if (!shadowMapInitialized) {
                shadowMap.Initialize(2048, 4); // 2048x2048, 4 cascades
                shadowMapInitialized = true;
            }
            
            glm::vec3 lightDirection = glm::normalize(glm::vec3(1.0f, 1.0f, 1.0f));
            shadowMap.UpdateMatrices(lightDirection, camera.GetViewMatrix(), 
                                    camera.GetProjectionMatrix(), camera.GetPosition(),
                                    0.1f, 100.0f);
            
            // Render shadow maps for each cascade
            for (uint32_t i = 0; i < shadowMap.GetCascadeCount(); i++) {
                RenderSystem::RenderShadowPass(scene, shadowMap, i, lightDirection);
            }
            
            // Geometry pass: fill G-Buffer
            RenderSystem::RenderGeometryPass(scene, gbuffer, camera,
                                             camera.GetViewMatrix(), camera.GetProjectionMatrix(),
                                             frustum);
            
            // Lighting pass: render from G-Buffer
            static std::shared_ptr<Shader> lightingShader = nullptr;
            if (!lightingShader) {
                lightingShader = std::make_shared<Shader>();
                if (!lightingShader->LoadFromFile("assets/shaders/lighting.vert", "assets/shaders/lighting.frag")) {
                    OE_ERROR("Failed to load lighting shader");
                    lightingShader = nullptr;
                }
            }
            
            if (lightingShader) {
                // Pass shadow map to lighting pass (it will handle uniform setting)
                RenderSystem::RenderLightingPass(gbuffer, camera, lightingShader, &shadowMap);
            }
        }
        
        // TODO: Render UI panels (Phase 9)
        
        // Swap buffers
        window.SwapBuffers();
        
        // Log FPS every second
        static float lastFpsLog = 0.0f;
        if (Time::TotalTime() - lastFpsLog >= 1.0f) {
            OE_INFO_F("FPS: %.1f | Frame: %llu | DeltaTime: %.4f", 
                      Time::FPS(), Time::FrameCount(), Time::DeltaTime());
            lastFpsLog = Time::TotalTime();
        }
    }
    
    OE_INFO("=== OpenEye Editor Shutting Down ===");
    window.Shutdown();
    Log::Shutdown();
    
    return 0;
}

