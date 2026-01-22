#pragma once

#include <string>
#include <memory>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include "entt/entt.hpp"
#include "rendering/resources/Mesh.h"
#include "rendering/resources/Material.h"
#include "rendering/Camera.h"
#include "rendering/LODManager.h"
#include "animation/controller/AnimatorParams.h"

// Forward declarations for animation
namespace OpenEye {
namespace Animation {
    class Skeleton;
    class AnimationClip;
}
}

// Forward declarations for physics
namespace OpenEye {
namespace Physics {
    class PhysicsWorld;
}
namespace Particles {
    class ParticleEmitter;
}
namespace Fluids {
    class FluidSimulation;
}
}
// Forward declaration for Bullet soft body (in global namespace)
class btSoftBody;

// Forward declarations for Bullet
class btRigidBody;
class btCollisionShape;
class btTypedConstraint;

namespace OpenEye {
namespace ECS {

using Entity = entt::entity;

// Transform component
struct Transform {
    glm::vec3 position = glm::vec3(0.0f);
    glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 scale = glm::vec3(1.0f);
    
    // Parent entity (null entity if no parent)
    entt::entity parent = entt::null;
    
    // Cached world matrix (recomputed when dirty)
    mutable glm::mat4 worldMatrix = glm::mat4(1.0f);
    mutable bool dirty = true;
    
    glm::mat4 GetLocalMatrix() const {
        glm::mat4 transform = glm::translate(glm::mat4(1.0f), position);
        transform = transform * glm::mat4_cast(rotation);
        transform = glm::scale(transform, scale);
        return transform;
    }
    
    glm::mat4 GetWorldMatrix(const entt::registry& registry) const;
};

// Name component
struct Name {
    std::string name;
    
    Name() = default;
    explicit Name(const std::string& n) : name(n) {}
};

// Renderable component
struct Renderable {
    std::shared_ptr<Rendering::Mesh> mesh;
    std::shared_ptr<Rendering::Material> material;
    bool visible = true;
    
    // LOD support: can have multiple meshes for different LOD levels
    std::vector<std::shared_ptr<Rendering::Mesh>> lodMeshes; // LOD 0, 1, 2, etc.
    size_t activeLOD = 0;
    
    Renderable() = default;
    Renderable(std::shared_ptr<Rendering::Mesh> m, std::shared_ptr<Rendering::Material> mat)
        : mesh(std::move(m)), material(std::move(mat)) {}
};

// Camera component
struct CameraComponent {
    Rendering::Camera camera; // Still holds the rendering camera for view/projection matrices
    bool isPrimary = false; // Primary camera for rendering

    // Projection settings (moved from Rendering::Camera for ECS control)
    bool isPerspective = true;
    float fov = 45.0f;
    float aspect = 16.0f / 9.0f;
    float nearPlane = 0.1f;
    float farPlane = 100.0f;

    CameraComponent() = default;
};

// Light component
struct Light {
    enum class Type {
        Directional = 0,
        Point = 1,
        Spot = 2
    };
    
    Type type = Type::Directional;
    glm::vec3 color = glm::vec3(1.0f);
    float intensity = 1.0f;
    float range = 10.0f; // For point/spot lights
    float innerConeAngle = 15.0f; // For spot lights
    float outerConeAngle = 30.0f; // For spot lights
    bool castShadows = true;
};

// AABB component (for culling)
struct AABB {
    glm::vec3 min = glm::vec3(-0.5f);
    glm::vec3 max = glm::vec3(0.5f);
    bool isVisible = true; // Culling result flag
    
    glm::vec3 GetCenter() const {
        return (min + max) * 0.5f;
    }
    
    glm::vec3 GetExtents() const {
        return (max - min) * 0.5f;
    }
    
    glm::vec3 GetSize() const {
        return max - min;
    }
    
    // Transform AABB by matrix
    AABB Transform(const glm::mat4& matrix) const;
};

// LOD component (for level-of-detail management)
struct LOD {
    Rendering::LODSettings settings;
    
