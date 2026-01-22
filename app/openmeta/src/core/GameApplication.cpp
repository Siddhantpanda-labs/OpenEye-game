#include "GameApplication.h"
#include "core/api/All.h"
#include "ecs/api/All.h"
#include "rendering/api/All.h"
#include "ecs/Systems.h"
// Include internal PhysicsWorld for PhysicsSystem functions that need it
// This is needed because PhysicsSystem::CreateGroundPlane and AttachPhysicsBody
// take Physics::PhysicsWorld* (internal type), not the API type
#include "physics/PhysicsWorld.h"
#include "physics/softbody/SoftBodyFactory.h"
#include "physics/softbody/templates/ClothTemplates.h"
#include "physics/softbody/templates/VolumeTemplates.h"
#include "physics/bodies/RigidBodyFactory.h"
#include "physics/shapes/CollisionShapeFactory.h"
#include "particles/ParticleEmitter.h"
#include "fluids/FluidSimulation.h"
#include "animation/AnimationLoader.h"
#include "animation/AnimationClip.h"
#include "animation/Skeleton.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <filesystem>
#include <glm/gtc/quaternion.hpp>
#include <unordered_map>
#include <BulletSoftBody/btSoftBody.h>

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

    // --------------------------------------------------------------------
    // Animator demo (Orc): combine separate FBX clips into one animation set
    // --------------------------------------------------------------------
    {
        const std::string idlePath = "assets/models/animation/Orc Idle.fbx";
        const std::string walkPath = "assets/models/animation/Start Walking.fbx";
        const std::string runPath  = "assets/models/animation/Running.fbx";

        Entity orcRoot = m_scene->LoadModel(idlePath, "Orc", glm::vec3(-3.0f, 0.0f, 0.0f));
        if (orcRoot != entt::null) {
            // Scale down the imported FBX (it comes in very large in world units)
            registry.get<Transform>(orcRoot).scale = glm::vec3(0.1f);

            // Find the mesh entity that actually has AnimationComponent
            Entity orcMeshEntity = entt::null;
            auto animView = registry.view<AnimationComponent, Transform>();
            for (auto ent : animView) {
                const auto& t = animView.get<Transform>(ent);
                if (t.parent == orcRoot) {
                    orcMeshEntity = ent;
                    break;
                }
            }

            if (orcMeshEntity != entt::null) {
                auto& anim = registry.get<AnimationComponent>(orcMeshEntity);
                auto skeleton = std::static_pointer_cast<OpenEye::Animation::Skeleton>(anim.skeleton);

                if (skeleton) {
                    // Rename the base clip to match controller expectations (if present)
                    if (!anim.clips.empty()) {
                        auto baseClip = std::static_pointer_cast<OpenEye::Animation::AnimationClip>(anim.clips[0]);
                        if (baseClip) baseClip->SetName("Orc Idle");
                    }

                    // Load and append extra clips from other FBX files (map by bone names)
                    auto walkClips = OpenEye::Animation::AnimationLoader::LoadClipsForSkeletonFromFile(walkPath, *skeleton);
                    for (auto& c : walkClips) {
                        if (c) {
                            c->SetName("Start Walking");
                            anim.clips.push_back(c);
                        }
                    }
                    auto runClips = OpenEye::Animation::AnimationLoader::LoadClipsForSkeletonFromFile(runPath, *skeleton);
                    for (auto& c : runClips) {
                        if (c) {
                            c->SetName("Running");
                            anim.clips.push_back(c);
                        }
                    }

                    // Attach AnimatorComponent to drive this AnimationComponent
                    if (!registry.all_of<AnimatorComponent>(orcMeshEntity)) {
                        auto& animator = registry.emplace<AnimatorComponent>(orcMeshEntity);
                        animator.controllerPath = "assets/anim/controllers/orc_controller.json";
                        animator.applyRootMotion = false; // safer default for demo (can toggle in Animator panel)
                    }

                    OE_INFO_F("Orc animator demo: clips=%zu bones=%zu", anim.clips.size(), skeleton->GetBoneCount());
                } else {
                    OE_WARN("Orc animator demo: skeleton missing (FBX may not contain skin weights)");
                }
            } else {
                OE_WARN("Orc animator demo: could not find child mesh with AnimationComponent");
            }
        } else {
            OE_WARN("Orc animator demo: failed to load FBX");
        }
    }
    
    // XPBD cloth demo (CPU) + rigid-cluster jelly (springs). No Bullet deformables.
    if (m_physicsWorld) {
        void* internalPhysicsWorld = m_physicsWorld->GetInternalWorld();
        OpenEye::Physics::PhysicsWorld* physicsWorld = static_cast<OpenEye::Physics::PhysicsWorld*>(internalPhysicsWorld);

        // --------------------------------------------------------------------
        // Cloth anchor cylinder (static)
        // --------------------------------------------------------------------
        const glm::vec3 clothTopPos(0.0f, 4.0f, -2.0f);
        const float clothWidth = 3.2f;
        const float clothHeight = 2.2f;
        const int clothSegX = 44;
        const int clothSegY = 30;

        const float cylinderRadius = 0.10f;
        const float cylinderHeight = clothWidth + 0.25f;
        const glm::vec3 cylinderPos = clothTopPos;
        const glm::quat cylinderRot = glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));

        auto cylinderShape = OpenEye::Physics::CollisionShapeFactory::CreateCylinder(
            glm::vec3(cylinderRadius, cylinderHeight * 0.5f, cylinderRadius)
        );
        btRigidBody* cylinderBody = OpenEye::Physics::RigidBodyFactory::Create(
            physicsWorld,
            cylinderPos,
            cylinderRot,
            cylinderShape,
            0.0f
        );
        if (cylinderBody) {
            cylinderBody->setCollisionFlags(cylinderBody->getCollisionFlags() | btCollisionObject::CF_STATIC_OBJECT);
            cylinderBody->setActivationState(DISABLE_SIMULATION);
        }

        Entity cylinderEntity = m_scene->CreateEntity("ClothAnchorCylinder");
        auto& cylinderTransform = registry.get<Transform>(cylinderEntity);
        cylinderTransform.position = cylinderPos;
        cylinderTransform.rotation = cylinderRot;

        auto cylinderMesh = CreateCylinderMesh(cylinderRadius, cylinderHeight, 32);
        auto cylinderMaterial = std::make_shared<OpenEye::Rendering::Material>();
        auto cylinderShader = std::make_shared<OpenEye::Rendering::Shader>();
        cylinderShader->LoadFromFile("assets/shaders/passes/deferred/gbuffer.vert",
                                     "assets/shaders/passes/deferred/gbuffer.frag");
        cylinderMaterial->SetShader(cylinderShader);
        cylinderMaterial->SetAlbedo(glm::vec3(0.25f, 0.25f, 0.25f));

        auto& cylinderRenderable = registry.emplace<Renderable>(cylinderEntity);
        cylinderRenderable.mesh = cylinderMesh;
        cylinderRenderable.material = cylinderMaterial;
        cylinderRenderable.visible = true;
        registry.emplace<AABB>(cylinderEntity);

        auto& cylinderPhysicsBody = registry.emplace<PhysicsBody>(cylinderEntity);
        cylinderPhysicsBody.rigidBody = cylinderBody;
        cylinderPhysicsBody.collisionShape = cylinderShape;
        cylinderPhysicsBody.mass = 0.0f;
        cylinderPhysicsBody.syncTransformToPhysics = true;
        cylinderPhysicsBody.syncPhysicsToTransform = false;

        // --------------------------------------------------------------------
        // XPBD cloth entity (pinned top edge)
        // --------------------------------------------------------------------
        Entity clothEntity = m_scene->CreateEntity("XPBDCloth");
        auto& clothTransform = registry.get<Transform>(clothEntity);
        clothTransform.position = clothTopPos;

        // Build a vertical grid mesh in local space: x in [-w/2..w/2], y in [0..-h]
        auto clothMesh = std::make_shared<OpenEye::Rendering::Mesh>();
        std::vector<OpenEye::Rendering::Vertex> verts;
        std::vector<uint32_t> indices;
        const int w = clothSegX + 1;
        const int h = clothSegY + 1;
        verts.resize(size_t(w * h));

        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                const float u = (clothSegX > 0) ? (float(x) / float(clothSegX)) : 0.0f;
                const float v = (clothSegY > 0) ? (float(y) / float(clothSegY)) : 0.0f;
                OpenEye::Rendering::Vertex vert{};
                vert.position = glm::vec3((u - 0.5f) * clothWidth, -v * clothHeight, 0.0f);
                vert.normal = glm::vec3(0.0f, 0.0f, 1.0f);
                vert.texCoord = glm::vec2(u, v);
                verts[size_t(y * w + x)] = vert;
            }
        }
        indices.reserve(size_t(clothSegX * clothSegY * 6));
        for (int y = 0; y < clothSegY; ++y) {
            for (int x = 0; x < clothSegX; ++x) {
                const uint32_t i0 = uint32_t(y * w + x);
                const uint32_t i1 = i0 + 1;
                const uint32_t i2 = i0 + uint32_t(w);
                const uint32_t i3 = i2 + 1;
                // two tris
                indices.push_back(i0); indices.push_back(i2); indices.push_back(i1);
                indices.push_back(i1); indices.push_back(i2); indices.push_back(i3);
            }
        }

        OpenEye::Rendering::Submesh sm{};
        sm.indexOffset = 0;
        sm.indexCount = uint32_t(indices.size());
        sm.vertexOffset = 0;
        sm.vertexCount = uint32_t(verts.size());
        sm.materialIndex = 0;
        clothMesh->SetVertices(verts);
        clothMesh->SetIndices(indices);
        clothMesh->AddSubmesh(sm);
        clothMesh->UploadToGPU();

        auto clothMaterial = std::make_shared<OpenEye::Rendering::Material>();
        auto clothShader = std::make_shared<OpenEye::Rendering::Shader>();
        clothShader->LoadFromFile("assets/shaders/passes/deferred/gbuffer.vert",
                                  "assets/shaders/passes/deferred/gbuffer.frag");
        clothMaterial->SetShader(clothShader);
        clothMaterial->SetAlbedo(glm::vec3(0.75f, 0.15f, 0.15f));

        auto& clothRenderable = registry.emplace<Renderable>(clothEntity);
        clothRenderable.mesh = clothMesh;
        clothRenderable.material = clothMaterial;
        clothRenderable.visible = true;
        registry.emplace<AABB>(clothEntity);

        XPBDClothComponent clothSim;
        clothSim.widthSegments = clothSegX;
        clothSim.heightSegments = clothSegY;
        clothSim.width = clothWidth;
        clothSim.height = clothHeight;
        clothSim.iterations = 10;
        clothSim.compliance = 0.00035f;
        clothSim.damping = 0.03f;
        clothSim.enableShear = true;
        clothSim.windEnabled = true;
        clothSim.windDirection = glm::vec3(1.0f, 0.0f, 0.0f);
        clothSim.windStrength = 7.0f;
        clothSim.restLenX = clothWidth / float(clothSegX);
        clothSim.restLenY = clothHeight / float(clothSegY);
        clothSim.positions.resize(verts.size());
        clothSim.prevPositions.resize(verts.size());
        clothSim.invMass.resize(verts.size(), 1.0f);
        for (size_t i = 0; i < verts.size(); ++i) {
            clothSim.positions[i] = verts[i].position;
            clothSim.prevPositions[i] = verts[i].position;
        }
        // Pin top edge (row 0)
        for (int x = 0; x < w; ++x) {
            clothSim.invMass[size_t(x)] = 0.0f;
        }
        registry.emplace<XPBDClothComponent>(clothEntity, std::move(clothSim));

        OE_INFO("Created XPBD cloth demo (CPU) with static cylinder anchor");

        // --------------------------------------------------------------------
        // Jelly blob demo: rigid lattice + constraints (physics) + one deforming sphere mesh (render)
        // --------------------------------------------------------------------
        const glm::vec3 jellyOrigin(2.5f, 6.0f, 1.0f); // min corner of lattice in world
        const int dim = 3;                             // 3x3x3 control points
        const float spacing = 0.55f;
        const float nodeRadius = 0.18f;
        const float nodeMass = 0.35f;

        const glm::vec3 cageExt = glm::vec3((dim - 1) * spacing);
        const glm::vec3 cageCenterWorld = jellyOrigin + cageExt * 0.5f;

        // Node render mesh/material (debug only)
        auto nodeMesh = CreateSphereMesh(nodeRadius, 18, 14);
        auto nodeMaterial = std::make_shared<OpenEye::Rendering::Material>();
        auto nodeShader = std::make_shared<OpenEye::Rendering::Shader>();
        nodeShader->LoadFromFile("assets/shaders/passes/deferred/gbuffer.vert",
                                 "assets/shaders/passes/deferred/gbuffer.frag");
        nodeMaterial->SetShader(nodeShader);
        nodeMaterial->SetAlbedo(glm::vec3(0.12f, 0.38f, 0.9f));

        // Blob render mesh/material (what the user sees)
        const float blobRadius = cageExt.x * 0.5f * 0.95f;
        auto blobMesh = CreateSphereMesh(blobRadius, 40, 26);
        auto blobMaterial = std::make_shared<OpenEye::Rendering::Material>();
        auto blobShader = std::make_shared<OpenEye::Rendering::Shader>();
        blobShader->LoadFromFile("assets/shaders/passes/deferred/gbuffer.vert",
                                 "assets/shaders/passes/deferred/gbuffer.frag");
        blobMaterial->SetShader(blobShader);
        blobMaterial->SetAlbedo(glm::vec3(0.10f, 0.30f, 0.85f));

        Entity blobEntity = m_scene->CreateEntity("JellyBlob");
        auto& blobTransform = registry.get<Transform>(blobEntity);
        blobTransform.position = cageCenterWorld;

        auto& blobRenderable = registry.emplace<Renderable>(blobEntity);
        blobRenderable.mesh = blobMesh;
        blobRenderable.material = blobMaterial;
        blobRenderable.visible = true;
        registry.emplace<AABB>(blobEntity);

        // Build rigid nodes
        auto nodeShape = physicsWorld->CreateSphereShape(nodeRadius);
        std::vector<Entity> nodes;
        nodes.reserve(size_t(dim * dim * dim));
        std::vector<btTypedConstraint*> constraints;

        auto idxOf = [dim](int x, int y, int z) { return (z * dim * dim) + (y * dim) + x; };

        for (int z = 0; z < dim; ++z) {
            for (int y = 0; y < dim; ++y) {
                for (int x = 0; x < dim; ++x) {
                    Entity n = m_scene->CreateEntity("JellyNode");
                    auto& t = registry.get<Transform>(n);
                    t.position = jellyOrigin + glm::vec3(x * spacing, y * spacing, z * spacing);

                    auto& r = registry.emplace<Renderable>(n);
                    r.mesh = nodeMesh;
                    r.material = nodeMaterial;
                    r.visible = false; // hidden by default; can be enabled from editor for debugging
                    registry.emplace<AABB>(n);

                    PhysicsSystem::AttachPhysicsBody(*m_scene, n, physicsWorld, nodeShape, nodeMass, false);
                    if (registry.all_of<PhysicsBody>(n)) {
                        auto& pb = registry.get<PhysicsBody>(n);
                        if (pb.rigidBody) {
                            pb.rigidBody->setDamping(0.05f, 0.85f);
                            pb.rigidBody->setFriction(0.8f);
                            pb.rigidBody->setRestitution(0.05f);
                        }
                    }

                    nodes.push_back(n);
                }
            }
        }

        // Connect neighbors with point-to-point constraints (springy lattice)
        const float tau = 0.55f;
        const float damp = 0.75f;

        for (int z = 0; z < dim; ++z) {
            for (int y = 0; y < dim; ++y) {
                for (int x = 0; x < dim; ++x) {
                    const int aIndex = idxOf(x, y, z);
                    auto& aPB = registry.get<PhysicsBody>(nodes[size_t(aIndex)]);
                    if (!aPB.rigidBody) continue;

                    auto link = [&](int nx, int ny, int nz, const glm::vec3& restVec) {
                        if (nx < 0 || ny < 0 || nz < 0 || nx >= dim || ny >= dim || nz >= dim) return;
                        const int bIndex = idxOf(nx, ny, nz);
                        auto& bPB = registry.get<PhysicsBody>(nodes[size_t(bIndex)]);
                        if (!bPB.rigidBody) return;

                        const btVector3 pivotA(restVec.x * 0.5f, restVec.y * 0.5f, restVec.z * 0.5f);
                        const btVector3 pivotB(-restVec.x * 0.5f, -restVec.y * 0.5f, -restVec.z * 0.5f);
                        auto* c = new btPoint2PointConstraint(*aPB.rigidBody, *bPB.rigidBody, pivotA, pivotB);
                        c->m_setting.m_tau = tau;
                        c->m_setting.m_damping = damp;
                        physicsWorld->AddConstraint(c, true);
                        constraints.push_back(c);
                    };

                    link(x + 1, y, z, glm::vec3(spacing, 0, 0));
                    link(x, y + 1, z, glm::vec3(0, spacing, 0));
                    link(x, y, z + 1, glm::vec3(0, 0, spacing));
                }
            }
        }

        // Attach JellyBlob component for deformation + editor controls
        JellyBlobComponent blobComp;
        blobComp.dim = dim;
        blobComp.nodeEntities = nodes;
        blobComp.constraints = constraints;
        blobComp.springTau = tau;
        blobComp.springDamping = damp;
        blobComp.showDebugNodes = false;
        blobComp.restMin = -cageExt * 0.5f;
        blobComp.restMax = cageExt * 0.5f;
        blobComp.baseVertices = blobMesh->GetVertices(); // rest shape
        registry.emplace<JellyBlobComponent>(blobEntity, std::move(blobComp));

        OE_INFO("Created jelly blob demo (rigid lattice + single deforming render mesh)");
    }

    // Ensure debug primitives (cloth/cylinder) get initial AABBs right away so they render on frame 1
    OpenEye::ECS::CullingSystem::UpdateAABBs(*m_scene);
    
    OE_INFO("Created default scene with camera, light, ground plane, Dess model, XPBD cloth, and jelly cluster");
}

