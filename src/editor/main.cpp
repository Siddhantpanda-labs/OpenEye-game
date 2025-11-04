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
#include <filesystem>
#include "rendering/Camera.h"
#include "rendering/Frustum.h"
#include "rendering/GBuffer.h"
#include "rendering/ShadowMap.h"
#include "rendering/resources/Shader.h"
#include "rendering/resources/Cubemap.h"
#include "rendering/SkyboxRenderer.h"
#include "rendering/RenderPipeline.h"
#include "rendering/IBL.h"
#include "physics/PhysicsWorld.h"
#include "animation/AnimationSystem.h"
#include "particles/ParticleEmitter.h"
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <filesystem>
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

using namespace OpenEye::Core;
using namespace OpenEye::Rendering;
using namespace OpenEye::ECS;
using namespace OpenEye::Particles;

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
    
    // Initialize ImGui (docking + viewports)
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }
    ImGui_ImplGlfw_InitForOpenGL(window.GetNativeHandle(), true);
    ImGui_ImplOpenGL3_Init("#version 450");
    
    // Initialize time
    Time::Initialize();
    
    OE_INFO("Creating ECS Scene...");
    Scene scene;
    
    // Initialize Physics World
    OE_INFO("Initializing Physics World...");
    auto physicsWorld = std::make_unique<OpenEye::Physics::PhysicsWorld>();
    physicsWorld->Initialize(glm::vec3(0.0f, -9.81f, 0.0f));
    OE_INFO("Physics World initialized successfully");
    
    OE_INFO("Loading shaders and creating resources...");
    auto shader = std::make_shared<Shader>();
    if (!shader->LoadFromFile("assets/shaders/passes/forward/basic.vert", "assets/shaders/passes/forward/basic.frag")) {
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
    
    // Add some test point lights for multiple light testing
    Entity pointLight1 = scene.CreateEntity("PointLight1");
    auto& light1Transform = scene.GetRegistry().get<Transform>(pointLight1);
    light1Transform.position = glm::vec3(2.0f, 2.0f, 0.0f);
    Light light1;
    light1.type = Light::Type::Point;
    light1.color = glm::vec3(1.0f, 0.8f, 0.6f); // Warm yellow
    light1.intensity = 5.0f;
    light1.range = 10.0f;
    scene.GetRegistry().emplace<Light>(pointLight1, light1);
    
    Entity pointLight2 = scene.CreateEntity("PointLight2");
    auto& light2Transform = scene.GetRegistry().get<Transform>(pointLight2);
    light2Transform.position = glm::vec3(-2.0f, 2.0f, -2.0f);
    Light light2;
    light2.type = Light::Type::Point;
    light2.color = glm::vec3(0.6f, 0.8f, 1.0f); // Cool blue
    light2.intensity = 4.0f;
    light2.range = 8.0f;
    scene.GetRegistry().emplace<Light>(pointLight2, light2);
    
    // Load glTF model with textures
    auto resolveAssetPath = [](const std::string& relative) -> std::string {
        namespace fs = std::filesystem;
        std::error_code ec;
        fs::path current = fs::current_path(ec);
        fs::path rel(relative);
        for (fs::path p = current; !p.empty(); p = p.parent_path()) {
            fs::path candidate = p / rel;
            if (fs::exists(candidate, ec)) return candidate.string();
            if (p == p.parent_path()) break;
        }
        return relative;
    };
    
    // Try to load animated FBX model first (target: "Kick To The Groin.fbx")
    std::string fbxPath = resolveAssetPath("assets/models/Kick To The Groin.fbx");
    std::string modelPath = "";
    
    if (std::filesystem::exists(fbxPath)) {
        modelPath = fbxPath;
        OE_INFO("Found FBX model: " + fbxPath);
    } else {
        // Fallback to glTF model
        modelPath = resolveAssetPath("assets/models/dess_holiday_hb_juan_deltarune/scene.gltf");
        OE_INFO("FBX model not found, trying glTF: " + modelPath);
    }
    
    if (std::filesystem::exists(modelPath)) {
        OE_INFO("Loading model: " + modelPath);
        auto loadedMeshes = MeshLoader::LoadSceneFromFile(modelPath);
        
        if (!loadedMeshes.empty()) {
            OE_INFO("Loaded " + std::to_string(loadedMeshes.size()) + " meshes from model");
            
            for (size_t i = 0; i < loadedMeshes.size(); ++i) {
                const auto& loadedMesh = loadedMeshes[i];
                if (!loadedMesh->mesh || loadedMeshes[i]->materials.empty()) continue;
                
                // Check if mesh has multiple submeshes with different materials
                size_t submeshCount = loadedMesh->mesh->GetSubmeshCount();
                size_t materialCount = loadedMesh->materials.size();
                
                OE_INFO("Mesh '" + loadedMesh->name + "': " + 
                       std::to_string(submeshCount) + " submeshes, " + 
                       std::to_string(materialCount) + " materials");
                
                // For now, create one entity per mesh (with first material)
                // TODO: If mesh has multiple submeshes with different materials, 
                // we should create separate entities or handle submesh rendering per-material
                Entity modelEntity = scene.CreateEntity(loadedMesh->name.empty() ? "ModelMesh_" + std::to_string(i) : loadedMesh->name);
                auto& modelTransform = scene.GetRegistry().get<Transform>(modelEntity);
                
                // Position model - center it and scale down if needed
                modelTransform.position = glm::vec3(0.0f, 0.0f, -5.0f); // Place in front of camera
                modelTransform.scale = glm::vec3(0.1f); // Scale down 10x (0.1 = 10 times smaller)
                
                // Use the material from loaded mesh (which should have textures)
                auto modelMaterial = loadedMesh->materials[0];
                if (!modelMaterial) {
                    modelMaterial = std::make_shared<Material>(shader);
                }
                // Check if model has animation (determine shader type)
                bool hasAnimation = loadedMesh->skeleton && !loadedMesh->animations.empty();
                
                // Always set the correct shader for deferred rendering (replace any existing shader)
                // This ensures animated models use skinned shader and static models use regular gbuffer shader
                if (modelMaterial) {
                    auto gbufferShader = std::make_shared<Shader>();
                    if (hasAnimation) {
                        // Use skinned shader for animated models
                        gbufferShader->LoadFromFile("assets/shaders/passes/deferred/gbuffer_skinned.vert", 
                                                    "assets/shaders/passes/deferred/gbuffer_skinned.frag");
                    } else {
                        // Use regular shader for static models
                        gbufferShader->LoadFromFile("assets/shaders/passes/deferred/gbuffer.vert", 
                                                    "assets/shaders/passes/deferred/gbuffer.frag");
                    }
                    modelMaterial->SetShader(gbufferShader);
                }
                
                scene.GetRegistry().emplace<Renderable>(modelEntity, loadedMesh->mesh, modelMaterial);
                scene.GetRegistry().emplace<AABB>(modelEntity);
                
                // Attach AnimationComponent if model has skeleton/animation
                if (hasAnimation) {
                    AnimationComponent animComp;
                    animComp.skeleton = std::static_pointer_cast<void>(loadedMesh->skeleton);
                    
                    // Convert AnimationClip pointers to void pointers for storage
                    animComp.clips.clear();
                    animComp.clips.reserve(loadedMesh->animations.size());
                    for (const auto& clip : loadedMesh->animations) {
                        animComp.clips.push_back(std::static_pointer_cast<void>(clip));
                    }
                    
                    // Auto-play first animation clip
                    animComp.currentClipIndex = 0;  // Play first clip automatically
                    animComp.isPlaying = true;       // Start playing automatically
                    animComp.loop = true;
                    animComp.playbackSpeed = 1.0f;
                    animComp.boneMatricesDirty = true;
                    
                    scene.GetRegistry().emplace<AnimationComponent>(modelEntity, animComp);
                    
                    // Initialize bone matrices immediately
                    auto& registry = scene.GetRegistry();
                    OpenEye::Animation::AnimationSystem::CalculateBoneMatrices(registry, modelEntity);
                    
                    OE_INFO("  - Animation component attached (skeleton: " + std::to_string(loadedMesh->skeleton->GetBoneCount()) + " bones, " + std::to_string(loadedMesh->animations.size()) + " clips)");
                    OE_INFO("  - Auto-playing first animation clip");
                }
                
                // Log loaded textures for debugging
                OE_INFO("Model entity '" + loadedMesh->name + "' created");
                if (modelMaterial->GetTexture("u_AlbedoTexture")) {
                    OE_INFO("  - Albedo texture loaded");
                }
                if (modelMaterial->GetTexture("u_NormalTexture")) {
                    OE_INFO("  - Normal texture loaded");
                }
                if (modelMaterial->GetTexture("u_MetallicRoughnessTexture")) {
                    OE_INFO("  - MetallicRoughness texture loaded");
                }
                if (modelMaterial->GetTexture("u_MetallicTexture")) {
                    OE_INFO("  - Metallic texture loaded");
                }
                if (modelMaterial->GetTexture("u_RoughnessTexture")) {
                    OE_INFO("  - Roughness texture loaded");
                }
                if (modelMaterial->GetTexture("u_AOTexture")) {
                    OE_INFO("  - AO texture loaded");
                }
                if (modelMaterial->GetTexture("u_EmissiveTexture")) {
                    OE_INFO("  - Emissive texture loaded");
                }
                
                // Attach physics body to model (dynamic, with mass)
                // Use convex hull shape from mesh for collision
                if (loadedMesh->mesh) {
                    auto collisionShape = physicsWorld->CreateConvexHullShape(loadedMesh->mesh);
                    if (collisionShape) {
                        // Position model above ground for physics test
                        modelTransform.position = glm::vec3(0.0f, 5.0f, -5.0f); // Start 5 units above ground
                        modelTransform.scale = glm::vec3(0.1f);
                        
                        // Attach physics body with mass (dynamic)
                        if (PhysicsSystem::AttachPhysicsBody(scene, modelEntity, physicsWorld.get(), collisionShape, 1.0f, false)) {
                            OE_INFO("  - Physics body attached (dynamic, mass=1.0)");
                        } else {
                            OE_WARN("  - Failed to attach physics body to model");
                        }
                    } else {
                        OE_WARN("  - Failed to create collision shape for model");
                    }
                }
            }
        } else {
            OE_WARN("Failed to load model from: " + modelPath);
        }
    } else {
        OE_WARN("Model file not found: " + modelPath);
    }
    
    // Create ground plane for physics test
    OE_INFO("Creating ground plane for physics test...");
    Entity groundEntity = PhysicsSystem::CreateGroundPlane(
        scene,
        physicsWorld.get(),
        glm::vec3(0.0f, -1.0f, 0.0f),  // Position ground at y=-1
        glm::vec2(50.0f, 50.0f)         // 50x50 ground plane
    );
    OE_INFO("Ground plane created successfully");
    
    // Create test particle emitters (positioned near the model)
    // Model is at (0.0f, 5.0f, -5.0f) with scale 0.1f
    glm::vec3 modelBasePos = glm::vec3(0.0f, 5.0f, -5.0f);
    
    OE_INFO("Creating test particle emitters...");
    
    // Fire effect (to the left of model, at ground level)
    ParticleEmitterConfig fireConfig;
    fireConfig.position = modelBasePos + glm::vec3(-2.0f, -4.0f, 0.0f);  // Left side, ground level
    fireConfig.direction = glm::vec3(0.0f, 1.0f, 0.0f);
    fireConfig.spread = 20.0f;
    fireConfig.minSpeed = 0.5f;
    fireConfig.maxSpeed = 2.0f;
    fireConfig.minLifetime = 0.5f;
    fireConfig.maxLifetime = 1.5f;
    fireConfig.minSize = 0.2f;
    fireConfig.maxSize = 0.5f;
    fireConfig.startColor = glm::vec4(1.0f, 0.3f, 0.0f, 1.0f);  // Orange-red
    fireConfig.endColor = glm::vec4(0.8f, 0.8f, 0.2f, 0.0f);    // Yellow fade
    fireConfig.particlesPerSecond = 150.0f;
    fireConfig.maxParticles = 500;
    fireConfig.useGravity = false;  // Fire rises
    fireConfig.gravity = glm::vec3(0.0f, 2.0f, 0.0f);  // Upward force
    fireConfig.damping = 0.95f;
    
    auto fireEmitter = std::make_shared<ParticleEmitter>(fireConfig);
    Entity fireEntity = scene.CreateEntity("FireParticles");
    auto& fireTransform = scene.GetRegistry().get<Transform>(fireEntity);
    fireTransform.position = fireConfig.position;
    scene.GetRegistry().emplace<ParticleEmitterComponent>(fireEntity, fireEmitter);
    
    // Smoke effect (to the right of model, slightly above ground)
    ParticleEmitterConfig smokeConfig;
    smokeConfig.position = modelBasePos + glm::vec3(2.0f, -3.0f, 0.0f);  // Right side, slightly above ground
    smokeConfig.direction = glm::vec3(0.0f, 1.0f, 0.0f);
    smokeConfig.spread = 30.0f;
    smokeConfig.minSpeed = 0.3f;
    smokeConfig.maxSpeed = 1.0f;
    smokeConfig.minLifetime = 3.0f;
    smokeConfig.maxLifetime = 5.0f;
    smokeConfig.minSize = 0.5f;
    smokeConfig.maxSize = 1.5f;
    smokeConfig.startColor = glm::vec4(0.3f, 0.3f, 0.3f, 0.8f);  // Dark gray
    smokeConfig.endColor = glm::vec4(0.1f, 0.1f, 0.1f, 0.0f);    // Fade to black
    smokeConfig.particlesPerSecond = 50.0f;
    smokeConfig.maxParticles = 300;
    smokeConfig.useGravity = false;
    smokeConfig.gravity = glm::vec3(0.0f, 1.5f, 0.0f);  // Gentle upward drift
    smokeConfig.damping = 0.98f;
    
    auto smokeEmitter = std::make_shared<ParticleEmitter>(smokeConfig);
    Entity smokeEntity = scene.CreateEntity("SmokeParticles");
    auto& smokeTransform = scene.GetRegistry().get<Transform>(smokeEntity);
    smokeTransform.position = smokeConfig.position;
    scene.GetRegistry().emplace<ParticleEmitterComponent>(smokeEntity, smokeEmitter);
    
    // Dust effect (behind model, at ground level)
    ParticleEmitterConfig dustConfig;
    dustConfig.position = modelBasePos + glm::vec3(0.0f, -4.0f, 1.0f);  // Behind model, ground level
    dustConfig.direction = glm::vec3(0.0f, 1.0f, 0.0f);
    dustConfig.spread = 45.0f;
    dustConfig.minSpeed = 0.1f;
    dustConfig.maxSpeed = 0.5f;
    dustConfig.minLifetime = 2.0f;
    dustConfig.maxLifetime = 4.0f;
    dustConfig.minSize = 0.1f;
    dustConfig.maxSize = 0.3f;
    dustConfig.startColor = glm::vec4(0.6f, 0.5f, 0.4f, 0.6f);  // Brownish
    dustConfig.endColor = glm::vec4(0.5f, 0.4f, 0.3f, 0.0f);
    dustConfig.particlesPerSecond = 80.0f;
    dustConfig.maxParticles = 400;
    dustConfig.useGravity = true;
    dustConfig.gravity = glm::vec3(0.0f, -2.0f, 0.0f);  // Gentle downward
    dustConfig.damping = 0.9f;
    
    auto dustEmitter = std::make_shared<ParticleEmitter>(dustConfig);
    Entity dustEntity = scene.CreateEntity("DustParticles");
    auto& dustTransform = scene.GetRegistry().get<Transform>(dustEntity);
    dustTransform.position = dustConfig.position;
    scene.GetRegistry().emplace<ParticleEmitterComponent>(dustEntity, dustEmitter);
    
    OE_INFO("Particle emitters created: Fire, Smoke, Dust");
    
    // Store particle entity references for debug UI (declare static outside function scope)
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
    
    // Debug and lighting settings (UI-controlled)
    static int debugOutputMode = 0; // 0=Lighting,1=Albedo,2=Normal,3=Position,4=PBR
    
    // Post-processing UI controls (declared here so they're accessible in render loop)
    static bool uiEnableBloom = true;
    static bool uiEnableBlur = false;
    static bool uiEnableChromaticAberration = false;
    static bool uiEnableFXAA = true;
    static bool uiEnableSSAO = false;
    static bool uiEnableSSR = false;
    static bool uiEnableMotionBlur = false;
    static bool uiEnableVignette = false;
    static bool uiEnableFilmGrain = false;
    static float uiBlurIntensity = 1.0f;
    static int uiBlurIterations = 3;
    static float uiChromaticAberrationIntensity = 0.002f; // Default, can go up to 0.05
    static float uiSSAORadius = 0.5f;
    static float uiSSAOBias = 0.025f;
    static int uiSSAOSamples = 16;
    static float uiSSRStepSize = 0.1f;
    static int uiSSRMaxSteps = 32;
    static float uiSSRThickness = 0.01f;
    static float uiMotionBlurIntensity = 1.0f;
    static int uiMotionBlurSamples = 8;
    static float uiVignetteIntensity = 0.5f;
    static float uiVignetteRadius = 0.75f;
    static float uiVignetteSoftness = 0.5f;
    static float uiFilmGrainIntensity = 0.1f;
    static glm::vec3 uiLightDir = glm::normalize(glm::vec3(0.5f, 1.0f, 0.3f));
    static glm::vec3 uiLightColor = glm::vec3(1.0f, 0.95f, 0.9f);
    static float uiLightIntensity = 3.0f;
    static glm::vec3 uiAmbient = glm::vec3(0.08f);
    static float uiExposure = 1.0f;
    // Render toggles
    static bool uiEnableCulling = true;
    static bool uiEnableLOD = true;
    static bool uiEnableInstancing = false;
    static int uiMinInstancesForBatch = 3;
    
    // GBuffer & ShadowMap (deferred path resources)
    static RenderPipeline renderPipeline;
    static bool pipelineInitialized = false;
    
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
        
        // Update Physics System (syncs Transform <-> PhysicsBody and steps simulation)
        PhysicsSystem::Update(scene, physicsWorld.get(), deltaTime);
        
        // Update Animations (must be called after physics, before rendering)
        OpenEye::Animation::AnimationSystem::Update(scene, deltaTime);
        
        // Update Particle System
        ParticleSystem::Update(scene, deltaTime);
        
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
        static const bool useDeferred = true;  // Deferred pipeline enabled
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
            // Deferred rendering pipeline (centralized)
            if (!pipelineInitialized) {
                renderPipeline.Initialize(static_cast<uint32_t>(window.GetWidth()), static_cast<uint32_t>(window.GetHeight()));
                
                // Set sky color (bright greyish)
                renderPipeline.SetSkyColor(glm::vec3(0.5f, 0.7f, 0.8f));
                
                // Create procedural environment cubemap for IBL (solid color matching sky)
                glm::vec3 skyColor = renderPipeline.GetSkyColor();
                if (auto* envCubemap = renderPipeline.GetSkyboxCubemap()) {
                    if (envCubemap->CreateSolidColor(skyColor.r, skyColor.g, skyColor.b, 256)) {
                        OE_INFO("Procedural environment cubemap created for IBL");
                        // Initialize IBL from procedural environment
                        if (auto* ibl = renderPipeline.GetIBL()) {
                            GLuint envTexID = envCubemap->GetID();
                            if (!ibl->Initialize(envTexID, 32, 512)) {
                                OE_WARN("Failed to initialize IBL from procedural environment");
                            } else {
                                OE_INFO("IBL initialized from procedural environment");
                            }
                        }
                    } else {
                        OE_WARN("Failed to create procedural environment cubemap.");
                    }
                }
                pipelineInitialized = true;
            }
            renderPipeline.Resize(static_cast<uint32_t>(window.GetWidth()), static_cast<uint32_t>(window.GetHeight()));

            LightingParams params;
            params.debugMode = debugOutputMode;
            params.lightDir = uiLightDir;
            params.lightColor = uiLightColor;
            params.lightIntensity = uiLightIntensity;
            params.ambient = uiAmbient;
            params.exposure = uiExposure;
            
            RenderOptions renderOptions;
            renderOptions.enableCulling = uiEnableCulling;
            renderOptions.enableLOD = uiEnableLOD;
            renderOptions.enableInstancing = uiEnableInstancing;
            renderOptions.minInstancesForBatch = uiMinInstancesForBatch;
            renderOptions.enableBloom = uiEnableBloom;
            renderOptions.enableBlur = uiEnableBlur;
            renderOptions.enableChromaticAberration = uiEnableChromaticAberration;
            renderOptions.enableFXAA = uiEnableFXAA;
            renderOptions.blurIntensity = uiBlurIntensity;
            renderOptions.blurIterations = uiBlurIterations;
            renderOptions.chromaticAberrationIntensity = uiChromaticAberrationIntensity;
            renderOptions.enableSSAO = uiEnableSSAO;
            renderOptions.ssaoRadius = uiSSAORadius;
            renderOptions.ssaoBias = uiSSAOBias;
            renderOptions.ssaoSamples = uiSSAOSamples;
            renderOptions.enableSSR = uiEnableSSR;
            renderOptions.ssrStepSize = uiSSRStepSize;
            renderOptions.ssrMaxSteps = uiSSRMaxSteps;
            renderOptions.ssrThickness = uiSSRThickness;
            renderOptions.enableMotionBlur = uiEnableMotionBlur;
            renderOptions.motionBlurIntensity = uiMotionBlurIntensity;
            renderOptions.motionBlurSamples = uiMotionBlurSamples;
            renderOptions.enableVignette = uiEnableVignette;
            renderOptions.vignetteIntensity = uiVignetteIntensity;
            renderOptions.vignetteRadius = uiVignetteRadius;
            renderOptions.vignetteSoftness = uiVignetteSoftness;
            renderOptions.enableFilmGrain = uiEnableFilmGrain;
            renderOptions.filmGrainIntensity = uiFilmGrainIntensity;
            
            renderPipeline.RenderFrame(scene, camera, params, renderOptions);
        }
        
        // Render particles (after main rendering, before UI)
        ParticleSystem::Render(scene, camera, camera.GetViewMatrix(), camera.GetProjectionMatrix());
        
        // --- ImGui UI ---
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        // Tools dockspace (floating window) - main scene stays untouched beneath
        static bool openToolsDock = true;
        ImGui::SetNextWindowSize(ImVec2(520, 400), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Tools", &openToolsDock, ImGuiWindowFlags_MenuBar)) {
            ImGuiID tools_dockspace = ImGui::GetID("ToolsDockSpace");
            ImGui::DockSpace(tools_dockspace, ImVec2(0.0f, 0.0f));
            
            // Renderer Debug docks here on first open
            ImGui::SetNextWindowDockID(tools_dockspace, ImGuiCond_FirstUseEver);
        }
        ImGui::End();
        
        // Renderer Debug panel (docked into Tools window)
        ImGui::Begin("Renderer Debug");
        const char* modes[] = {"Lighting", "Albedo", "Normal", "Position", "PBR"};
        ImGui::Combo("Output", &debugOutputMode, modes, IM_ARRAYSIZE(modes));
        ImGui::Separator();
        ImGui::Text("Render Options");
        ImGui::Checkbox("Frustum Culling", &uiEnableCulling);
        ImGui::Checkbox("LOD", &uiEnableLOD);
        ImGui::Checkbox("Instancing (deferred)", &uiEnableInstancing);
        ImGui::DragInt("Min Instances for Batch", &uiMinInstancesForBatch, 1, 1, 1024);
        ImGui::Separator();
        ImGui::Text("Lighting");
        ImGui::DragFloat3("Light Dir", &uiLightDir.x, 0.01f, -1.0f, 1.0f);
        if (glm::length(uiLightDir) < 0.001f) uiLightDir = glm::vec3(0.0f,1.0f,0.0f);
        ImGui::ColorEdit3("Light Color", &uiLightColor.x);
        ImGui::DragFloat("Light Intensity", &uiLightIntensity, 0.05f, 0.0f, 50.0f);
        ImGui::DragFloat3("Ambient", &uiAmbient.x, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat("Exposure", &uiExposure, 0.05f, 0.01f, 5.0f);
        ImGui::Separator();
        ImGui::Text("Post-Processing");
        ImGui::Checkbox("Bloom", &uiEnableBloom);
        ImGui::Checkbox("Blur", &uiEnableBlur);
        if (uiEnableBlur) {
            ImGui::Indent();
            ImGui::DragFloat("Blur Intensity", &uiBlurIntensity, 0.1f, 0.1f, 5.0f);
            ImGui::DragInt("Blur Iterations", &uiBlurIterations, 1, 1, 10);
            ImGui::Unindent();
        }
        ImGui::Checkbox("Chromatic Aberration", &uiEnableChromaticAberration);
        if (uiEnableChromaticAberration) {
            ImGui::Indent();
            ImGui::DragFloat("CA Intensity", &uiChromaticAberrationIntensity, 0.001f, 0.0f, 0.05f, "%.4f");
            ImGui::Unindent();
        }
        ImGui::Separator();
        ImGui::Text("Advanced Effects");
        ImGui::Checkbox("SSAO", &uiEnableSSAO);
        if (uiEnableSSAO) {
            ImGui::Indent();
            ImGui::DragFloat("SSAO Radius", &uiSSAORadius, 0.01f, 0.1f, 2.0f);
            ImGui::DragFloat("SSAO Bias", &uiSSAOBias, 0.001f, 0.001f, 0.1f);
            ImGui::DragInt("SSAO Samples", &uiSSAOSamples, 1, 8, 32);
            ImGui::Unindent();
        }
        ImGui::Checkbox("SSR", &uiEnableSSR);
        if (uiEnableSSR) {
            ImGui::Indent();
            ImGui::DragFloat("SSR Step Size", &uiSSRStepSize, 0.01f, 0.01f, 0.5f);
            ImGui::DragInt("SSR Max Steps", &uiSSRMaxSteps, 1, 8, 64);
            ImGui::DragFloat("SSR Thickness", &uiSSRThickness, 0.001f, 0.001f, 0.1f);
            ImGui::Unindent();
        }
        ImGui::Checkbox("Motion Blur", &uiEnableMotionBlur);
        if (uiEnableMotionBlur) {
            ImGui::Indent();
            ImGui::DragFloat("Motion Blur Intensity", &uiMotionBlurIntensity, 0.1f, 0.1f, 5.0f);
            ImGui::DragInt("Motion Blur Samples", &uiMotionBlurSamples, 1, 4, 16);
            ImGui::Unindent();
        }
        ImGui::Checkbox("Vignette", &uiEnableVignette);
        if (uiEnableVignette) {
            ImGui::Indent();
            ImGui::DragFloat("Vignette Intensity", &uiVignetteIntensity, 0.01f, 0.0f, 1.0f);
            ImGui::DragFloat("Vignette Radius", &uiVignetteRadius, 0.01f, 0.0f, 1.0f);
            ImGui::DragFloat("Vignette Softness", &uiVignetteSoftness, 0.01f, 0.0f, 1.0f);
            ImGui::Unindent();
        }
        ImGui::Checkbox("Film Grain", &uiEnableFilmGrain);
        if (uiEnableFilmGrain) {
            ImGui::Indent();
            ImGui::DragFloat("Film Grain Intensity", &uiFilmGrainIntensity, 0.01f, 0.0f, 0.5f);
            ImGui::Unindent();
        }
        ImGui::Separator();
        ImGui::Checkbox("FXAA", &uiEnableFXAA);
        if (useDeferred && pipelineInitialized && renderPipeline.GetGBuffer()) {
            ImGui::Separator();
            ImGui::Text("G-Buffer");
            ImGui::Text("Position");
            ImGui::Image((ImTextureID)(intptr_t)renderPipeline.GetGBuffer()->GetPositionTexture(), ImVec2(256, 256), ImVec2(0,1), ImVec2(1,0));
            ImGui::SameLine(); ImGui::Text("Normal");
            ImGui::Image((ImTextureID)(intptr_t)renderPipeline.GetGBuffer()->GetNormalTexture(), ImVec2(256, 256), ImVec2(0,1), ImVec2(1,0));
            ImGui::SameLine(); ImGui::Text("Albedo");
            ImGui::Image((ImTextureID)(intptr_t)renderPipeline.GetGBuffer()->GetAlbedoTexture(), ImVec2(256, 256), ImVec2(0,1), ImVec2(1,0));
        }
        ImGui::End();

        // FrameGraph panel (execution order and timings)
        ImGui::Begin("FrameGraph");
        if (useDeferred && pipelineInitialized) {
            const auto& passes = renderPipeline.GetFrameGraphPasses();
            ImGui::Columns(3, nullptr, false);
            ImGui::Text("Pass"); ImGui::NextColumn();
            ImGui::Text("GPU ms"); ImGui::NextColumn();
            ImGui::Text("Status"); ImGui::NextColumn();
            ImGui::Separator();
            for (const auto& p : passes) {
                ImGui::TextUnformatted(p.name.c_str()); ImGui::NextColumn();
                ImGui::Text("%.3f", p.gpuMs); ImGui::NextColumn();
                ImGui::TextUnformatted(p.culled ? "culled" : "executed"); ImGui::NextColumn();
            }
            ImGui::Columns(1);
        } else {
            ImGui::TextUnformatted("Not available (deferred disabled or pipeline not initialized)");
        }
        ImGui::End();
        
        // Animation Timeline panel
        ImGui::Begin("Animation Timeline");
        auto& registry = scene.GetRegistry();
        auto animView = registry.view<AnimationComponent>();
        
        if (animView.empty()) {
            ImGui::Text("No animated entities in scene");
            ImGui::Text("Load an FBX model with animation to use this panel");
        } else {
            // Get first animated entity (will be used as the controller)
            entt::entity animEntity = *animView.begin();
            auto& animComp = registry.get<AnimationComponent>(animEntity);

            // Helper: propagate current controller state to all components sharing the same skeleton
            auto syncToAllWithSkeleton = [&](const AnimationComponent& source, bool forceRecalc) {
                for (auto e : animView) {
                    if (e == animEntity) continue;
                    auto& other = registry.get<AnimationComponent>(e);
                    if (other.skeleton == source.skeleton) {
                        other.currentClipIndex = source.currentClipIndex;
                        other.currentTime = source.currentTime;
                        other.playbackSpeed = source.playbackSpeed;
                        other.isPlaying = source.isPlaying;
                        other.loop = source.loop;
                        other.boneMatricesDirty = true;
                        if (forceRecalc) {
                            OpenEye::Animation::AnimationSystem::CalculateBoneMatrices(registry, e);
                        }
                    }
                }
            };
            
            if (!animComp.skeleton) {
                ImGui::Text("Entity has AnimationComponent but no skeleton");
            } else {
                auto skeleton = std::static_pointer_cast<OpenEye::Animation::Skeleton>(animComp.skeleton);
                size_t boneCount = skeleton ? skeleton->GetBoneCount() : 0;
                
                // Display skeleton info
                ImGui::Text("Skeleton: %zu bones", boneCount);
                
                // Display available clips
                ImGui::Separator();
                ImGui::Text("Available Clips: %zu", animComp.clips.size());
                
                if (animComp.clips.empty()) {
                    ImGui::Text("No animation clips loaded");
                } else {
                    // Clip selection dropdown
                    static int selectedClipIndex = -1;
                    if (selectedClipIndex == -1 && animComp.currentClipIndex >= 0) {
                        selectedClipIndex = animComp.currentClipIndex;
                    }
                    
                    // Build clip names list
                    std::vector<std::string> clipNames;
                    for (size_t i = 0; i < animComp.clips.size(); i++) {
                        auto clip = std::static_pointer_cast<OpenEye::Animation::AnimationClip>(animComp.clips[i]);
                        if (clip) {
                            clipNames.push_back("Clip " + std::to_string(i) + " (" + std::to_string(clip->GetDuration()) + "s)");
                        } else {
                            clipNames.push_back("Clip " + std::to_string(i));
                        }
                    }
                    
                    std::vector<const char*> clipNamesPtrs;
                    for (const auto& name : clipNames) {
                        clipNamesPtrs.push_back(name.c_str());
                    }
                    
                    if (!clipNamesPtrs.empty()) {
                        int oldSelected = selectedClipIndex;
                        ImGui::Combo("Select Clip", &selectedClipIndex, clipNamesPtrs.data(), static_cast<int>(clipNamesPtrs.size()));
                        if (oldSelected != selectedClipIndex && selectedClipIndex >= 0 && selectedClipIndex < static_cast<int>(animComp.clips.size())) {
                            // Apply selection to all components sharing the same skeleton
                            for (auto e : animView) {
                                auto& ac = registry.get<AnimationComponent>(e);
                                if (ac.skeleton == animComp.skeleton) {
                                    OpenEye::Animation::AnimationSystem::PlayAnimation(registry, e, selectedClipIndex, 0.2f);
                                }
                            }
                        }
                    }
                    
                    // Current clip info
                    if (animComp.currentClipIndex >= 0 && animComp.currentClipIndex < static_cast<int>(animComp.clips.size())) {
                        auto currentClip = std::static_pointer_cast<OpenEye::Animation::AnimationClip>(animComp.clips[animComp.currentClipIndex]);
                        if (currentClip) {
                            float duration = currentClip->GetDuration();
                            
                            ImGui::Separator();
                            ImGui::Text("Current Clip: %d", animComp.currentClipIndex);
                            ImGui::Text("Duration: %.2f seconds", duration);
                            
                            // Playback controls
                            ImGui::Separator();
                            if (ImGui::Button(animComp.isPlaying ? "Pause" : "Play")) {
                                animComp.isPlaying = !animComp.isPlaying;
                                syncToAllWithSkeleton(animComp, false);
                            }
                            ImGui::SameLine();
                            if (ImGui::Button("Stop")) {
                                // Stop all sharing the same skeleton
                                for (auto e : animView) {
                                    auto& ac = registry.get<AnimationComponent>(e);
                                    if (ac.skeleton == animComp.skeleton) {
                                        OpenEye::Animation::AnimationSystem::StopAnimation(registry, e);
                                    }
                                }
                                selectedClipIndex = -1;
                            }
                            ImGui::SameLine();
                            {
                                bool loopChanged = ImGui::Checkbox("Loop", &animComp.loop);
                                if (loopChanged) {
                                    syncToAllWithSkeleton(animComp, false);
                                }
                            }
                            
                            // Timeline scrubbing
                            ImGui::Separator();
                            float currentTime = animComp.currentTime;
                            if (ImGui::SliderFloat("Time", &currentTime, 0.0f, duration, "%.2f s")) {
                                animComp.currentTime = currentTime;
                                animComp.boneMatricesDirty = true;
                                // Force update bone matrices for all sharing the same skeleton
                                syncToAllWithSkeleton(animComp, true);
                                OpenEye::Animation::AnimationSystem::CalculateBoneMatrices(registry, animEntity);
                            }
                            
                            // Time display
                            ImGui::Text("%.2f / %.2f seconds", currentTime, duration);
                            float progress = duration > 0.0f ? (currentTime / duration) : 0.0f;
                            ImGui::ProgressBar(progress, ImVec2(0.0f, 0.0f));
                            
                            // Playback speed
                            ImGui::Separator();
                            if (ImGui::DragFloat("Playback Speed", &animComp.playbackSpeed, 0.1f, 0.0f, 5.0f, "%.1fx")) {
                                syncToAllWithSkeleton(animComp, false);
                            }
                            
                            // Blend info
                            if (animComp.blendFromClipIndex >= 0) {
                                ImGui::Separator();
                                ImGui::Text("Blending from clip %d", animComp.blendFromClipIndex);
                                ImGui::Text("Blend progress: %.2f / %.2f", animComp.blendTime, animComp.blendDuration);
                            }
                        }
                    } else {
                        ImGui::Text("No clip selected");
                    }
                }
            }
        }
        ImGui::End();
        
        // Particle System Debug panel
        ImGui::Begin("Particle System");
        {
            auto& particleRegistry = scene.GetRegistry();
            
            // Find particle entities by name
            Entity fireParticleEntity = entt::null;
            Entity smokeParticleEntity = entt::null;
            Entity dustParticleEntity = entt::null;
            
            auto nameView = particleRegistry.view<Name>();
            for (auto e : nameView) {
                const auto& name = particleRegistry.get<Name>(e);
                if (name.name == "FireParticles") fireParticleEntity = e;
                else if (name.name == "SmokeParticles") smokeParticleEntity = e;
                else if (name.name == "DustParticles") dustParticleEntity = e;
            }
            
            // Fire Particles
            if (fireParticleEntity != entt::null && particleRegistry.all_of<ParticleEmitterComponent>(fireParticleEntity)) {
                auto& fireComp = particleRegistry.get<ParticleEmitterComponent>(fireParticleEntity);
                if (fireComp.emitter) {
                    ImGui::PushID("Fire");
                    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Fire Particles");
                    ImGui::Separator();
                    
                    auto& fireParticleConfig = fireComp.emitter->GetConfig();
                    auto& fireParticleTransform = particleRegistry.get<Transform>(fireParticleEntity);
                    
                    // Position
                    ImGui::DragFloat3("Position", &fireParticleTransform.position.x, 0.1f);
                    fireComp.emitter->SetPosition(fireParticleTransform.position);
                    
                    // Direction
                    ImGui::DragFloat3("Direction", &fireParticleConfig.direction.x, 0.01f);
                    
                    // Spread
                    ImGui::DragFloat("Spread (deg)", &fireParticleConfig.spread, 1.0f, 0.0f, 180.0f);
                    
                    // Speed
                    ImGui::DragFloatRange2("Speed", &fireParticleConfig.minSpeed, &fireParticleConfig.maxSpeed, 0.1f, 0.0f, 10.0f);
                    
                    // Lifetime
                    ImGui::DragFloatRange2("Lifetime", &fireParticleConfig.minLifetime, &fireParticleConfig.maxLifetime, 0.1f, 0.0f, 10.0f);
                    
                    // Size
                    ImGui::DragFloatRange2("Size", &fireParticleConfig.minSize, &fireParticleConfig.maxSize, 0.01f, 0.0f, 5.0f);
                    
                    // Colors
                    ImGui::ColorEdit4("Start Color", &fireParticleConfig.startColor.x);
                    ImGui::ColorEdit4("End Color", &fireParticleConfig.endColor.x);
                    
                    // Emission
                    ImGui::DragFloat("Particles/Second", &fireParticleConfig.particlesPerSecond, 1.0f, 0.0f, 1000.0f);
                    ImGui::DragInt("Max Particles", &fireParticleConfig.maxParticles, 1, 0, 10000);
                    
                    // Physics
                    ImGui::Checkbox("Use Gravity", &fireParticleConfig.useGravity);
                    ImGui::DragFloat3("Gravity", &fireParticleConfig.gravity.x, 0.1f);
                    ImGui::DragFloat("Damping", &fireParticleConfig.damping, 0.01f, 0.0f, 1.0f);
                    
                    // Control
                    bool isPlaying = fireComp.emitter->IsPlaying();
                    if (ImGui::Button(isPlaying ? "Pause" : "Play")) {
                        if (isPlaying) fireComp.emitter->Stop();
                        else fireComp.emitter->Start();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Reset")) {
                        fireComp.emitter->Reset();
                    }
                    
                    // Stats
                    ImGui::Separator();
                    ImGui::Text("Active Particles: %zu / %d", 
                               fireComp.emitter->GetActiveParticleCount(), 
                               fireParticleConfig.maxParticles);
                    
                    ImGui::PopID();
                }
            }
            
            ImGui::Separator();
            
            // Smoke Particles
            if (smokeParticleEntity != entt::null && particleRegistry.all_of<ParticleEmitterComponent>(smokeParticleEntity)) {
                auto& smokeComp = particleRegistry.get<ParticleEmitterComponent>(smokeParticleEntity);
                if (smokeComp.emitter) {
                    ImGui::PushID("Smoke");
                    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Smoke Particles");
                    ImGui::Separator();
                    
                    auto& smokeParticleConfig = smokeComp.emitter->GetConfig();
                    auto& smokeParticleTransform = particleRegistry.get<Transform>(smokeParticleEntity);
                    
                    // Position
                    ImGui::DragFloat3("Position", &smokeParticleTransform.position.x, 0.1f);
                    smokeComp.emitter->SetPosition(smokeParticleTransform.position);
                    
                    // Direction
                    ImGui::DragFloat3("Direction", &smokeParticleConfig.direction.x, 0.01f);
                    
                    // Spread
                    ImGui::DragFloat("Spread (deg)", &smokeParticleConfig.spread, 1.0f, 0.0f, 180.0f);
                    
                    // Speed
                    ImGui::DragFloatRange2("Speed", &smokeParticleConfig.minSpeed, &smokeParticleConfig.maxSpeed, 0.1f, 0.0f, 10.0f);
                    
                    // Lifetime
                    ImGui::DragFloatRange2("Lifetime", &smokeParticleConfig.minLifetime, &smokeParticleConfig.maxLifetime, 0.1f, 0.0f, 10.0f);
                    
                    // Size
                    ImGui::DragFloatRange2("Size", &smokeParticleConfig.minSize, &smokeParticleConfig.maxSize, 0.01f, 0.0f, 5.0f);
                    
                    // Colors
                    ImGui::ColorEdit4("Start Color", &smokeParticleConfig.startColor.x);
                    ImGui::ColorEdit4("End Color", &smokeParticleConfig.endColor.x);
                    
                    // Emission
                    ImGui::DragFloat("Particles/Second", &smokeParticleConfig.particlesPerSecond, 1.0f, 0.0f, 1000.0f);
                    ImGui::DragInt("Max Particles", &smokeParticleConfig.maxParticles, 1, 0, 10000);
                    
                    // Physics
                    ImGui::Checkbox("Use Gravity", &smokeParticleConfig.useGravity);
                    ImGui::DragFloat3("Gravity", &smokeParticleConfig.gravity.x, 0.1f);
                    ImGui::DragFloat("Damping", &smokeParticleConfig.damping, 0.01f, 0.0f, 1.0f);
                    
                    // Control
                    bool isPlaying = smokeComp.emitter->IsPlaying();
                    if (ImGui::Button(isPlaying ? "Pause" : "Play")) {
                        if (isPlaying) smokeComp.emitter->Stop();
                        else smokeComp.emitter->Start();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Reset")) {
                        smokeComp.emitter->Reset();
                    }
                    
                    // Stats
                    ImGui::Separator();
                    ImGui::Text("Active Particles: %zu / %d", 
                               smokeComp.emitter->GetActiveParticleCount(), 
                               smokeParticleConfig.maxParticles);
                    
                    ImGui::PopID();
                }
            }
            
            ImGui::Separator();
            
            // Dust Particles
            if (dustParticleEntity != entt::null && particleRegistry.all_of<ParticleEmitterComponent>(dustParticleEntity)) {
                auto& dustComp = particleRegistry.get<ParticleEmitterComponent>(dustParticleEntity);
                if (dustComp.emitter) {
                    ImGui::PushID("Dust");
                    ImGui::TextColored(ImVec4(0.6f, 0.5f, 0.4f, 1.0f), "Dust Particles");
                    ImGui::Separator();
                    
                    auto& dustParticleConfig = dustComp.emitter->GetConfig();
                    auto& dustParticleTransform = particleRegistry.get<Transform>(dustParticleEntity);
                    
                    // Position
                    ImGui::DragFloat3("Position", &dustParticleTransform.position.x, 0.1f);
                    dustComp.emitter->SetPosition(dustParticleTransform.position);
                    
                    // Direction
                    ImGui::DragFloat3("Direction", &dustParticleConfig.direction.x, 0.01f);
                    
                    // Spread
                    ImGui::DragFloat("Spread (deg)", &dustParticleConfig.spread, 1.0f, 0.0f, 180.0f);
                    
                    // Speed
                    ImGui::DragFloatRange2("Speed", &dustParticleConfig.minSpeed, &dustParticleConfig.maxSpeed, 0.1f, 0.0f, 10.0f);
                    
                    // Lifetime
                    ImGui::DragFloatRange2("Lifetime", &dustParticleConfig.minLifetime, &dustParticleConfig.maxLifetime, 0.1f, 0.0f, 10.0f);
                    
                    // Size
                    ImGui::DragFloatRange2("Size", &dustParticleConfig.minSize, &dustParticleConfig.maxSize, 0.01f, 0.0f, 5.0f);
                    
                    // Colors
                    ImGui::ColorEdit4("Start Color", &dustParticleConfig.startColor.x);
                    ImGui::ColorEdit4("End Color", &dustParticleConfig.endColor.x);
                    
                    // Emission
                    ImGui::DragFloat("Particles/Second", &dustParticleConfig.particlesPerSecond, 1.0f, 0.0f, 1000.0f);
                    ImGui::DragInt("Max Particles", &dustParticleConfig.maxParticles, 1, 0, 10000);
                    
                    // Physics
                    ImGui::Checkbox("Use Gravity", &dustParticleConfig.useGravity);
                    ImGui::DragFloat3("Gravity", &dustParticleConfig.gravity.x, 0.1f);
                    ImGui::DragFloat("Damping", &dustParticleConfig.damping, 0.01f, 0.0f, 1.0f);
                    
                    // Control
                    bool isPlaying = dustComp.emitter->IsPlaying();
                    if (ImGui::Button(isPlaying ? "Pause" : "Play")) {
                        if (isPlaying) dustComp.emitter->Stop();
                        else dustComp.emitter->Start();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Reset")) {
                        dustComp.emitter->Reset();
                    }
                    
                    // Stats
                    ImGui::Separator();
                    ImGui::Text("Active Particles: %zu / %d", 
                               dustComp.emitter->GetActiveParticleCount(), 
                               dustParticleConfig.maxParticles);
                    
                    ImGui::PopID();
                }
            }
        }
        ImGui::End();
        
        // Render ImGui
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            GLFWwindow* backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }
        
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
    
    // Shutdown Physics World
    if (physicsWorld) {
        physicsWorld->Shutdown();
        physicsWorld.reset();
        OE_INFO("Physics World shut down");
    }
    
    // Shutdown ImGui
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    window.Shutdown();
    Log::Shutdown();
    
    return 0;
}

