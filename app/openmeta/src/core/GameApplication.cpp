#include "GameApplication.h"
#include "core/api/All.h"
#include "ecs/api/All.h"
#include "rendering/api/All.h"
#include "ecs/Systems.h"
// Include internal PhysicsWorld for PhysicsSystem functions that need it
// This is needed because PhysicsSystem::CreateGroundPlane and AttachPhysicsBody
// take Physics::PhysicsWorld* (internal type), not the API type
#include "physics/PhysicsWorld.h"
#include "particles/ParticleEmitter.h"
#include "fluids/FluidSimulation.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <filesystem>
#include <glm/gtc/quaternion.hpp>

namespace OpenMeta {

GameApplication::GameApplication() = default;

GameApplication::~GameApplication() {
    Shutdown();
}

bool GameApplication::Initialize() {
    using namespace OpenEye::Core;
    
    // Initialize logging
    Log::Initialize("openmeta.log");
    OE_INFO("OpenMeta Game Initializing...");

    // Create window
    m_window = std::make_unique<Window>();
    WindowConfig config;
    config.title = "OpenMeta - City Sandbox";
    config.width = 1920;
    config.height = 1080;
    config.vsync = true;
    config.debugContext = true;
    config.resizable = true; // Enable window resizing, maximize, and minimize
    
    if (!m_window->Initialize(config)) {
        OE_ERROR("Failed to initialize window");
        return false;
    }

    // Initialize GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        OE_ERROR("Failed to initialize GLAD");
        return false;
    }

    // Initialize time
    Time::Initialize();
    
    // Setup input callbacks
    SetupInputCallbacks();

    // Initialize Physics World (using API)
    OE_INFO("Initializing Physics World...");
    m_physicsWorld = std::make_unique<OpenEye::Physics::API::PhysicsWorld>();
    m_physicsWorld->Initialize(glm::vec3(0.0f, -9.81f, 0.0f));

    // Create scene
    m_scene = std::make_unique<OpenEye::ECS::Scene>();
    
    // Initialize RenderPipeline
    int width, height;
    glfwGetFramebufferSize(m_window->GetNativeHandle(), &width, &height);
    m_renderPipeline = std::make_unique<OpenEye::Rendering::RenderPipeline>();
    
    // Setup resize callback AFTER render pipeline is created
    // Defer resize to next frame to avoid destroying framebuffers during rendering
    m_window->SetResizeCallback([this](int width, int height) {
        if (width > 0 && height > 0) {
            // Store pending resize (will be applied at start of next Render() call)
            m_pendingWidth = width;
            m_pendingHeight = height;
            m_pendingResize = true;
        }
    });
    if (!m_renderPipeline->Initialize(width, height)) {
        OE_ERROR("Failed to initialize RenderPipeline");
        return false;
    }
    
    // Load scene from file
    LoadScene();

    m_isRunning = true;
    OE_INFO("OpenMeta Game Initialized Successfully");
    return true;
}

void GameApplication::Run() {
    using namespace OpenEye::Core;
    
    while (m_isRunning && m_window && !m_window->ShouldClose()) {
        Time::Update();
        m_window->PollEvents();
        Input::Update();

        float deltaTime = Time::DeltaTime();
        
        // Update game systems
        Update(deltaTime);

        // Render
        Render();
        
        m_window->SwapBuffers();
    }
}

void GameApplication::Update(float deltaTime) {
    if (!m_scene || !m_physicsWorld) return;
    
    // Update camera controls
    UpdateCameraControls(deltaTime);
    
    // Get internal physics world from API wrapper (void* to maintain API boundary)
    void* internalPhysicsWorld = m_physicsWorld->GetInternalWorld();
    
    // Update all ECS systems (Scene orchestrates everything)
    m_scene->Update(deltaTime, internalPhysicsWorld);
}