std::shared_ptr<OpenEye::Rendering::Mesh> GameApplication::CreateSphereMesh(float radius, int segments, int rings) {
    using namespace OpenEye::Rendering;
    auto mesh = std::make_shared<Mesh>();

    segments = std::max(3, segments);
    rings = std::max(2, rings);

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    // Vertices
    vertices.reserve(size_t((rings + 1) * (segments + 1)));
    for (int y = 0; y <= rings; ++y) {
        const float v = float(y) / float(rings);
        const float phi = v * glm::pi<float>(); // 0..pi
        const float sinPhi = std::sin(phi);
        const float cosPhi = std::cos(phi);

        for (int x = 0; x <= segments; ++x) {
            const float u = float(x) / float(segments);
            const float theta = u * glm::two_pi<float>(); // 0..2pi
            const float sinTheta = std::sin(theta);
            const float cosTheta = std::cos(theta);

            glm::vec3 n = glm::vec3(cosTheta * sinPhi, cosPhi, sinTheta * sinPhi);

            Vertex vert{};
            vert.position = n * radius;
            vert.normal = glm::normalize(n);
            vert.texCoord = glm::vec2(u, v);
            vertices.push_back(vert);
        }
    }

    // Indices
    indices.reserve(size_t(rings * segments * 6));
    const int stride = segments + 1;
    for (int y = 0; y < rings; ++y) {
        for (int x = 0; x < segments; ++x) {
            const uint32_t i0 = uint32_t(y * stride + x);
            const uint32_t i1 = i0 + 1;
            const uint32_t i2 = i0 + uint32_t(stride);
            const uint32_t i3 = i2 + 1;

            indices.push_back(i0); indices.push_back(i2); indices.push_back(i1);
            indices.push_back(i1); indices.push_back(i2); indices.push_back(i3);
        }
    }

    Submesh sm{};
    sm.indexOffset = 0;
    sm.indexCount = uint32_t(indices.size());
    sm.vertexOffset = 0;
    sm.vertexCount = uint32_t(vertices.size());
    sm.materialIndex = 0;

    mesh->SetVertices(vertices);
    mesh->SetIndices(indices);
    mesh->AddSubmesh(sm);
    mesh->UploadToGPU();
    return mesh;
}

