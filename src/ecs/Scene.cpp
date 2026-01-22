#include "Scene.h"
#include "Components.h"
#include "core/log/Log.h"
#include "rendering/resources/MeshLoader.h"
#include "rendering/resources/Material.h"
#include "rendering/resources/Shader.h"
#include "animation/AnimationSystem.h"
#include "systems/TransformSystem.h"
#include "systems/PhysicsSystem.h"
#include "systems/ParticleSystem.h"
#include "systems/FluidSystem.h"
#include "systems/SoftBodySystem.h"
#include "systems/AnimatorSystem.h"
#include "systems/XPBDClothSystem.h"
#include "systems/JellyBlobSystem.h"
#include "systems/CullingSystem.h"
#include "physics/PhysicsWorld.h"
#include <filesystem>
#include <glm/glm.hpp>

using namespace OpenEye::Core;

namespace OpenEye {
namespace ECS {

Scene::Scene() {
    OE_DEBUG("Scene created");
}

Scene::~Scene() {
    OE_DEBUG("Scene destroyed");
}

Entity Scene::CreateEntity(const std::string& name) {
    Entity entity = m_registry.create();
    
    // Add Name component
    m_registry.emplace<Name>(entity, name);
    
    // Add Transform component (default)
    m_registry.emplace<Transform>(entity);
    
    return entity;
}

void Scene::DestroyEntity(Entity entity) {
    if (!m_registry.valid(entity)) {
        return;
    }
    
    // Remove from parent's children before destroying
    if (m_registry.all_of<Transform>(entity)) {
        auto& transform = m_registry.get<Transform>(entity);
        transform.parent = entt::null;
    }
    
    m_registry.destroy(entity);
}

Entity Scene::LoadModel(const std::string& filepath, const std::string& name, const glm::vec3& position) {
    using namespace OpenEye::Rendering;
    
    // Load scene (supports multiple meshes from glTF/FBX)
    auto loadedMeshes = MeshLoader::LoadSceneFromFile(filepath);
    
    if (loadedMeshes.empty()) {
        OE_WARN("Failed to load model from: " + filepath);
        return entt::null;
    }
    
    std::filesystem::path filePath(filepath);
    std::string baseName = name.empty() ? filePath.stem().string() : name;
    
    // Load shader for deferred rendering
    auto gbufferShader = std::make_shared<Shader>();
    bool useSkinnedShader = false;
    
    // Check if model has animation (determine shader type)
    for (const auto& loadedMesh : loadedMeshes) {
        if (loadedMesh->skeleton && !loadedMesh->animations.empty()) {
            useSkinnedShader = true;
            break;
        }
    }
    
    if (useSkinnedShader) {
        gbufferShader->LoadFromFile("assets/shaders/passes/deferred/gbuffer_skinned.vert", 
                                    "assets/shaders/passes/deferred/gbuffer_skinned.frag");
    } else {
        gbufferShader->LoadFromFile("assets/shaders/passes/deferred/gbuffer.vert", 
                                    "assets/shaders/passes/deferred/gbuffer.frag");
    }
    
    // Create parent entity for the model
    Entity parentEntity = CreateEntity(baseName);
    auto& parentTransform = m_registry.get<Transform>(parentEntity);
    parentTransform.position = position;
    
    // Create entities for each mesh
    for (size_t i = 0; i < loadedMeshes.size(); ++i) {
        const auto& loadedMesh = loadedMeshes[i];
        if (!loadedMesh->mesh || loadedMeshes[i]->materials.empty()) continue;
        
        std::string entityName = loadedMesh->name.empty() ? baseName + "_Mesh_" + std::to_string(i) : loadedMesh->name;
        Entity entity = CreateEntity(entityName);
        
        // Make parent entity the parent
        auto& meshTransform = m_registry.get<Transform>(entity);
        meshTransform.parent = parentEntity;
        meshTransform.dirty = true;
        
        // Use material from loaded mesh or create default
        auto material = loadedMesh->materials[0];
        if (!material) {
            material = std::make_shared<Material>(gbufferShader);
        } else {
            material->SetShader(gbufferShader);
        }
        
        // Add renderable component
        auto& renderable = m_registry.emplace<Renderable>(entity);
        renderable.mesh = loadedMesh->mesh;
        renderable.material = material;
        
        // Add AABB for culling
        m_registry.emplace<AABB>(entity);
        
        // Add animation component if model has skeleton
        if (loadedMesh->skeleton && !loadedMesh->animations.empty()) {
            AnimationComponent animComp;
            animComp.skeleton = std::static_pointer_cast<void>(loadedMesh->skeleton);
            
            // Convert AnimationClip pointers to void pointers
            animComp.clips.clear();
            animComp.clips.reserve(loadedMesh->animations.size());
            for (const auto& clip : loadedMesh->animations) {
                animComp.clips.push_back(std::static_pointer_cast<void>(clip));
            }
            
            animComp.currentClipIndex = 0;
            animComp.isPlaying = true;
            animComp.loop = true;
            animComp.playbackSpeed = 1.0f;
            animComp.boneMatricesDirty = true;
            
            m_registry.emplace<AnimationComponent>(entity, animComp);
            
            // Initialize bone matrices
            OpenEye::Animation::AnimationSystem::CalculateBoneMatrices(m_registry, entity);
        }
    }
    
    OE_INFO("Model loaded: " + baseName);
    return parentEntity;
}

void Scene::Update(float deltaTime, void* physicsWorld) {
    // 1. Update transforms first (world matrices need to be up to date)
    TransformSystem::Update(*this);
    
    // 2. Update animations (after transforms are updated)
    // Controller-driven animator runs first (fills AnimationComponent matrices), then legacy system can run too.
    AnimatorSystem::Update(*this, deltaTime, physicsWorld);
    // Legacy clip player for entities that don't have AnimatorComponent
    OpenEye::Animation::AnimationSystem::Update(*this, deltaTime);
    
    // 3. Update physics (if physics world is provided)
    if (physicsWorld) {
        // Cast void* to internal PhysicsWorld type (Scene is part of library, can access internal types)
        Physics::PhysicsWorld* internalPhysicsWorld = static_cast<Physics::PhysicsWorld*>(physicsWorld);
        PhysicsSystem::Update(*this, internalPhysicsWorld, deltaTime);
    }
    
    // 4. Update particles
    ParticleSystem::Update(*this, deltaTime);
    
    // 5. Update fluids
    FluidSystem::Update(*this, deltaTime);
    
    // 6. Update soft bodies (if physics world is provided)
    if (physicsWorld) {
        Physics::PhysicsWorld* internalPhysicsWorld = static_cast<Physics::PhysicsWorld*>(physicsWorld);
        SoftBodySystem::Update(*this, internalPhysicsWorld, deltaTime);
        SoftBodySystem::SyncMesh(*this); // Sync soft body nodes to mesh vertices
    }

    // 6.5 Update XPBD cloth (CPU, independent of Bullet)
    XPBDClothSystem::Update(*this, deltaTime);

    // 6.6 Update jelly blob render deformation (rigid cluster -> single deforming mesh)
    JellyBlobSystem::Update(*this, deltaTime);
    
    // 7. Update culling AABBs (after all position/transform updates)
    CullingSystem::UpdateAABBs(*this);
}

} // namespace ECS
} // namespace OpenEye