void GameApplication::Render() {
    if (!m_scene || !m_renderPipeline) return;
    
    // Handle pending resize BEFORE rendering (prevents destroying framebuffers during render)
    if (m_pendingResize) {
        // Update viewport immediately
        glViewport(0, 0, m_pendingWidth, m_pendingHeight);
        
        // Update render pipeline
        if (m_renderPipeline) {
            m_renderPipeline->Resize(m_pendingWidth, m_pendingHeight);
        }
        
        // Update camera aspect ratio
        if (m_scene) {
            auto& registry = m_scene->GetRegistry();
            auto cameraView = registry.view<OpenEye::ECS::CameraComponent, OpenEye::ECS::Transform>();
            for (auto entity : cameraView) {
                auto& cameraComp = cameraView.get<OpenEye::ECS::CameraComponent>(entity);
                if (cameraComp.isPrimary) {
                    float newAspect = (float)m_pendingWidth / (float)m_pendingHeight;
                    cameraComp.aspect = newAspect;
                    cameraComp.camera.SetPerspective(cameraComp.fov, cameraComp.aspect, cameraComp.nearPlane, cameraComp.farPlane);
                    break;
                }
            }
        }
        
        // Clear pending resize flag
        m_pendingResize = false;
    }
    
    auto* camera = GetMainCamera();
    if (!camera) return;
    
    // Setup lighting params
    OpenEye::Rendering::LightingParams params;
    params.lightDir = glm::vec3(0.3f, -1.0f, 0.2f);
    params.lightColor = glm::vec3(1.0f, 1.0f, 1.0f);
    params.lightIntensity = 1.0f;
    params.ambient = glm::vec3(0.1f, 0.1f, 0.1f);
    params.exposure = 1.0f;
    
    // Get lighting from scene
    auto& registry = m_scene->GetRegistry();
    auto lightView = registry.view<OpenEye::ECS::Light, OpenEye::ECS::Transform>();
    for (auto entity : lightView) {
        auto& light = lightView.get<OpenEye::ECS::Light>(entity);
        auto& transform = lightView.get<OpenEye::ECS::Transform>(entity);
        
        if (light.type == OpenEye::ECS::Light::Type::Directional) {
            glm::mat4 worldMatrix = transform.GetWorldMatrix(registry);
            params.lightDir = glm::normalize(glm::vec3(worldMatrix * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f)));
            params.lightColor = light.color;
            params.lightIntensity = light.intensity;
        }
    }
    
    // Render options
    OpenEye::Rendering::RenderOptions options;
    options.enableCulling = m_enableCulling;
    options.enableLOD = m_enableLOD;
    options.enableInstancing = m_enableInstancing;
    options.minInstancesForBatch = m_minInstancesForBatch;
    
    // Render frame
    m_renderPipeline->RenderFrame(*m_scene, *camera, params, options);
}

OpenEye::Rendering::Camera* GameApplication::GetMainCamera() {
    if (!m_scene) return nullptr;
    
    auto& registry = m_scene->GetRegistry();
    auto cameraView = registry.view<OpenEye::ECS::CameraComponent, OpenEye::ECS::Transform>();
    
    // Find primary camera
    for (auto entity : cameraView) {
        auto& cameraComp = cameraView.get<OpenEye::ECS::CameraComponent>(entity);
        if (cameraComp.isPrimary) {
            auto& transform = cameraView.get<OpenEye::ECS::Transform>(entity);
            auto& camera = cameraComp.camera;
            
            // Update camera transform
            glm::mat4 worldMatrix = transform.GetWorldMatrix(registry);
            glm::vec3 position = glm::vec3(worldMatrix[3]);
            glm::mat3 rotationMatrix = glm::mat3(worldMatrix);
            glm::vec3 forward = glm::normalize(rotationMatrix * glm::vec3(0.0f, 0.0f, -1.0f));
            camera.SetPosition(position);
            camera.SetTarget(position + forward); // Set target to position + forward direction
            
            return &camera;
        }
    }
    
    return nullptr;
}