std::shared_ptr<OpenEye::Rendering::Mesh> GameApplication::CreateMeshFromCloth(
    btSoftBody* cloth, 
    int widthSegments, 
    int heightSegments
) {
    using namespace OpenEye::Rendering;
    
    if (!cloth) return nullptr;
    
    auto mesh = std::make_shared<Mesh>();
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    
    // Get cloth world transform to convert to local space
    btTransform clothTransform = cloth->getWorldTransform();
    btVector3 clothWorldPos = clothTransform.getOrigin();
    glm::vec3 clothPos = glm::vec3(clothWorldPos.x(), clothWorldPos.y(), clothWorldPos.z());
    
    // Create vertices from cloth nodes (convert to local space)
    int nodeCount = cloth->m_nodes.size();
    for (int i = 0; i < nodeCount; ++i) {
        const btVector3& nodePos = cloth->m_nodes[i].m_x;
        const btVector3& nodeNormal = cloth->m_nodes[i].m_n;
        
        Vertex vertex;
        // Convert world space node position to local space (relative to cloth origin)
        vertex.position = glm::vec3(nodePos.x(), nodePos.y(), nodePos.z()) - clothPos;
        vertex.normal = glm::vec3(nodeNormal.x(), nodeNormal.y(), nodeNormal.z());
        
        // Calculate UV coordinates based on grid position
        int x = i % (widthSegments + 1);
        int y = i / (widthSegments + 1);
        vertex.texCoord = glm::vec2(
            static_cast<float>(x) / static_cast<float>(widthSegments),
            static_cast<float>(y) / static_cast<float>(heightSegments)
        );
        
        vertex.tangent = glm::vec3(1.0f, 0.0f, 0.0f);
        vertex.bitangent = glm::vec3(0.0f, 0.0f, 1.0f);
        
        vertices.push_back(vertex);
    }
    
    // Create indices from cloth faces
    // Bullet stores face nodes as pointers, need to find their indices
    for (int i = 0; i < cloth->m_faces.size(); ++i) {
        const btSoftBody::Face& face = cloth->m_faces[i];
        // Find node indices by comparing pointers
        int idx0 = -1, idx1 = -1, idx2 = -1;
        for (int j = 0; j < nodeCount; ++j) {
            if (&cloth->m_nodes[j] == face.m_n[0]) idx0 = j;
            if (&cloth->m_nodes[j] == face.m_n[1]) idx1 = j;
            if (&cloth->m_nodes[j] == face.m_n[2]) idx2 = j;
        }
        if (idx0 >= 0 && idx1 >= 0 && idx2 >= 0) {
            indices.push_back(idx0);
            indices.push_back(idx1);
            indices.push_back(idx2);
        }
    }
    
    mesh->SetVertices(vertices);
    mesh->SetIndices(indices);
    
    // Add submesh for rendering
    OpenEye::Rendering::Submesh submesh;
    submesh.indexOffset = 0;
    submesh.indexCount = static_cast<uint32_t>(indices.size());
    submesh.vertexOffset = 0;
    submesh.vertexCount = static_cast<uint32_t>(vertices.size());
    submesh.materialIndex = 0;
    mesh->AddSubmesh(submesh);
    
    mesh->UploadToGPU();
    
    return mesh;
}