    LOD() = default;
    explicit LOD(const Rendering::LODSettings& s) : settings(s) {}
};

// Tag component (for grouping/querying)
struct Tag {
    std::vector<std::string> tags;
    
    void AddTag(const std::string& tag);
    void RemoveTag(const std::string& tag);
    bool HasTag(const std::string& tag) const;
};

// Animation component (for skeletal animation)
struct AnimationComponent {
    // Skeleton reference (shared across instances of same model)
    std::shared_ptr<void> skeleton;  // Actually std::shared_ptr<Animation::Skeleton>
    
    // Animation clips (all clips from the model)
    std::vector<std::shared_ptr<void>> clips;  // Actually std::shared_ptr<Animation::AnimationClip>
    
    // Playback state
    int currentClipIndex = -1;  // -1 if no clip playing
    float currentTime = 0.0f;   // Current time in seconds
    float playbackSpeed = 1.0f; // Playback speed multiplier (1.0 = normal)
    bool isPlaying = false;      // Is animation currently playing
    bool loop = true;            // Should animation loop when finished
    
    // Blending state (for crossfade between animations)
    int blendFromClipIndex = -1; // Index of clip to blend from (-1 if not blending)
    float blendTime = 0.0f;      // Current blend time
    float blendDuration = 0.2f;  // Total blend duration in seconds
    
    // Animation State Machine (optional - for character animation control)
    std::shared_ptr<void> stateMachine;  // Actually std::shared_ptr<Animation::AnimationStateMachine>
    
    // GPU-ready bone matrices (final matrices for skinning)
    std::vector<glm::mat4> finalBoneMatrices;
    
    // Optimization flag (set to true when bone matrices need recalculation)
    bool boneMatricesDirty = true;
};

// Animator runtime component (Unity/Unreal-style controller)
// This drives AnimationComponent (skeleton/clips/finalBoneMatrices) using a controller asset + parameters.
struct AnimatorComponent {
    // Controller asset reference
    std::string controllerPath;            // e.g. "assets/anim/controllers/humanoid.json"
    std::shared_ptr<void> controller;      // Actually std::shared_ptr<Animation::Controller::AnimatorController>

    // Runtime params (instance state)
    Animation::Controller::AnimatorParams params;

    // Root motion
    bool applyRootMotion = true;
    bool rootMotionAffectsPhysics = false; // if true and PhysicsBody is kinematic, push to Bullet

    // Per-layer runtime
    struct LayerRuntime {
        std::string currentState;
        std::string previousState;
        float stateTime = 0.0f;

        // Transition runtime
        bool inTransition = false;
        float blendTime = 0.0f;
        float blendDuration = 0.2f;
        float fromTime = 0.0f;
        std::string fromState;
    };

    std::vector<LayerRuntime> layers;

    // Debug/event history (for editor inspection)
    struct FiredEvent {
        std::string clip;
        std::string name;
        std::string payload;
        float time = 0.0f;
    };
    std::vector<FiredEvent> recentEvents;
    int maxRecentEvents = 32;
};

// Physics body component (for physics simulation)
struct PhysicsBody {
    // Bullet physics objects (owned by PhysicsWorld, but we keep references)
    btRigidBody* rigidBody = nullptr;  // Raw pointer - PhysicsWorld manages lifetime
    
    // Collision shape (shared_ptr to keep shape alive while component exists)
    std::shared_ptr<btCollisionShape> collisionShape;
    
    // Physics properties
    float mass = 0.0f;  // 0 = static/kinematic, > 0 = dynamic
    glm::vec3 inertia = glm::vec3(0.0f);  // Auto-calculated if not set
    
    // Sync flags
    bool syncTransformToPhysics = true;  // Sync Transform -> PhysicsBody before step
    bool syncPhysicsToTransform = true;  // Sync PhysicsBody -> Transform after step
    
    // Kinematic flag (if true, physics doesn't affect transform, but can be moved manually)
    bool isKinematic = false;
    