void GameApplication::LoadScene() {
    using namespace OpenEye::Core;
    using namespace OpenEye::ECS;
    
    // Try to load scene from file (check both relative and absolute paths)
    std::string scenePath = m_scenePath;
    if (!std::filesystem::exists(scenePath)) {
        // Try relative to executable directory
        scenePath = "resources/scenes/default.json";
    }
    
    if (std::filesystem::exists(scenePath)) {
        if (SceneSerializer::Deserialize(*m_scene, scenePath)) {
            OE_INFO("Scene loaded from: " + scenePath);
            return;
        } else {
            OE_WARN("Failed to load scene from: " + scenePath);
        }
    } else {
        OE_WARN("Scene file not found: " + scenePath + ", creating default scene");
    }
    
    // Scene file doesn't exist or failed to load - create basic scene
    CreateDefaultScene();
}

void GameApplication::CreateDefaultScene() {
    using namespace OpenEye::Core;
    using namespace OpenEye::ECS;
    
    auto& registry = m_scene->GetRegistry();
    
    // Create main camera
    auto mainCamera = m_scene->CreateEntity("Main Camera");
    auto& cameraComp = registry.emplace<CameraComponent>(mainCamera);
    cameraComp.isPrimary = true;
    
    auto& transform = registry.get<Transform>(mainCamera);
    transform.position = glm::vec3(0.0f, 5.0f, 10.0f);
    transform.rotation = glm::quatLookAt(glm::normalize(glm::vec3(0.0f, -0.3f, -1.0f)), glm::vec3(0.0f, 1.0f, 0.0f));
    
    // Create directional light
    auto light = m_scene->CreateEntity("Directional Light");
    auto& lightComp = registry.emplace<Light>(light);
    lightComp.type = Light::Type::Directional;
    lightComp.color = glm::vec3(1.0f, 1.0f, 1.0f);
    lightComp.intensity = 1.0f;
    
    auto& lightTransform = registry.get<Transform>(light);
    lightTransform.rotation = glm::quatLookAt(glm::normalize(glm::vec3(0.3f, -1.0f, 0.2f)), glm::vec3(0.0f, 1.0f, 0.0f));
    
    // Set greyish sky color
    if (m_renderPipeline) {
        m_renderPipeline->SetSkyColor(glm::vec3(0.6f, 0.65f, 0.7f)); // Greyish sky
    }
    
    // Create ground plane
    if (m_physicsWorld) {
        void* internalPhysicsWorld = m_physicsWorld->GetInternalWorld();
        OpenEye::Physics::PhysicsWorld* physicsWorld = static_cast<OpenEye::Physics::PhysicsWorld*>(internalPhysicsWorld);
        
        Entity groundEntity = PhysicsSystem::CreateGroundPlane(
            *m_scene,
            physicsWorld,
            glm::vec3(0.0f, 0.0f, 0.0f),  // Position (top surface at y=0)
            glm::vec2(50.0f, 50.0f)        // Size (50x50 units)
        );
        OE_INFO("Created ground plane");
    }
    
    // Load dess_holiday model
    std::string dessModelPath = "assets/models/dess_holiday_hb_juan_deltarune/scene.gltf";
    Entity dessEntity = m_scene->LoadModel(dessModelPath, "Dess", glm::vec3(0.0f, 1.0f, 0.0f));
    
    if (dessEntity != entt::null && m_physicsWorld) {
        // Get internal physics world
        void* internalPhysicsWorld = m_physicsWorld->GetInternalWorld();
        OpenEye::Physics::PhysicsWorld* physicsWorld = static_cast<OpenEye::Physics::PhysicsWorld*>(internalPhysicsWorld);
        
        // Create capsule collider for dess model
        // Capsule: radius=0.4f, height=1.6f (typical character proportions)
        float capsuleRadius = 0.4f;
        float capsuleHeight = 1.6f;
        auto capsuleShape = physicsWorld->CreateCapsuleShape(capsuleRadius, capsuleHeight);
        
        // Attach physics body to the parent entity (or first child mesh if parent has no renderable)
        // First try to find a child entity with Renderable component
        Entity targetEntity = dessEntity;
        auto view = registry.view<Renderable>();
        for (auto entity : view) {
            auto& t = registry.get<Transform>(entity);
            if (t.parent == dessEntity) {
                targetEntity = entity;
                break;
            }
        }
        
        // Attach capsule collider with dynamic physics (mass > 0)
        float mass = 1.0f; // Dynamic body
        if (PhysicsSystem::AttachPhysicsBody(*m_scene, targetEntity, physicsWorld, capsuleShape, mass, false)) {
            OE_INFO("Added capsule collider to Dess model");
        }
    }
    
    // Create particle emitter (smoke/dust effect)
    {
        auto particleEntity = m_scene->CreateEntity("ParticleEmitter");
        auto& particleTransform = registry.get<Transform>(particleEntity);
        particleTransform.position = glm::vec3(2.0f, 0.5f, 0.0f);
        
        OpenEye::Particles::ParticleEmitterConfig particleConfig;
        particleConfig.position = particleTransform.position;
        particleConfig.direction = glm::vec3(0.0f, 1.0f, 0.0f);
        particleConfig.spread = 25.0f;
        particleConfig.minSpeed = 0.5f;
        particleConfig.maxSpeed = 2.0f;
        particleConfig.minLifetime = 2.0f;
        particleConfig.maxLifetime = 4.0f;
        particleConfig.minSize = 0.2f;
        particleConfig.maxSize = 0.6f;
        particleConfig.startColor = glm::vec4(0.8f, 0.8f, 0.9f, 1.0f); // Light gray/white
        particleConfig.endColor = glm::vec4(0.3f, 0.3f, 0.3f, 0.0f);   // Fade to transparent gray
        particleConfig.particlesPerSecond = 50.0f;
        particleConfig.maxParticles = 500;
        particleConfig.useGravity = true;
        particleConfig.gravity = glm::vec3(0.0f, -2.0f, 0.0f); // Lighter gravity for floating effect
        particleConfig.damping = 0.95f;
        
        auto particleEmitter = std::make_shared<OpenEye::Particles::ParticleEmitter>(particleConfig);
        particleEmitter->Start();
        registry.emplace<ParticleEmitterComponent>(particleEntity, particleEmitter);
        
        OE_INFO("Created particle emitter");
    }
    
    // Create fluid volume (smoke simulation)
    {
        auto fluidEntity = m_scene->CreateEntity("FluidVolume");
        auto& fluidTransform = registry.get<Transform>(fluidEntity);
        fluidTransform.position = glm::vec3(-2.0f, 1.0f, 0.0f);
        
        OpenEye::Fluids::FluidGridDesc fluidDesc;
        fluidDesc.resolution = glm::ivec3(64, 64, 64);
        fluidDesc.volumeSize = glm::vec3(2.0f, 2.0f, 2.0f);
        
        auto fluidSim = std::make_shared<OpenEye::Fluids::FluidSimulation>();
        if (fluidSim->Initialize(fluidDesc, OpenEye::Fluids::FluidType::Gas)) {
            // Configure smoke emission - increased rates for better visibility
            glm::vec3 emitPos(0.5f, 0.15f, 0.5f); // Normalized position in volume [0,1]
            float emitRadius = 0.08f; // Slightly larger radius
            float densityRate = 2.0f; // Increased density rate for better visibility
            float tempRate = 2.5f; // Increased temperature rate
            fluidSim->SetEmission(emitPos, emitRadius, densityRate, tempRate, true);
            
            // Set buoyancy for rising smoke
            fluidSim->SetBuoyancy(2.0f, 0.02f, 0.0f); // Increased sigma for stronger buoyancy
            
            // Reduce dissipation so smoke persists longer
            fluidSim->SetDissipation(0.998f, 0.995f); // Slower dissipation
            
            auto& fluidComp = registry.emplace<FluidVolumeComponent>(fluidEntity);
            fluidComp.simulation = fluidSim;
            fluidComp.resolution = fluidDesc.resolution;
            fluidComp.volumeSize = fluidDesc.volumeSize;
            fluidComp.enabled = true;
            
            OE_INFO("Created fluid volume (smoke simulation)");
        } else {
            OE_WARN("Failed to initialize fluid simulation");
        }
    }
    
    OE_INFO("Created default scene with camera, light, ground plane, Dess model, particles, and fluids");
}

