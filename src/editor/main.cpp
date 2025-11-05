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
#include <btBulletDynamicsCommon.h>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <filesystem>
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "animation/AnimationLoader.h"

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
    
    // Create a few more test cubes with physics (so they don't pass through ground)
    for (int i = 0; i < 3; i++) {
        Entity testCube = scene.CreateEntity("TestCube_" + std::to_string(i));
        auto& transform = scene.GetRegistry().get<Transform>(testCube);
        transform.position = glm::vec3((i - 1) * 2.0f, 2.0f, -2.0f); // Position above ground
        
        auto testMaterial = std::make_shared<Material>(shader);
        float hue = i / 3.0f;
        testMaterial->SetAlbedo(glm::vec3(
            0.5f + 0.5f * cos(hue * 6.28f),
            0.5f + 0.5f * cos((hue + 0.33f) * 6.28f),
            0.5f + 0.5f * cos((hue + 0.66f) * 6.28f)
        ));
        
        scene.GetRegistry().emplace<Renderable>(testCube, mesh, testMaterial);
        scene.GetRegistry().emplace<AABB>(testCube);
        
        // Add physics body to test cubes (cube shape, 1 unit size)
        auto boxShape = physicsWorld->CreateBoxShape(glm::vec3(0.5f, 0.5f, 0.5f)); // Half-extents for 1x1x1 cube
        if (boxShape) {
            PhysicsSystem::AttachPhysicsBody(scene, testCube, physicsWorld.get(), boxShape, 1.0f, false);
        }
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
    
    // Load character model (Kick To The Groin.fbx) - this is the main animated character
    Entity characterEntity = entt::null;
    std::string characterFbxPath = resolveAssetPath("assets/models/Kick To The Groin.fbx");
    std::string modelPath = "";
    
    if (std::filesystem::exists(characterFbxPath)) {
        modelPath = characterFbxPath;
        OE_INFO("Found character FBX model: " + characterFbxPath);
    } else {
        // Fallback to glTF model
        modelPath = resolveAssetPath("assets/models/dess_holiday_hb_juan_deltarune/scene.gltf");
        OE_INFO("FBX model not found, trying glTF: " + modelPath);
    }
    
    // Store animation clips for keypress mapping (m/n/o/p)
    std::vector<std::pair<int, std::string>> keyAnimationMap; // key code -> clip index in character's AnimationComponent
    int idleClipIndex = 0; // Default idle animation (first clip from character model)
    size_t additionalClipsCount = 0; // Count of additional clips loaded (for mapping calculation)
    
    if (std::filesystem::exists(modelPath)) {
        OE_INFO("Loading character model: " + modelPath);
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
                if (modelMaterial) {
                    auto gbufferShader = std::make_shared<Shader>();
                    if (hasAnimation) {
                        gbufferShader->LoadFromFile("assets/shaders/passes/deferred/gbuffer_skinned.vert", 
                                                    "assets/shaders/passes/deferred/gbuffer_skinned.frag");
                    } else {
                        gbufferShader->LoadFromFile("assets/shaders/passes/deferred/gbuffer.vert", 
                                                    "assets/shaders/passes/deferred/gbuffer.frag");
                    }
                    modelMaterial->SetShader(gbufferShader);
                }
                
                scene.GetRegistry().emplace<Renderable>(modelEntity, loadedMesh->mesh, modelMaterial);
                scene.GetRegistry().emplace<AABB>(modelEntity);
                
                // Attach AnimationComponent if model has skeleton/animation
                if (hasAnimation) {
                    characterEntity = modelEntity; // Store reference to character entity
                    
                    AnimationComponent animComp;
                    animComp.skeleton = std::static_pointer_cast<void>(loadedMesh->skeleton);
                    
                    // Convert AnimationClip pointers to void pointers for storage
                    animComp.clips.clear();
                    animComp.clips.reserve(loadedMesh->animations.size());
                    for (const auto& clip : loadedMesh->animations) {
                        animComp.clips.push_back(std::static_pointer_cast<void>(clip));
                    }
                    
                    // Start with idle (first clip, looping)
                    animComp.currentClipIndex = 0;
                    animComp.isPlaying = true;
                    animComp.loop = true;
                    animComp.playbackSpeed = 1.0f;
                    animComp.boneMatricesDirty = true;
                    
                    scene.GetRegistry().emplace<AnimationComponent>(modelEntity, animComp);
                    
                    // Initialize bone matrices immediately
                    auto& registry = scene.GetRegistry();
                    OpenEye::Animation::AnimationSystem::CalculateBoneMatrices(registry, modelEntity);
                    
                    OE_INFO("  - Animation component attached (skeleton: " + std::to_string(loadedMesh->skeleton->GetBoneCount()) + " bones, " + std::to_string(loadedMesh->animations.size()) + " clips)");
                    OE_INFO("  - Starting with idle animation (clip 0)");
                }
                
                // Log loaded textures for debugging
                OE_INFO("Model entity '" + loadedMesh->name + "' created");
                
                // Attach physics body to model (using convex hull)
                if (loadedMesh->mesh) {
                    auto collisionShape = physicsWorld->CreateConvexHullShape(loadedMesh->mesh);
                    if (collisionShape) {
                        modelTransform.position = glm::vec3(0.0f, 2.0f, -5.0f); // Position above ground
                        modelTransform.scale = glm::vec3(0.1f);
                        if (PhysicsSystem::AttachPhysicsBody(scene, modelEntity, physicsWorld.get(), collisionShape, 1.0f, false)) {
                            OE_INFO("  - Physics body attached (dynamic, mass=1.0)");
                        }
                    }
                }
            }
        } else {
            OE_WARN("Failed to load model from: " + modelPath);
        }
    } else {
        OE_WARN("Model file not found: " + modelPath);
    }
    
    // Load additional animation files (Dying.fbx) and collect their clips
    // IMPORTANT: Extract animations using the character's skeleton, not Dying.fbx's skeleton
    std::string dyingFbxPath = resolveAssetPath("assets/models/Dying.fbx");
    std::vector<std::shared_ptr<OpenEye::Animation::AnimationClip>> additionalClips;
    
    // Get character skeleton to use for extracting animations from Dying.fbx
    std::shared_ptr<OpenEye::Animation::Skeleton> characterSkeleton = nullptr;
    if (characterEntity != entt::null) {
        auto& animComp = scene.GetRegistry().get<AnimationComponent>(characterEntity);
        if (animComp.skeleton) {
            characterSkeleton = std::static_pointer_cast<OpenEye::Animation::Skeleton>(animComp.skeleton);
            OE_INFO("Using character skeleton to extract animations from Dying.fbx");
        }
    }
    
    if (std::filesystem::exists(dyingFbxPath) && characterSkeleton) {
        OE_INFO("Loading additional animation file: " + dyingFbxPath);
        
        // Load the Assimp scene directly to extract animations
        Assimp::Importer importer;
        const aiScene* dyingScene = importer.ReadFile(
            dyingFbxPath,
            aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipUVs | aiProcess_CalcTangentSpace
        );
        
        if (dyingScene && dyingScene->HasAnimations()) {
            OE_INFO("  - Found " + std::to_string(dyingScene->mNumAnimations) + " animations in Dying.fbx");
            
            // Extract animations using character's skeleton (not Dying.fbx's skeleton)
            auto dyingClips = OpenEye::Animation::AnimationLoader::ExtractAnimations(dyingScene, *characterSkeleton);
            
            for (const auto& clip : dyingClips) {
                // Check if clip has any channels (only add if it has valid animation data)
                size_t channelCount = clip->GetChannelCount();
                if (channelCount > 0) {
                    additionalClips.push_back(clip);
                    OE_INFO("    - Extracted animation clip: " + clip->GetName() + 
                           " (duration: " + std::to_string(clip->GetDuration()) + "s, " + 
                           std::to_string(channelCount) + " channels)");
                } else {
                    OE_WARN("    - Skipping clip '" + clip->GetName() + "': no valid channels (0 channels)");
                }
            }
            
            OE_INFO("  - Successfully extracted " + std::to_string(additionalClips.size()) + " valid clips (out of " + std::to_string(dyingClips.size()) + " total) using character skeleton");
        } else {
            OE_WARN("  - Dying.fbx has no animations or failed to load");
        }
    } else {
        if (!std::filesystem::exists(dyingFbxPath)) {
            OE_WARN("Dying.fbx file not found at: " + dyingFbxPath);
        }
        if (!characterSkeleton) {
            OE_WARN("Cannot extract animations: character skeleton not available");
        }
    }
    
    additionalClipsCount = additionalClips.size();
    OE_INFO("Total additional clips collected: " + std::to_string(additionalClipsCount));
    
    // Merge Dying.fbx clips into existing list for all entities sharing the Kick skeleton
    // This keeps original Kick clips and adds Dying clips, so both appear in the dropdown
    if (characterEntity != entt::null) {
        auto& registry = scene.GetRegistry();
        auto& animCompRef = registry.get<AnimationComponent>(characterEntity);
        if (!additionalClips.empty() && animCompRef.skeleton) {
            auto targetSkeleton = animCompRef.skeleton;
            auto animView = registry.view<AnimationComponent>();
            size_t totalAppended = 0;
            for (auto e : animView) {
                auto& ac = registry.get<AnimationComponent>(e);
                if (ac.skeleton == targetSkeleton) {
                    size_t before = ac.clips.size();
                    for (const auto& clip : additionalClips) {
                        ac.clips.push_back(std::static_pointer_cast<void>(clip));
                    }
                    size_t appended = ac.clips.size() - before;
                    totalAppended += appended;
                    ac.boneMatricesDirty = true;
                    OpenEye::Animation::AnimationSystem::CalculateBoneMatrices(registry, e);
                }
            }
            OE_INFO("Appended Dying.fbx clips to entities sharing the character skeleton (added " + std::to_string(totalAppended) + " clips across entities)");
        } else {
            OE_WARN("No additional clips from Dying.fbx or missing character skeleton; showing existing clips only");
        }
    }
    
    // Create ground plane for physics test
    OE_INFO("Creating ground plane for physics test...");
    // Position ground top surface at y=0.0 (physics will auto-adjust center)
    Entity groundEntity = PhysicsSystem::CreateGroundPlane(
        scene,
        physicsWorld.get(),
        glm::vec3(0.0f, 0.0f, 0.0f),  // Top surface position (physics center will be adjusted)
        glm::vec2(50.0f, 50.0f)         // 50x50 ground plane
    );
    OE_INFO("Ground plane created successfully (top surface at y=0.0)");
    
    // Count entities
    size_t entityCount = scene.GetRegistry().view<Transform>().size();
    OE_INFO_F("Created %zu entities in scene", entityCount);

    // Create a bouncing ball with physics
    {
        OE_INFO("Creating bouncing ball with physics...");
        Entity ballEntity = scene.CreateEntity("BouncingBall");
        auto& ballTransform = scene.GetRegistry().get<Transform>(ballEntity);
        ballTransform.position = glm::vec3(0.0f, 5.0f, 0.0f); // Start 5 units above ground
        ballTransform.scale = glm::vec3(1.0f); // Scale 1.0 (sphere mesh is already 0.5 radius)
        
        // Create sphere mesh for rendering
        auto sphereMesh = MeshLoader::CreateSphere(32);
        auto ballMaterial = std::make_shared<Material>(shader);
        ballMaterial->SetAlbedo(glm::vec3(0.2f, 0.6f, 1.0f)); // Blue color
        ballMaterial->SetMetallic(0.1f);
        ballMaterial->SetRoughness(0.3f);
        
        scene.GetRegistry().emplace<Renderable>(ballEntity, sphereMesh, ballMaterial);
        scene.GetRegistry().emplace<AABB>(ballEntity);
        
        // Create physics sphere shape (radius = 0.5 units)
        // The sphere mesh from CreateSphere(32) has radius 0.5 by default
        // Since scale is 1.0, physics radius matches visual radius
        auto sphereShape = physicsWorld->CreateSphereShape(0.5f);
        if (sphereShape) {
            // Attach dynamic rigid body (mass > 0 makes it dynamic/bouncing)
            if (PhysicsSystem::AttachPhysicsBody(scene, ballEntity, physicsWorld.get(), sphereShape, 1.0f, false)) {
                // Get the physics body and set restitution (bounciness) for bouncing
                auto& physicsBody = scene.GetRegistry().get<PhysicsBody>(ballEntity);
                if (physicsBody.rigidBody) {
                    // Set restitution BEFORE adding to world (restitution is used during collision response)
                    physicsBody.rigidBody->setRestitution(0.9f); // 0.9 = very bouncy
                    physicsBody.rigidBody->setFriction(0.7f);  // Higher friction to prevent sliding
                    // Ensure the rigid body is active and awake
                    physicsBody.rigidBody->setActivationState(DISABLE_DEACTIVATION); // Keep it always active
                    physicsBody.rigidBody->activate(true); // Activate immediately
                    // Set linear damping to reduce bounciness slightly (more realistic)
                    physicsBody.rigidBody->setDamping(0.02f, 0.8f); // Very low linear damping (0.02) for maximum bouncy
                    // Verify restitution was set
                    float actualRestitution = physicsBody.rigidBody->getRestitution();
                    OE_INFO("  - Bouncing ball created with physics (mass=1.0, radius=0.5, restitution=" + 
                            std::to_string(actualRestitution) + ", active=true)");
                    
                    // Force update the motion state to ensure initial position is correct
                    btTransform initialTransform;
                    initialTransform.setOrigin(btVector3(0.0f, 5.0f, 0.0f));
                    initialTransform.setRotation(btQuaternion::getIdentity());
                    physicsBody.rigidBody->getMotionState()->setWorldTransform(initialTransform);
                    physicsBody.rigidBody->setWorldTransform(initialTransform);
                }
            } else {
                OE_WARN("  - Failed to attach physics body to ball");
            }
        } else {
            OE_WARN("  - Failed to create sphere collision shape");
        }
    }
    
    // Camera setup (using standalone camera for now, will integrate with ECS later)
    Camera camera;
    camera.SetPosition(glm::vec3(0.0f, 0.0f, 5.0f));
    // Ensure aspect ratio is valid (avoid division by zero)
    float aspect = window.GetAspectRatio();
    if (aspect <= 0.0f || !std::isfinite(aspect)) {
        aspect = 16.0f / 9.0f; // Default to 16:9 if invalid
        OE_WARN("Invalid window aspect ratio, using default 16:9");
    }
    camera.SetPerspective(45.0f, aspect, 0.1f, 100.0f);
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
        
        // Update projection if window was resized (with safety check)
        float newAspect = window.GetAspectRatio();
        if (newAspect > 0.0f && std::isfinite(newAspect)) {
            camera.SetPerspective(camera.GetFOV(), newAspect, 0.1f, 100.0f);
        }
        
        // Update ECS systems
        TransformSystem::Update(scene);
        
        // Update Physics System (syncs Transform <-> PhysicsBody and steps simulation)
        PhysicsSystem::Update(scene, physicsWorld.get(), deltaTime);
        
        // Update Animations (must be called after physics, before rendering)
        OpenEye::Animation::AnimationSystem::Update(scene, deltaTime);
        
        // Handle keypress animation triggers (m/n/o/p)
        if (characterEntity != entt::null) {
            auto& animComp = scene.GetRegistry().get<AnimationComponent>(characterEntity);
            
            // Check if animation finished (non-looping animation reached end)
            if (!animComp.loop && animComp.isPlaying && animComp.currentClipIndex >= 0) {
                if (animComp.currentClipIndex < static_cast<int>(animComp.clips.size())) {
                    auto clip = std::static_pointer_cast<OpenEye::Animation::AnimationClip>(animComp.clips[animComp.currentClipIndex]);
                    if (clip && animComp.currentTime >= clip->GetDuration()) {
                        // Animation finished, return to idle
                        animComp.currentClipIndex = idleClipIndex;
                        animComp.currentTime = 0.0f;
                        animComp.loop = true;
                        animComp.isPlaying = true;
                        animComp.boneMatricesDirty = true;
                        OE_INFO("Animation finished, returning to idle");
                    }
                }
            }
            
            // Handle keypresses for animation switching
            static int keyToClipMap[4] = {-1, -1, -1, -1}; // m/n/o/p -> clip indices (set once)
            static bool mapInitialized = false;
            
            if (!mapInitialized && animComp.clips.size() > 0) {
                // Initialize mapping: m=1, n=2, o=first from Dying, p=second from Dying
                // Find where Dying clips start (assume first clips are from Kick, rest from Dying)
                size_t kickClipCount = animComp.clips.size() - additionalClipsCount;
                
                keyToClipMap[0] = (kickClipCount > 1) ? 1 : 0; // M -> second clip from Kick or first
                keyToClipMap[1] = (kickClipCount > 2) ? 2 : ((kickClipCount > 1) ? 1 : 0); // N -> third or second
                keyToClipMap[2] = static_cast<int>(kickClipCount); // O -> first Dying clip
                keyToClipMap[3] = static_cast<int>(kickClipCount + 1 < animComp.clips.size() ? kickClipCount + 1 : kickClipCount); // P -> second Dying clip
                
                mapInitialized = true;
                OE_INFO("Animation key mapping initialized:");
                OE_INFO("  M -> clip " + std::to_string(keyToClipMap[0]));
                OE_INFO("  N -> clip " + std::to_string(keyToClipMap[1]));
                OE_INFO("  O -> clip " + std::to_string(keyToClipMap[2]));
                OE_INFO("  P -> clip " + std::to_string(keyToClipMap[3]));
            }
            
            // Check keypresses
            if (Input::IsKeyJustPressed(GLFW_KEY_M) && keyToClipMap[0] >= 0 && keyToClipMap[0] < static_cast<int>(animComp.clips.size())) {
                auto clip = std::static_pointer_cast<OpenEye::Animation::AnimationClip>(animComp.clips[keyToClipMap[0]]);
                OE_INFO("Playing animation M (clip " + std::to_string(keyToClipMap[0]) + ", name: " + (clip ? clip->GetName() : "unknown") + ")");
                OpenEye::Animation::AnimationSystem::PlayAnimation(scene.GetRegistry(), characterEntity, keyToClipMap[0], 0.2f);
                animComp.loop = false; // Play once, then return to idle
            }
            if (Input::IsKeyJustPressed(GLFW_KEY_N) && keyToClipMap[1] >= 0 && keyToClipMap[1] < static_cast<int>(animComp.clips.size())) {
                auto clip = std::static_pointer_cast<OpenEye::Animation::AnimationClip>(animComp.clips[keyToClipMap[1]]);
                OE_INFO("Playing animation N (clip " + std::to_string(keyToClipMap[1]) + ", name: " + (clip ? clip->GetName() : "unknown") + ")");
                OpenEye::Animation::AnimationSystem::PlayAnimation(scene.GetRegistry(), characterEntity, keyToClipMap[1], 0.2f);
                animComp.loop = false;
            }
            if (Input::IsKeyJustPressed(GLFW_KEY_O) && keyToClipMap[2] >= 0 && keyToClipMap[2] < static_cast<int>(animComp.clips.size())) {
                auto clip = std::static_pointer_cast<OpenEye::Animation::AnimationClip>(animComp.clips[keyToClipMap[2]]);
                if (clip) {
                    size_t channelCount = clip->GetChannelCount();
                    OE_INFO("Playing animation O (clip " + std::to_string(keyToClipMap[2]) + ", name: " + clip->GetName() + 
                           ", channels: " + std::to_string(channelCount) + ", duration: " + std::to_string(clip->GetDuration()) + "s)");
                    if (channelCount == 0) {
                        OE_WARN("  WARNING: Clip has 0 channels - animation may not play!");
                    }
                } else {
                    OE_WARN("Playing animation O (clip " + std::to_string(keyToClipMap[2]) + "): Invalid clip pointer!");
                }
                OpenEye::Animation::AnimationSystem::PlayAnimation(scene.GetRegistry(), characterEntity, keyToClipMap[2], 0.2f);
                animComp.loop = false;
            }
            if (Input::IsKeyJustPressed(GLFW_KEY_P) && keyToClipMap[3] >= 0 && keyToClipMap[3] < static_cast<int>(animComp.clips.size())) {
                auto clip = std::static_pointer_cast<OpenEye::Animation::AnimationClip>(animComp.clips[keyToClipMap[3]]);
                if (clip) {
                    size_t channelCount = clip->GetChannelCount();
                    OE_INFO("Playing animation P (clip " + std::to_string(keyToClipMap[3]) + ", name: " + clip->GetName() + 
                           ", channels: " + std::to_string(channelCount) + ", duration: " + std::to_string(clip->GetDuration()) + "s)");
                    if (channelCount == 0) {
                        OE_WARN("  WARNING: Clip has 0 channels - animation may not play!");
                    }
                } else {
                    OE_WARN("Playing animation P (clip " + std::to_string(keyToClipMap[3]) + "): Invalid clip pointer!");
                }
                OpenEye::Animation::AnimationSystem::PlayAnimation(scene.GetRegistry(), characterEntity, keyToClipMap[3], 0.2f);
                animComp.loop = false;
            }
            
            // Debug: Log current animation state
            static float lastLogTime = 0.0f;
            float currentTime = OpenEye::Core::Time::TotalTime();
            if (currentTime - lastLogTime > 2.0f) { // Log every 2 seconds
                if (animComp.currentClipIndex >= 0 && animComp.currentClipIndex < static_cast<int>(animComp.clips.size())) {
                    auto currentClip = std::static_pointer_cast<OpenEye::Animation::AnimationClip>(animComp.clips[animComp.currentClipIndex]);
                    if (currentClip) {
                        OE_INFO("Current animation: clip " + std::to_string(animComp.currentClipIndex) + 
                               " (" + currentClip->GetName() + "), time=" + std::to_string(animComp.currentTime) + 
                               "/" + std::to_string(currentClip->GetDuration()) + ", loop=" + (animComp.loop ? "yes" : "no"));
                    }
                }
                lastLogTime = currentTime;
            }
        }
        
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
                    
                    // Build clip names list (index, name, duration, channels)
                    std::vector<std::string> clipNames;
                    clipNames.reserve(animComp.clips.size());
                    for (size_t i = 0; i < animComp.clips.size(); i++) {
                        auto clip = std::static_pointer_cast<OpenEye::Animation::AnimationClip>(animComp.clips[i]);
                        if (clip) {
                            std::string name = clip->GetName();
                            if (name.empty()) name = "Unnamed";
                            clipNames.push_back(
                                std::to_string(i) + ": " + name +
                                " (" + std::to_string(clip->GetDuration()) + "s, " +
                                std::to_string(clip->GetChannelCount()) + " ch)"
                            );
                        } else {
                            clipNames.push_back(std::to_string(i) + ": (invalid)");
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
        
        // Physics Ball Debug Panel
        ImGui::Begin("Physics Ball Debug");
        {
            // Find ball entity by name
            auto& sceneRegistry = scene.GetRegistry();
            Entity ballEntity = entt::null;
            auto nameView = sceneRegistry.view<Name>();
            for (auto entity : nameView) {
                const auto& name = nameView.get<Name>(entity);
                if (name.name == "BouncingBall") {
                    ballEntity = entity;
                    break;
                }
            }
            
            if (ballEntity != entt::null && sceneRegistry.all_of<PhysicsBody, Transform>(ballEntity)) {
                auto& physicsBody = sceneRegistry.get<PhysicsBody>(ballEntity);
                auto& transform = sceneRegistry.get<Transform>(ballEntity);
                
                if (physicsBody.rigidBody) {
                    // Current state display
                    ImGui::Text("Ball State:");
                    ImGui::Text("Position: (%.2f, %.2f, %.2f)", transform.position.x, transform.position.y, transform.position.z);
                    
                    btVector3 velocity = physicsBody.rigidBody->getLinearVelocity();
                    ImGui::Text("Velocity: (%.2f, %.2f, %.2f)", velocity.x(), velocity.y(), velocity.z());
                    ImGui::Text("Speed: %.2f m/s", velocity.length());
                    
                    int activationState = physicsBody.rigidBody->getActivationState();
                    ImGui::Text("Activation: %s", (activationState == ACTIVE_TAG ? "Active" : "Inactive"));
                    
                    ImGui::Separator();
                    
                    // Bounciness controls
                    ImGui::Text("Bounce Settings:");
                    static float restitution = 0.9f;
                    if (ImGui::DragFloat("Restitution", &restitution, 0.01f, 0.0f, 2.0f)) {
                        physicsBody.rigidBody->setRestitution(restitution);
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Reset##Rest")) {
                        restitution = 0.9f;
                        physicsBody.rigidBody->setRestitution(restitution);
                    }
                    
                    static float friction = 0.7f;
                    if (ImGui::DragFloat("Friction", &friction, 0.01f, 0.0f, 2.0f)) {
                        physicsBody.rigidBody->setFriction(friction);
                    }
                    
                    static float linearDamping = 0.02f;
                    static float angularDamping = 0.8f;
                    if (ImGui::DragFloat("Linear Damping", &linearDamping, 0.01f, 0.0f, 1.0f)) {
                        physicsBody.rigidBody->setDamping(linearDamping, angularDamping);
                    }
                    if (ImGui::DragFloat("Angular Damping", &angularDamping, 0.01f, 0.0f, 1.0f)) {
                        physicsBody.rigidBody->setDamping(linearDamping, angularDamping);
                    }
                    
                    ImGui::Separator();
                    ImGui::Text("Physics Settings:");
                    
                    static float mass = 1.0f;
                    ImGui::Text("Mass: %.2f (read-only)", physicsBody.mass);
                    
                    ImGui::Separator();
                    ImGui::Text("Actions:");
                    
                    if (ImGui::Button("Reset Position")) {
                        transform.position = glm::vec3(0.0f, 5.0f, 0.0f);
                        btTransform resetTransform;
                        resetTransform.setOrigin(btVector3(0.0f, 5.0f, 0.0f));
                        resetTransform.setRotation(btQuaternion::getIdentity());
                        physicsBody.rigidBody->getMotionState()->setWorldTransform(resetTransform);
                        physicsBody.rigidBody->setWorldTransform(resetTransform);
                        physicsBody.rigidBody->setLinearVelocity(btVector3(0, 0, 0));
                        physicsBody.rigidBody->setAngularVelocity(btVector3(0, 0, 0));
                        physicsBody.rigidBody->activate(true);
                        TransformSystem::MarkDirty(sceneRegistry, ballEntity);
                    }
                    
                    ImGui::SameLine();
                    if (ImGui::Button("Apply Upward Force")) {
                        physicsBody.rigidBody->applyCentralImpulse(btVector3(0, 10.0f, 0));
                        physicsBody.rigidBody->activate(true);
                    }
                    
                    if (ImGui::Button("Apply Forward Force")) {
                        physicsBody.rigidBody->applyCentralImpulse(btVector3(0, 0, -5.0f));
                        physicsBody.rigidBody->activate(true);
                    }
                    
                    ImGui::SameLine();
                    if (ImGui::Button("Stop")) {
                        physicsBody.rigidBody->setLinearVelocity(btVector3(0, 0, 0));
                        physicsBody.rigidBody->setAngularVelocity(btVector3(0, 0, 0));
                    }
                    
                    if (ImGui::Button("Force Activate")) {
                        physicsBody.rigidBody->setActivationState(DISABLE_DEACTIVATION);
                        physicsBody.rigidBody->activate(true);
                    }
                    
                    ImGui::Separator();
                    ImGui::Text("Ground Restitution:");
                    // Find ground entity
                    Entity groundPlaneEntity = entt::null;
                    for (auto entity : nameView) {
                        const auto& groundName = nameView.get<Name>(entity);
                        if (groundName.name == "GroundPlane") {
                            groundPlaneEntity = entity;
                            break;
                        }
                    }
                    
                    if (groundPlaneEntity != entt::null && sceneRegistry.all_of<PhysicsBody>(groundPlaneEntity)) {
                        auto& groundPhysicsBody = sceneRegistry.get<PhysicsBody>(groundPlaneEntity);
                        if (groundPhysicsBody.rigidBody) {
                            static float groundRestitution = 0.0f;
                            float currentGroundRest = groundPhysicsBody.rigidBody->getRestitution();
                            if (ImGui::DragFloat("Ground Restitution", &groundRestitution, 0.01f, 0.0f, 2.0f)) {
                                groundPhysicsBody.rigidBody->setRestitution(groundRestitution);
                            }
                            if (currentGroundRest != groundRestitution) {
                                groundRestitution = currentGroundRest;
                            }
                        }
                    }
                } else {
                    ImGui::Text("Ball has no physics body!");
                }
            } else {
                ImGui::Text("Ball entity not found or missing components!");
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

