#pragma once

#include "WorldState.h"
#include "Commands/CommandBuffer.h"
#include "Commands/CommandValidator.h"
#include "Events/EventDispatcher.h"
#include "Events/EventLog.h"
#include "Events/PhysicsEvents.h"
#include "Transactions/TransactionManager.h"
#include "systems/RegionSystem.h"
#include "systems/TransformSystem.h"
#include "systems/PhysicsSystem.h"
#include "systems/WorldAABBSystem.h"
#include "systems/BroadPhaseSystem.h"
#include "systems/NarrowPhaseSystem.h"
#include "systems/CollisionResponseSystem.h"
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

    /** Contact events from last frame (per-substep broad phase). Drain with GetAndClearContactEvents() or DrainAndDispatchContactEvents(). */
    std::vector<ContactEvent> GetAndClearContactEvents();
    /** Contact listener: called for each contact when you call DrainAndDispatchContactEvents(). */
    using ContactListener = std::function<void(const ContactEvent&)>;
    using ContactListenerId = uint64_t;
    ContactListenerId AddContactListener(ContactListener listener);
    void RemoveContactListener(ContactListenerId id);
    /** Drains the contact queue, invokes all contact listeners with each event, then clears the queue. */
    void DrainAndDispatchContactEvents();

    /** All unique system names that appear in any region's SystemBindings. For AI/system introspection. */
    std::vector<std::string> GetSystemList() const;
    /** Enabled system names for a region entity (that has SystemBindings). Empty if entity has no SystemBindings. */
    std::vector<std::string> GetRegionEnabledSystems(EntityId regionEntityId) const;

    /** Gravity vector (default 0, -9.8, 0). Applied each substep to non-kinematic RigidBody with useGravity. */
    void SetGravity(const math::Vec3& gravity);
    math::Vec3 GetGravity() const;

    /** Queued; applied at start of next substep. Safe to call from gameplay/AI. */
    void AddForce(EntityId entity, const math::Vec3& force, ForceMode mode);
    /** Queued; applied at start of next substep (velocity += impulse/mass). Safe to call from gameplay/AI. */
    void AddImpulse(EntityId entity, const math::Vec3& impulse);
    /** Queued; applied at start of next substep (accumulatedTorque += torque). */
    void AddTorque(EntityId entity, const math::Vec3& torque);
    /** Queued; applied at start of next substep (angularVelocity += impulse/mass). */
    void AddAngularImpulse(EntityId entity, const math::Vec3& impulse);

    /** Interpolation: previous transforms are captured at the start of each Update(deltaSeconds). Use alpha = 1 - GetSimulationAccumulator()/GetFixedTimestep() (0 = previous, 1 = current). Returns nullopt if entity has no transform. */
    std::optional<math::Transform> GetInterpolatedTransform(EntityId entity, float alpha) const;

private:
    void CopyTransformsToPrevious();

    WorldState m_state;
    CommandBuffer m_buffer;
    CommandValidator m_validator;
    EventDispatcher m_events;
    EventLog m_eventLog;
    TransactionManager m_transactions;
    systems::RegionSystem m_regionSystem;
    systems::TransformSystem m_transformSystem;
    systems::PhysicsSystem m_physicsSystem;
    systems::WorldAABBSystem m_worldAABBSystem;
    systems::BroadPhaseSystem m_broadPhaseSystem;
    systems::NarrowPhaseSystem m_narrowPhaseSystem;
    systems::CollisionResponseSystem m_collisionResponseSystem;
    std::vector<std::pair<EntityId, EntityId>> m_frameCollisionPairs;
    std::vector<ContactEvent> m_frameContactBuffer;
    SystemGraphScheduler m_systemGraph;
    WorldScheduler m_scheduler;
    double m_simulationTime = 0.0;
    std::vector<ContactEvent> m_contactEventQueue;
    std::unordered_map<ContactListenerId, ContactListener> m_contactListeners;
    ContactListenerId m_nextContactListenerId = 1;
    std::unordered_map<EntityId, math::Transform, EntityIdHash> m_previousTransforms;
    math::Vec3 m_gravity{0.f, -9.8f, 0.f};

    std::vector<PendingForceEntry> m_pendingForces;
    std::vector<PendingImpulseEntry> m_pendingImpulses;
    std::vector<PendingTorqueEntry> m_pendingTorques;
    std::vector<PendingAngularImpulseEntry> m_pendingAngularImpulses;
};

} // namespace world
} // namespace eyelib
