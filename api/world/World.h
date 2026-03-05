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
#include "eyelib/world/components/Name.h"
#include "eyelib/world/components/Region.h"
#include "eyelib/world/components/RegionMembership.h"
#include "eyelib/world/components/AABB.h"
#include "eyelib/world/components/SystemBindings.h"
#include "eyelib/world/components/ActiveSystems.h"
#include "eyelib/world/EntityId.h"
#include "eyelib/world/Events/EventLog.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace eyelib {
namespace api {
namespace world {

class Observation;

class World {
public:
    World();
    ~World();

    World(const World&) = delete;
    World& operator=(const World&) = delete;

    void SubmitCommand(std::unique_ptr<::eyelib::world::Command> command);
    /** Enqueue and apply one command immediately. When the command is CreateEntity and it succeeds, \a createdEntity is set. */
    struct ApplySyncResult {
        bool success = false;
        std::optional<::eyelib::world::EntityId> createdEntity;
    };
    ApplySyncResult ApplyCommandSync(std::unique_ptr<::eyelib::world::Command> command);
    void ApplyPendingCommands();
    void ApplyTransaction(::eyelib::world::Transaction&& transaction);
    /** Run Simulation phase once (dt=0) so derived state (world matrices) is up to date. Call after ApplyCommandSync/ApplyTransaction when editor or tools need immediate world-space reads. Explicit only. */
    void FlushSimulation();
    /** Run TransformSystem once (world matrices from local transforms). Does not advance accumulator or run physics. Call after transform-modifying commands, e.g. before CaptureRenderSnapshot in editor. */
    void FlushTransforms();
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

    /** Gravity vector (default 0, -9.8, 0). Use physics backend (e.g. PhysicsWorld::SetGravity) for simulation; this is for compatibility. */
    void SetGravity(const ::eyelib::math::Vec3& gravity);
    ::eyelib::math::Vec3 GetGravity() const;

    /** Simulation time (seconds), advanced each Update(deltaSeconds). For event timestamps and AI observation. */
    double GetSimulationTime() const;
    /** Accumulator after last Update (remaining time before next fixed step). For interpolation: alpha = 1 - GetSimulationAccumulator()/GetFixedTimestep() (0 = current step, 1 = next). */
    double GetSimulationAccumulator() const;
    /** Interpolation: previous transforms captured at start of each Update(deltaSeconds). Use alpha = 1 - GetSimulationAccumulator()/GetFixedTimestep() (0 = previous, 1 = current). Nullopt if entity has no transform. */
    std::optional<::eyelib::world::components::Transform> GetInterpolatedTransform(::eyelib::world::EntityId entity, float alpha) const;
    /** Events with sequenceId > sinceSequenceId. Use 0 for all events in the log. */
    std::vector<::eyelib::world::EventLogEntry> GetEventsSince(uint64_t sinceSequenceId) const;
    /** Events with time > sinceTime. */
    std::vector<::eyelib::world::EventLogEntry> GetEventsSinceTime(double sinceTime) const;

    /** Per-substep hook: run after Simulation phase inside the fixed-step loop. Must not mutate simulation state. */
    void SetPostPhysicsStepCallback(std::function<void()> fn);

    /** All unique system names that appear in any region's SystemBindings. For AI/system introspection. */
    std::vector<std::string> GetSystemList() const;
    /** Enabled system names for a region entity (that has SystemBindings). Empty if entity has no SystemBindings. */
    std::vector<std::string> GetRegionEnabledSystems(::eyelib::world::EntityId regionEntityId) const;

    ::eyelib::world::EntityId CreateEntity(const std::string& name = "");
    bool DestroyEntity(::eyelib::world::EntityId entity);
    /** Reparent child under parent. Default preserveWorldTransform = true (child stays visually in place, then follows parent). */
    bool SetParent(::eyelib::world::EntityId child, ::eyelib::world::EntityId parent, bool preserveWorldTransform = true);
    void ClearParent(::eyelib::world::EntityId child, bool preserveWorldTransform = true);
    ::eyelib::world::EntityId GetParent(::eyelib::world::EntityId child) const;
    std::vector<::eyelib::world::EntityId> GetChildren(::eyelib::world::EntityId parent) const;
    std::vector<::eyelib::world::EntityId> GetAncestors(::eyelib::world::EntityId entity) const;
    std::vector<::eyelib::world::EntityId> GetDescendants(::eyelib::world::EntityId entity) const;