    PhysicsBody() = default;
};

// Particle emitter component
struct ParticleEmitterComponent {
    std::shared_ptr<OpenEye::Particles::ParticleEmitter> emitter;
    
    ParticleEmitterComponent() = default;
    explicit ParticleEmitterComponent(std::shared_ptr<OpenEye::Particles::ParticleEmitter> e) : emitter(e) {}
};

// Fluid volume component (volumetric gas/liquid simulation)
struct FluidVolumeComponent {
    std::shared_ptr<OpenEye::Fluids::FluidSimulation> simulation;
    glm::ivec3 resolution{64, 64, 64};
    glm::vec3  volumeSize{2.0f, 2.0f, 2.0f};
    bool enabled = true;
};

// Soft body component (cloth, rope, etc.)
struct SoftBodyComponent {
    btSoftBody* softBody = nullptr; // Raw pointer - PhysicsWorld manages lifetime
    std::shared_ptr<Rendering::Mesh> mesh; // Mesh for rendering (updated from soft body nodes)
    bool enabled = true;
    
    // Wind force (for cloth waving)
    glm::vec3 windDirection = glm::vec3(1.0f, 0.0f, 0.0f);
    float windStrength = 0.0f;
    bool windEnabled = false;
    
    // Fixed node indices (for cloth - top edge nodes that should stay fixed)
    std::vector<int> fixedNodeIndices;

    // Volume softbody force controls (also usable for cloth)
    // One-shot impulse request (e.g. "push up" button in editor)
    bool impulseRequested = false;
    glm::vec3 impulseDirection = glm::vec3(0.0f, 1.0f, 0.0f);
    float impulseStrength = 8.0f; // interpreted as velocity impulse scale

    // Optional pulse mode: apply impulse every N seconds
    bool pulseEnabled = false;
    float pulseInterval = 1.0f;   // seconds
    float pulseTimer = 0.0f;      // internal
    
    SoftBodyComponent() = default;
};

// CPU XPBD cloth (lightweight, scalable replacement for deformable cloth)
struct XPBDClothComponent {
    bool enabled = true;

    int widthSegments = 30;
    int heightSegments = 20;
    float width = 3.0f;
    float height = 2.0f;

    // Simulation params
    int iterations = 8;
    float compliance = 0.0005f; // lower = stiffer
    float damping = 0.02f;
    bool enableShear = true;

    // Forces
    glm::vec3 gravity = glm::vec3(0.0f, -9.81f, 0.0f);
    bool windEnabled = true;
    glm::vec3 windDirection = glm::vec3(1.0f, 0.0f, 0.0f);
    float windStrength = 6.0f;

    // Derived/rest lengths
    float restLenX = 0.1f;
    float restLenY = 0.1f;

    // Local-space simulation buffers (entity Transform applies world)
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> prevPositions;
    std::vector<float> invMass;
};

// Rigid-cluster "jelly" approximation rendered as a single deforming mesh (FFD using 3x3x3 control points).
// Physics: many small rigid bodies + constraints. Rendering: one sphere mesh that deforms from node positions.
struct JellyBlobComponent {
    bool enabled = true;

    // Control lattice (expected 3x3x3 for now).
    int dim = 3;
    std::vector<Entity> nodeEntities;                 // size dim^3
    std::vector<btTypedConstraint*> constraints;      // optional, non-owning (PhysicsWorld owns)

    // Spring/constraint tuning (applied each frame if constraints are present)
    float springTau = 0.55f;
    float springDamping = 0.75f;

    // Debug rendering of the underlying rigid nodes
    bool showDebugNodes = false;

    // Rest cage in blob-local space (used to compute (u,v,w) in [0..1]^3).
    glm::vec3 restMin = glm::vec3(-1.0f);
    glm::vec3 restMax = glm::vec3(1.0f);

    // Render mesh info (mesh lives on Renderable, but we cache base vertices for deformation)
    std::vector<Rendering::Vertex> baseVertices;
};

} // namespace ECS
} // namespace OpenEye

