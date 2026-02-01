#pragma once

#include "eyelib/world/World.h"
#include "eyelib/world/components/Camera.h"
#include "eyelib/world/components/Collider.h"
#include "eyelib/world/components/Light.h"
#include "eyelib/world/components/MeshRenderer.h"
#include "eyelib/world/components/Renderable.h"
#include "eyelib/world/components/Animator.h"
#include "eyelib/world/components/RigidBody.h"
#include "eyelib/world/components/Script.h"
#include "eyelib/world/components/Transform.h"

#include <memory>
#include <string>
#include <vector>

namespace eyelib {
namespace api {
namespace world {

class World {
public:
    World();
    ~World();

    World(const World&) = delete;
    World& operator=(const World&) = delete;

    void SubmitCommand(std::unique_ptr<::eyelib::world::Command> command);
    void ApplyPendingCommands();
    void ApplyTransaction(::eyelib::world::Transaction&& transaction);
    void Update();
    void Update(float deltaSeconds);
    bool UndoLastTransaction();
    bool RedoLastTransaction();

    void SetFixedTimestep(float seconds);
    float GetFixedTimestep() const;
    void SetMaxSubsteps(int maxSubsteps);
    int GetMaxSubsteps() const;
    void SetPaused(bool paused);
    void StepOnce();
    void SetTimeScale(float scale);
    float GetTimeScale() const;
    void SetTimeMode(::eyelib::world::WorldScheduler::TimeMode mode);
    ::eyelib::world::WorldScheduler::TimeMode GetTimeMode() const;
    void SetReplayDelta(float seconds);

    enum class AssetRefKind {
        Mesh,
        Material,
        Animation,
        Skeleton,
        Script
    };

    struct AssetReference {
        std::string id;
        AssetRefKind kind = AssetRefKind::Mesh;
    };

    struct TransformEntry {
        ::eyelib::world::EntityId entity;
        ::eyelib::world::components::Transform component;
    };

    struct RenderableEntry {
        ::eyelib::world::EntityId entity;
        ::eyelib::world::components::Renderable component;
    };

    struct MeshRendererEntry {
        ::eyelib::world::EntityId entity;
        ::eyelib::world::components::MeshRenderer component;
    };

    struct CameraEntry {
        ::eyelib::world::EntityId entity;
        ::eyelib::world::components::Camera component;
    };

    struct LightEntry {
        ::eyelib::world::EntityId entity;
        ::eyelib::world::components::Light component;
    };

    struct RigidBodyEntry {
        ::eyelib::world::EntityId entity;
        ::eyelib::world::components::RigidBody component;
    };

    struct ColliderEntry {
        ::eyelib::world::EntityId entity;
        ::eyelib::world::components::Collider component;
    };

    struct AnimatorEntry {
        ::eyelib::world::EntityId entity;
        ::eyelib::world::components::Animator component;
    };

    struct ScriptEntry {
        ::eyelib::world::EntityId entity;
        ::eyelib::world::components::Script component;
    };

    std::vector<AssetReference> CollectAssetReferences() const;
    std::vector<TransformEntry> GetTransforms() const;
    std::vector<RenderableEntry> GetRenderables() const;
    std::vector<MeshRendererEntry> GetMeshRenderers() const;
    std::vector<CameraEntry> GetCameras() const;
    std::vector<LightEntry> GetLights() const;
    std::vector<RigidBodyEntry> GetRigidBodies() const;
    std::vector<ColliderEntry> GetColliders() const;
    std::vector<AnimatorEntry> GetAnimators() const;
    std::vector<ScriptEntry> GetScripts() const;

    bool SetTransform(::eyelib::world::EntityId entity,
                      const ::eyelib::world::components::Transform& transform);
    bool SetRigidBody(::eyelib::world::EntityId entity,
                      const ::eyelib::world::components::RigidBody& rigidBody);
    bool SetScript(::eyelib::world::EntityId entity,
                   const ::eyelib::world::components::Script& script);

    bool LoadScene(const std::string& path);
    size_t GetEntityCount() const;

    ::eyelib::world::World* GetInternal() const;

private:
    class Impl;
    Impl* m_impl = nullptr;
};

} // namespace world
} // namespace api
} // namespace eyelib