    void AddTag(::eyelib::world::EntityId entity, const std::string& tag);
    void RemoveTag(::eyelib::world::EntityId entity, const std::string& tag);
    bool HasTag(::eyelib::world::EntityId entity, const std::string& tag) const;
    std::vector<std::string> GetTags(::eyelib::world::EntityId entity) const;
    std::vector<::eyelib::world::EntityId> GetEntitiesWithTag(const std::string& tag) const;
    std::vector<::eyelib::world::EntityId> GetEntitiesWithTags(const std::vector<std::string>& tags) const;

    bool SetMetadata(::eyelib::world::EntityId entity, const std::string& name, uint32_t flags);
    bool GetMetadata(::eyelib::world::EntityId entity, std::string& name, uint32_t& flags) const;
    bool ClearMetadata(::eyelib::world::EntityId entity);
    bool GetEntityName(::eyelib::world::EntityId entity, std::string& name) const;
    bool SetEntityName(::eyelib::world::EntityId entity, const std::string& name);

    ::eyelib::world::EntityId FindEntityByName(const std::string& name) const;
    std::vector<::eyelib::world::EntityId> FindEntitiesByName(const std::string& name) const;
    std::vector<::eyelib::world::EntityId> FindEntitiesByNameContains(const std::string& namePart) const;
    std::vector<::eyelib::world::EntityId> FindEntitiesByFlags(uint32_t flagsMask, bool exactMatch = false) const;

    std::vector<::eyelib::world::EntityId> GetEntities() const;
    std::vector<::eyelib::world::EntityId> GetEntitiesWithComponent(const std::string& componentName) const;
    std::vector<::eyelib::world::EntityId> GetEntitiesWithComponents(
        const std::vector<std::string>& componentNames) const;
    std::vector<std::string> GetComponentNames(::eyelib::world::EntityId entity) const;

    bool GetComponentJson(::eyelib::world::EntityId entity,
                          const std::string& componentName,
                          std::string& outJson) const;
    bool SetComponentJson(::eyelib::world::EntityId entity,
                          const std::string& componentName,
                          const std::string& json);

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

    /** Immutable frame snapshot: one capture per frame. Filled by CaptureRenderSnapshot(); read-only after that. */
    struct RenderSnapshot {
        std::vector<RenderableEntry> renderables;
        std::vector<MeshRendererEntry> meshRenderers;
        std::vector<CameraEntry> cameras;
        std::vector<LightEntry> lights;
        std::vector<RigidBodyEntry> rigidBodies;
        std::vector<ColliderEntry> colliders;
        std::unordered_map<::eyelib::world::EntityId, ::eyelib::world::components::Transform, ::eyelib::world::EntityIdHash> transformCache;
        std::vector<TransformEntry> transforms;
        std::vector<::eyelib::world::EntityId> transformEntityIds;
        std::vector<::eyelib::math::Vec3> transformPositions;
        std::vector<::eyelib::math::Quat> transformRotations;
        std::vector<::eyelib::math::Vec3> transformScales;

        std::optional<::eyelib::world::components::Transform> GetTransform(::eyelib::world::EntityId entity) const;
    };

    /** One capture per frame: fills \a out from world state (one pass). Call once per frame; then use \a out for BuildScene and set as frame snapshot so GetObservation() returns a view over it. */
    void CaptureRenderSnapshot(RenderSnapshot& out) const;
    /** Set the current frame snapshot. GetObservation() will return a view over it until cleared. Call at frame start after CaptureRenderSnapshot; clear at frame end. */
    void SetFrameSnapshot(const RenderSnapshot* snapshot);
    /** Set callback run in RenderSnapshot phase (after PreRender). Use to capture and set frame snapshot so BuildScene sees stable world. */
    void SetCaptureSnapshotCallback(std::function<void()> fn);
    const RenderSnapshot* GetFrameSnapshot() const { return m_frameSnapshot; }

    struct AnimatorEntry {
        ::eyelib::world::EntityId entity;
        ::eyelib::world::components::Animator component;
    };

    struct ScriptEntry {
        ::eyelib::world::EntityId entity;
        ::eyelib::world::components::Script component;
    };

    std::vector<AssetReference> CollectAssetReferences() const;
    std::vector<AnimatorEntry> GetAnimators() const;
    std::vector<ScriptEntry> GetScripts() const;

