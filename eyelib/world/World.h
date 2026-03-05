#pragma once

#include "WorldState.h"
#include "Commands/CommandBuffer.h"
#include "Commands/CommandValidator.h"
#include "Events/EventDispatcher.h"
#include "Events/EventLog.h"
#include "Transactions/TransactionManager.h"
#include "systems/IWorldSystem.h"
#include "WorldView.h"
#include "scheduler/WorldScheduler.h"
#include "scheduler/SystemGraphScheduler.h"
#include "eyelib/math/Transform.h"
#include "eyelib/math/Types.h"
#include "EntityId.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace eyelib {
namespace world {

struct ApplySyncResult {
    bool success = false;
    std::optional<EntityId> createdEntity;
};

class World {
public:
    World();
    ~World();

    World(const World&) = delete;
    World& operator=(const World&) = delete;

    WorldState& GetState();
    const WorldState& GetState() const;

    void SubmitCommand(std::unique_ptr<Command> command);
    /** Enqueues the command and immediately applies all pending commands. When the single command is CreateEntityCommand and it succeeded, createdEntity is set. */
    ApplySyncResult ApplyCommandSync(std::unique_ptr<Command> command);
    bool ApplyPendingCommands();
    void ApplyTransaction(Transaction&& transaction);
    void Update();
    void Update(float deltaSeconds);
    bool UndoLastTransaction();
    bool RedoLastTransaction();
    WorldView GetView() const;

    WorldScheduler& Scheduler();
    const WorldScheduler& Scheduler() const;

    /** Set callback run in RenderSnapshot phase (after PreRender). Used by API/app to capture and set frame snapshot. */
    void SetCaptureSnapshotCallback(std::function<void()> fn);
    /** Per-substep hook: run after Simulation phase inside the fixed-step loop. Must not mutate simulation state—only enqueue events (e.g. collision). */
    void SetPostPhysicsStepCallback(std::function<void()> fn);

    /** Run Simulation phase once (e.g. dt=0) so derived state (world matrices) is up to date. For editor/tools after ApplyCommandSync/ApplyTransaction when immediate world-space reads are needed. Explicit only—no hidden updates in command paths. */
    void FlushSimulation();
    /** Run TransformSystem once only. Recomputes world matrices from local transforms; does not advance accumulator or run physics. Call after transform-modifying commands (e.g. in editor before CaptureRenderSnapshot). */
    void FlushTransforms();

    TransactionManager& Transactions();
    const TransactionManager& Transactions() const;

    EventDispatcher& Events();
    const EventDispatcher& Events() const;

    /** Simulation time (seconds), advanced each Update(deltaSeconds). Used for event timestamps. */
    double GetSimulationTime() const { return m_simulationTime; }

    /** Event log for introspection. Events are appended when commands are applied. */
    const EventLog& GetEventLog() const { return m_eventLog; }
    EventLog& GetEventLog() { return m_eventLog; }

    /** Return all events with sequenceId > sinceSequenceId. */
    std::vector<EventLogEntry> GetEventsSince(uint64_t sinceSequenceId) const;
    /** Return all events with time > sinceTime. */
    std::vector<EventLogEntry> GetEventsSinceTime(double sinceTime) const;

    /** Register a system; it will run in its Phase() with dependency order. Call before first Update(). */
    bool RegisterSystem(std::shared_ptr<IWorldSystem> system);

    /** All unique system names that appear in any region's SystemBindings. For AI/system introspection. */
    std::vector<std::string> GetSystemList() const;
    /** Enabled system names for a region entity (that has SystemBindings). Empty if entity has no SystemBindings. */
    std::vector<std::string> GetRegionEnabledSystems(EntityId regionEntityId) const;

    /** Gravity vector (default 0, -9.8, 0). Applied each substep to non-kinematic RigidBody with useGravity. */
    void SetGravity(const math::Vec3& gravity);
    math::Vec3 GetGravity() const;

    /** Interpolation: previous transforms are captured at the start of each Update(deltaSeconds). Use alpha = 1 - GetSimulationAccumulator()/GetFixedTimestep() (0 = previous, 1 = current). Returns nullopt if entity has no transform. */
    std::optional<math::Transform> GetInterpolatedTransform(EntityId entity, float alpha) const;

    /** Typed component read (from EnTT registry). Mutation must go through commands. */
    template<typename T>
    T* TryGetComponent(EntityId entity) { return GetView().TryGetComponent<T>(entity); }
    template<typename T>
    const T* TryGetComponent(EntityId entity) const { return GetView().TryGetComponent<T>(entity); }
    template<typename T>
    T& GetComponent(EntityId entity) { return GetView().GetComponent<T>(entity); }
    template<typename T>
    const T& GetComponent(EntityId entity) const { return GetView().GetComponent<T>(entity); }
    template<typename T>
    bool HasComponent(EntityId entity) const { return GetView().HasComponent<T>(entity); }

    /** Build a query over the registry. Example: world.CreateQuery().With<Transform>().With<Renderable>().Execute() */
    Query CreateQuery() const { return Query(&m_state); }

    /** Convenience API for Scene/Scripting: create entity and apply name. Returns InvalidEntity on failure. */
    EntityId CreateEntity(const std::string& name = "");
    bool DestroyEntity(EntityId entity);
    /** Reparent child under parent. Default preserveWorldTransform = true (child stays visually in place). */
    bool SetParent(EntityId child, EntityId parent, bool preserveWorldTransform = true);
    EntityId GetParent(EntityId child) const;
    std::vector<EntityId> GetChildren(EntityId parent) const;
    bool IsAlive(EntityId entity) const;
    std::optional<std::string> GetName(EntityId entity) const;
    void SetName(EntityId entity, const std::string& name);
    /** Local transform (relative to parent). */
    std::optional<math::Transform> GetTransform(EntityId entity) const;
    void SetTransform(EntityId entity, const math::Transform& t);
    /** World transform (cached after TransformSystem run). Call FlushTransforms() first if hierarchy/transforms changed. */
    std::optional<math::Transform> GetWorldTransform(EntityId entity) const;
    /** Set world transform; converts to local and stores. Keeps world = parentWorld ⊗ local. */
    void SetWorldTransform(EntityId entity, const math::Transform& worldTr);

private:
    void CopyTransformsToPrevious();

    WorldState m_state;
    CommandBuffer m_buffer;
    CommandValidator m_validator;
    EventDispatcher m_events;
    EventLog m_eventLog;
    TransactionManager m_transactions;
    std::vector<std::shared_ptr<IWorldSystem>> m_systems;
    SystemGraphScheduler m_systemGraph;
    WorldScheduler m_scheduler;
    double m_simulationTime = 0.0;
    std::unordered_map<EntityId, math::Transform, EntityIdHash> m_previousTransforms;
    math::Vec3 m_gravity{0.f, -9.8f, 0.f};
};

} // namespace world
} // namespace eyelib