void GameApplication::SetupInputCallbacks() {
    using namespace OpenEye::Core;
    using namespace OpenEye::ECS;
    
    // Key callback
    m_window->SetKeyCallback([this](int key, int scancode, int action, int mods) {
        Input::OnKeyEvent(key, scancode, action, mods);
    });
    
    // Mouse button callback - left click to look around
    m_window->SetMouseButtonCallback([this](int button, int action, int mods) {
        Input::OnMouseButtonEvent(button, action, mods);
        
        // Left mouse button controls camera rotation
        if (button == GLFW_MOUSE_BUTTON_LEFT) {
            m_cameraLocked = (action == GLFW_PRESS);
            m_window->SetMouseLocked(m_cameraLocked);
            if (m_cameraLocked) {
                m_firstMouse = true; // Reset mouse position when starting drag
                m_lastMousePos = Input::GetMousePosition(); // Initialize last position
                // Yaw/pitch will be initialized from current rotation in UpdateCameraControls
            } else {
                // When releasing mouse, keep current yaw/pitch so rotation continues smoothly
                m_firstMouse = false;
            }
        }
    });
    
    // Mouse move callback
    m_window->SetMouseMoveCallback([this](double x, double y) {
        Input::OnMouseMoveEvent(x, y);
    });
    
    // Scroll callback
    m_window->SetScrollCallback([this](double xoffset, double yoffset) {
        Input::OnScrollEvent(xoffset, yoffset);
    });
}