    bool IsAlive(::eyelib::world::EntityId entity) const;
    /** Local transform (relative to parent). */
    bool GetTransform(::eyelib::world::EntityId entity,
                      ::eyelib::world::components::Transform& out) const;
    /** World transform. Call FlushTransforms() first if hierarchy/transforms changed. */
    bool GetWorldTransform(::eyelib::world::EntityId entity,
                          ::eyelib::world::components::Transform& out) const;
    bool GetCamera(::eyelib::world::EntityId entity,
                   ::eyelib::world::components::Camera& out) const;
    bool GetLight(::eyelib::world::EntityId entity,
                  ::eyelib::world::components::Light& out) const;
    bool GetRenderable(::eyelib::world::EntityId entity,
                       ::eyelib::world::components::Renderable& out) const;
    bool GetMeshRenderer(::eyelib::world::EntityId entity,
                         ::eyelib::world::components::MeshRenderer& out) const;
    bool GetCollider(::eyelib::world::EntityId entity,
                     ::eyelib::world::components::Collider& out) const;
    bool GetScript(::eyelib::world::EntityId entity,
                   ::eyelib::world::components::Script& out) const;
    bool GetRigidBody(::eyelib::world::EntityId entity,
                      ::eyelib::world::components::RigidBody& out) const;
    bool GetAnimator(::eyelib::world::EntityId entity,
                     ::eyelib::world::components::Animator& out) const;
    bool GetName(::eyelib::world::EntityId entity,
                 ::eyelib::world::components::Name& out) const;
    bool GetRegion(::eyelib::world::EntityId entity,
                   ::eyelib::world::components::Region& out) const;
    bool GetRegionMembership(::eyelib::world::EntityId entity,
                             ::eyelib::world::components::RegionMembership& out) const;
    bool GetAABB(::eyelib::world::EntityId entity,
                 ::eyelib::world::components::AABB& out) const;
    bool GetSystemBindings(::eyelib::world::EntityId entity,
                           ::eyelib::world::components::SystemBindings& out) const;
    bool GetActiveSystems(::eyelib::world::EntityId entity,
                          ::eyelib::world::components::ActiveSystems& out) const;

    bool SetTransform(::eyelib::world::EntityId entity,
                      const ::eyelib::world::components::Transform& transform);
    /** Set world transform; converts to local and stores. Keeps hierarchy consistent. */
    bool SetWorldTransform(::eyelib::world::EntityId entity,
                          const ::eyelib::world::components::Transform& worldTransform);
    bool SetCamera(::eyelib::world::EntityId entity,
                   const ::eyelib::world::components::Camera& camera);
    bool SetLight(::eyelib::world::EntityId entity,
                  const ::eyelib::world::components::Light& light);
    bool SetRenderable(::eyelib::world::EntityId entity,
                       const ::eyelib::world::components::Renderable& renderable);
    bool SetMeshRenderer(::eyelib::world::EntityId entity,
                         const ::eyelib::world::components::MeshRenderer& renderer);
    bool SetCollider(::eyelib::world::EntityId entity,
                     const ::eyelib::world::components::Collider& collider);
    bool SetScript(::eyelib::world::EntityId entity,
                   const ::eyelib::world::components::Script& script);
    bool SetRigidBody(::eyelib::world::EntityId entity,
                      const ::eyelib::world::components::RigidBody& rigidBody);
    bool SetAnimator(::eyelib::world::EntityId entity,
                     const ::eyelib::world::components::Animator& animator);
    bool SetName(::eyelib::world::EntityId entity,
                 const ::eyelib::world::components::Name& name);
    bool SetRegion(::eyelib::world::EntityId entity,
                   const ::eyelib::world::components::Region& region);
    bool SetRegionMembership(::eyelib::world::EntityId entity,
                             const ::eyelib::world::components::RegionMembership& membership);
    bool SetAABB(::eyelib::world::EntityId entity,
                 const ::eyelib::world::components::AABB& aabb);
    bool SetSystemBindings(::eyelib::world::EntityId entity,
                           const ::eyelib::world::components::SystemBindings& bindings);
    bool SetActiveSystems(::eyelib::world::EntityId entity,
                          const ::eyelib::world::components::ActiveSystems& systems);

    bool LoadScene(const std::string& path);
    /** Save current world state to JSON scene file. Returns false on write error. */
    bool SaveScene(const std::string& path) const;
    size_t GetEntityCount() const;

    /** Scene-level skybox asset id (e.g. "Skybox/Citadella"). Empty = no skybox. Serialized in scene file. */
    std::string GetSkyboxAssetId() const;
    void SetSkyboxAssetId(const std::string& assetId);

    Observation GetObservation() const;

    ::eyelib::world::World* GetInternal() const;

private:
    class Impl;
    Impl* m_impl = nullptr;
    const RenderSnapshot* m_frameSnapshot = nullptr;
};

} // namespace world
} // namespace api
} // namespace eyelib