std::shared_ptr<OpenEye::Rendering::Mesh> GameApplication::CreateMeshFromSoftBody(btSoftBody* body, glm::vec3& outOrigin) {
    using namespace OpenEye::Rendering;
    if (!body) return nullptr;

    auto mesh = std::make_shared<Mesh>();

    const int nodeCount = body->m_nodes.size();
    std::vector<Vertex> vertices;
    vertices.reserve(nodeCount);

    // Compute an origin from the current node positions (CreateEllipsoid returns world-space nodes,
    // and getWorldTransform() is often identity). Using the average keeps the render transform stable.
    glm::vec3 origin(0.0f);
    if (nodeCount > 0) {
        for (int i = 0; i < nodeCount; ++i) {
            const btVector3& p = body->m_nodes[i].m_x;
            origin += glm::vec3(p.x(), p.y(), p.z());
        }
        origin /= float(nodeCount);
    }
    outOrigin = origin;

    for (int i = 0; i < nodeCount; ++i) {
        const btVector3& p = body->m_nodes[i].m_x;
        const btVector3& n = body->m_nodes[i].m_n;
        Vertex v;
        v.position = glm::vec3(p.x(), p.y(), p.z()) - origin;
        v.normal = glm::vec3(n.x(), n.y(), n.z());
        v.texCoord = glm::vec2(0.0f);
        v.tangent = glm::vec3(1, 0, 0);
        v.bitangent = glm::vec3(0, 1, 0);
        vertices.push_back(v);
    }

    // Build indices from faces. Use a pointer->index map for speed.
    std::vector<uint32_t> indices;
    indices.reserve(body->m_faces.size() * 3);

    std::unordered_map<const btSoftBody::Node*, uint32_t> nodeIndex;
    nodeIndex.reserve(static_cast<size_t>(nodeCount));
    for (int i = 0; i < nodeCount; ++i) {
        nodeIndex[&body->m_nodes[i]] = static_cast<uint32_t>(i);
    }

    for (int i = 0; i < body->m_faces.size(); ++i) {
        const btSoftBody::Face& f = body->m_faces[i];
        auto it0 = nodeIndex.find(f.m_n[0]);
        auto it1 = nodeIndex.find(f.m_n[1]);
        auto it2 = nodeIndex.find(f.m_n[2]);
        if (it0 != nodeIndex.end() && it1 != nodeIndex.end() && it2 != nodeIndex.end()) {
            indices.push_back(it0->second);
            indices.push_back(it1->second);
            indices.push_back(it2->second);
        }
    }

    mesh->SetVertices(vertices);
    mesh->SetIndices(indices);

    Submesh submesh;
    submesh.indexOffset = 0;
    submesh.indexCount = static_cast<uint32_t>(indices.size());
    submesh.vertexOffset = 0;
    submesh.vertexCount = static_cast<uint32_t>(vertices.size());
    submesh.materialIndex = 0;
    mesh->AddSubmesh(submesh);
    mesh->UploadToGPU();

    return mesh;
}