void GameApplication::UpdateCameraControls(float deltaTime) {
    using namespace OpenEye::Core;
    using namespace OpenEye::ECS;
    
    if (!m_scene) return;
    
    auto& registry = m_scene->GetRegistry();
    auto cameraView = registry.view<CameraComponent, Transform>();
    
    // Find primary camera
    for (auto entity : cameraView) {
        auto& cameraComp = cameraView.get<CameraComponent>(entity);
        if (!cameraComp.isPrimary) continue;
        
        auto& transform = cameraView.get<Transform>(entity);
        auto& camera = cameraComp.camera;
        
        // Get current camera orientation from transform (always up-to-date)
        glm::mat4 worldMatrix = transform.GetWorldMatrix(registry);
        glm::mat3 rotationMatrix = glm::mat3(worldMatrix);
        glm::vec3 forward = glm::normalize(rotationMatrix * glm::vec3(0.0f, 0.0f, -1.0f));
        glm::vec3 right = glm::normalize(rotationMatrix * glm::vec3(1.0f, 0.0f, 0.0f));
        glm::vec3 up = glm::normalize(rotationMatrix * glm::vec3(0.0f, 1.0f, 0.0f));
        
        // Process keyboard input (WASD QE movement) - always active, no X key needed
        glm::vec3 moveDirection(0.0f);
        
        if (Input::IsKeyPressed(GLFW_KEY_W)) {
            moveDirection += forward;
        }
        if (Input::IsKeyPressed(GLFW_KEY_S)) {
            moveDirection -= forward;
        }
        if (Input::IsKeyPressed(GLFW_KEY_A)) {
            moveDirection -= right;
        }
        if (Input::IsKeyPressed(GLFW_KEY_D)) {
            moveDirection += right;
        }
        if (Input::IsKeyPressed(GLFW_KEY_Q)) {
            moveDirection -= up; // Move down
        }
        if (Input::IsKeyPressed(GLFW_KEY_E)) {
            moveDirection += up; // Move up
        }
        
        // Apply movement
        if (glm::length(moveDirection) > 0.0f) {
            moveDirection = glm::normalize(moveDirection);
            transform.position += moveDirection * m_cameraSpeed * deltaTime;
            transform.dirty = true;
        }
        
        // Process mouse movement (camera rotation) - only when left mouse button is pressed
        if (m_cameraLocked) {
            glm::vec2 currentMousePos = Input::GetMousePosition();
            
            // Initialize yaw/pitch from current rotation when first locking
            if (m_firstMouse) {
                m_lastMousePos = currentMousePos;
                m_firstMouse = false;
                
                // Initialize yaw/pitch from current rotation (extract from quaternion)
                // Extract euler angles from quaternion rotation
                glm::vec3 euler = glm::eulerAngles(transform.rotation);
                m_cameraYaw = glm::degrees(euler.y); // Y rotation (around Y axis)
                m_cameraPitch = glm::degrees(euler.x); // X rotation (around X axis)
            } else {
                // Calculate mouse delta manually (more reliable than Input::GetMouseDelta)
                glm::vec2 mouseDelta = currentMousePos - m_lastMousePos;
                
                // Process rotation if there's any mouse movement
                const float epsilon = 0.0001f;
                if (glm::abs(mouseDelta.x) > epsilon || glm::abs(mouseDelta.y) > epsilon) {
                    // Convert mouse delta to rotation
                    // Reversed mouse look: left = rotate right, right = rotate left, up = rotate up
                    float yawDelta = -mouseDelta.x * m_mouseSensitivity; // Negative to reverse left-right
                    float pitchDelta = mouseDelta.y * m_mouseSensitivity;
                    
                    // Update yaw and pitch
                    m_cameraYaw += yawDelta;
                    m_cameraPitch -= pitchDelta; // Subtract for standard mouse look (moving mouse up looks up)
                    
                    // Clamp pitch to prevent gimbal lock
                    m_cameraPitch = glm::clamp(m_cameraPitch, -89.0f, 89.0f);
                    
                    // Convert yaw and pitch to quaternion
                    // For FPS camera: yaw around global Y axis, then pitch around local X axis
                    glm::quat yawQuat = glm::angleAxis(glm::radians(m_cameraYaw), glm::vec3(0.0f, 1.0f, 0.0f));
                    glm::quat pitchQuat = glm::angleAxis(glm::radians(m_cameraPitch), glm::vec3(1.0f, 0.0f, 0.0f));
                    transform.rotation = yawQuat * pitchQuat; // Yaw first, then pitch
                    transform.dirty = true;
                }
                
                // Update last mouse position for next frame
                m_lastMousePos = currentMousePos;
            }
        }
        
        // Always update camera position and orientation from transform
        worldMatrix = transform.GetWorldMatrix(registry);
        glm::vec3 position = glm::vec3(worldMatrix[3]);
        rotationMatrix = glm::mat3(worldMatrix);
        forward = glm::normalize(rotationMatrix * glm::vec3(0.0f, 0.0f, -1.0f));
        
        camera.SetPosition(position);
        camera.SetTarget(position + forward);
        
        break; // Only process primary camera
    }
}

void GameApplication::Shutdown() {
    using namespace OpenEye::Core;
    
    OE_INFO("OpenMeta Game Shutting Down...");

    m_renderPipeline.reset();
    m_physicsWorld.reset();
    m_scene.reset();
    m_window.reset();

    Log::Shutdown();
}

} // namespace OpenMeta