std::shared_ptr<OpenEye::Rendering::Mesh> GameApplication::CreateCylinderMesh(
    float radius,
    float height,
    int segments
) {
    using namespace OpenEye::Rendering;
    
    auto mesh = std::make_shared<Mesh>();
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    
    float halfHeight = height * 0.5f;
    
    // Create vertices for cylinder (centered at origin, aligned along Y axis)
    // Top circle
    for (int i = 0; i <= segments; ++i) {
        float angle = (float(i) / float(segments)) * 2.0f * glm::pi<float>();
        float x = radius * glm::cos(angle);
        float z = radius * glm::sin(angle);
        
        Vertex vertex;
        vertex.position = glm::vec3(x, halfHeight, z);
        vertex.normal = glm::normalize(glm::vec3(x / radius, 0.0f, z / radius));
        vertex.texCoord = glm::vec2(float(i) / float(segments), 1.0f);
        vertex.tangent = glm::vec3(-glm::sin(angle), 0.0f, glm::cos(angle));
        vertex.bitangent = glm::vec3(0.0f, 1.0f, 0.0f);
        vertices.push_back(vertex);
    }
    
    // Bottom circle
    for (int i = 0; i <= segments; ++i) {
        float angle = (float(i) / float(segments)) * 2.0f * glm::pi<float>();
        float x = radius * glm::cos(angle);
        float z = radius * glm::sin(angle);
        
        Vertex vertex;
        vertex.position = glm::vec3(x, -halfHeight, z);
        vertex.normal = glm::normalize(glm::vec3(x / radius, 0.0f, z / radius));
        vertex.texCoord = glm::vec2(float(i) / float(segments), 0.0f);
        vertex.tangent = glm::vec3(-glm::sin(angle), 0.0f, glm::cos(angle));
        vertex.bitangent = glm::vec3(0.0f, 1.0f, 0.0f);
        vertices.push_back(vertex);
    }
    
    // Create side faces (quads as triangles)
    int topStart = 0;
    int bottomStart = segments + 1;
    for (int i = 0; i < segments; ++i) {
        int top0 = topStart + i;
        int top1 = topStart + i + 1;
        int bottom0 = bottomStart + i;
        int bottom1 = bottomStart + i + 1;
        
        // First triangle
        indices.push_back(top0);
        indices.push_back(bottom0);
        indices.push_back(top1);
        
        // Second triangle
        indices.push_back(top1);
        indices.push_back(bottom0);
        indices.push_back(bottom1);
    }
    
    // Create top and bottom caps
    // Top cap
    int topCenter = static_cast<int>(vertices.size());
    Vertex topCenterVertex;
    topCenterVertex.position = glm::vec3(0.0f, halfHeight, 0.0f);
    topCenterVertex.normal = glm::vec3(0.0f, 1.0f, 0.0f);
    topCenterVertex.texCoord = glm::vec2(0.5f, 0.5f);
    topCenterVertex.tangent = glm::vec3(1.0f, 0.0f, 0.0f);
    topCenterVertex.bitangent = glm::vec3(0.0f, 0.0f, 1.0f);
    vertices.push_back(topCenterVertex);
    
    for (int i = 0; i < segments; ++i) {
        indices.push_back(topStart + i);
        indices.push_back(topCenter);
        indices.push_back(topStart + i + 1);
    }
    
    // Bottom cap
    int bottomCenter = static_cast<int>(vertices.size());
    Vertex bottomCenterVertex;
    bottomCenterVertex.position = glm::vec3(0.0f, -halfHeight, 0.0f);
    bottomCenterVertex.normal = glm::vec3(0.0f, -1.0f, 0.0f);
    bottomCenterVertex.texCoord = glm::vec2(0.5f, 0.5f);
    bottomCenterVertex.tangent = glm::vec3(1.0f, 0.0f, 0.0f);
    bottomCenterVertex.bitangent = glm::vec3(0.0f, 0.0f, -1.0f);
    vertices.push_back(bottomCenterVertex);
    
    for (int i = 0; i < segments; ++i) {
        indices.push_back(bottomStart + i + 1);
        indices.push_back(bottomCenter);
        indices.push_back(bottomStart + i);
    }
    
    mesh->SetVertices(vertices);
    mesh->SetIndices(indices);
    
    // Add submesh
    Submesh submesh;
    submesh.indexOffset = 0;
    submesh.indexCount = static_cast<uint32_t>(indices.size());
    submesh.vertexOffset = 0;
    submesh.vertexCount = static_cast<uint32_t>(vertices.size());
    submesh.materialIndex = 0;
    mesh->AddSubmesh(submesh);
    
    mesh->UploadToGPU();
    
    return mesh;
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

