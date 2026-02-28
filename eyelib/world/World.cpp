#include "World.h"

#include "Commands/CommandTypes.h"
#include "eyelib/world/components/RigidBody.h"
#include "Events/WorldEvents.h"
#include "eyelib/world/components/SystemBindings.h"
#include "eyelib/world/ComponentStorage/TransformStore.h"
#include "eyelib/math/Interpolation.h"
#include <unordered_set>

namespace eyelib {
namespace world {

World::World() {
    m_scheduler.SetSystemGraph(&m_systemGraph);
    m_systemGraph.SetFixedTimestep(1.0f / 60.0f);
    // ProcessCommands: apply pending commands only. No derived system updates; those run in Simulation phase.
    m_systemGraph.SetProcessCommandsCallback([this]() { ApplyPendingCommands(); });
    m_systemGraph.Register(SystemDescriptor{
        "region",
        SchedulePhase::PreSimulation,
        {},
        {},
        false,
        [this](float /*dt*/) { m_regionSystem.Update(m_state); }
    });
    // TransformSystem runs in Simulation phase inside the fixed-step loop. Authority rule: only Simulation-phase systems mutate authoritative sim state.
    m_systemGraph.Register(SystemDescriptor{
        "TransformSystem",
        SchedulePhase::Simulation,
        {},
        {"PhysicsSystem"},
        true,
        [this](float /*dt*/) { m_transformSystem.Update(m_state); }
    });
    // PhysicsSystem: integrate velocity -> position, write local transform. Runs after TransformSystem.
    m_systemGraph.Register(SystemDescriptor{
        "PhysicsSystem",
        SchedulePhase::Simulation,
        {"TransformSystem"},
        {"TransformSystemPostPhysics"},
        true,
        [this](float dt) { m_physicsSystem.Update(m_state, dt, m_gravity, m_pendingForces, m_pendingImpulses, m_pendingTorques, m_pendingAngularImpulses); }
    });
    // Second TransformSystem pass after physics write-back so world matrices are up to date.
    m_systemGraph.Register(SystemDescriptor{
        "TransformSystemPostPhysics",
        SchedulePhase::Simulation,
        {"PhysicsSystem"},
        {"WorldAABBSystem"},
        true,
        [this](float /*dt*/) { m_transformSystem.Update(m_state); }
    });
    m_systemGraph.Register(SystemDescriptor{
        "WorldAABBSystem",
        SchedulePhase::Simulation,
        {"TransformSystemPostPhysics"},
        {"BroadPhaseSystem"},
        true,
        [this](float /*dt*/) { m_worldAABBSystem.Update(m_state); }
    });
    m_systemGraph.Register(SystemDescriptor{
        "BroadPhaseSystem",
        SchedulePhase::Simulation,
        {"WorldAABBSystem"},
        {"NarrowPhaseSystem"},
        true,
        [this](float /*dt*/) { m_broadPhaseSystem.Update(m_state, m_frameCollisionPairs); }
    });
    m_systemGraph.Register(SystemDescriptor{
        "NarrowPhaseSystem",
        SchedulePhase::Simulation,
        {"BroadPhaseSystem"},
        {"CollisionResponseSystem"},
        true,
        [this](float /*dt*/) { m_narrowPhaseSystem.Update(m_state, m_frameCollisionPairs, m_frameContactBuffer); }
    });
    m_systemGraph.Register(SystemDescriptor{
        "CollisionResponseSystem",
        SchedulePhase::Simulation,
        {"NarrowPhaseSystem"},
        {},
        true,
        [this](float /*dt*/) {
            m_collisionResponseSystem.Update(m_state, m_frameContactBuffer, [this](const ContactEvent& e) {
                m_contactEventQueue.push_back(e);
            });
        }
    });
    m_systemGraph.SetPostPhysicsStepCallback(nullptr);
    m_events.SetEventLog(&m_eventLog, [this]() { return m_simulationTime; });
}
World::~World() = default;

WorldState& World::GetState() { return m_state; }
const WorldState& World::GetState() const { return m_state; }

void World::SubmitCommand(std::unique_ptr<Command> command) {
    if (m_transactions.IsOpen()) {
        m_transactions.Add(std::move(command));
        return;
    }
    m_buffer.Enqueue(std::move(command));
}

ApplySyncResult World::ApplyCommandSync(std::unique_ptr<Command> command) {
    ApplySyncResult result{};
    if (!command) {
        result.success = ApplyPendingCommands();
        return result;
    }
    m_buffer.Enqueue(std::move(command));
    auto commands = m_buffer.TakeAll();
    bool allOk = true;
    for (auto& cmd : commands) {
        if (!cmd) continue;
        if (!m_validator.Validate(m_state, *cmd)) {
            allOk = false;
            continue;
        }
        if (cmd->Apply(m_state)) {
            if (const auto* create = dynamic_cast<const CreateEntityCommand*>(cmd.get())) {
                if (create->GetEntity().has_value()) {
                    m_events.Emit(WorldEvent{ WorldEventType::EntityCreated, *create->GetEntity() });
                    if (commands.size() == 1u) {
                        result.createdEntity = *create->GetEntity();
                    }
                }
            } else if (const auto* createRegion = dynamic_cast<const CreateRegionCommand*>(cmd.get())) {
                if (createRegion->GetEntity().has_value()) {
                    m_events.Emit(WorldEvent{ WorldEventType::EntityCreated, *createRegion->GetEntity() });
                    m_events.Emit(WorldEvent{ WorldEventType::ComponentAdded, *createRegion->GetEntity() });
                }
            } else if (const auto* destroy = dynamic_cast<const DestroyEntityCommand*>(cmd.get())) {
                m_events.Emit(WorldEvent{ WorldEventType::EntityDestroyed, destroy->GetEntity() });
            } else if (const auto* reparent = dynamic_cast<const ReparentEntityCommand*>(cmd.get())) {
                m_events.Emit(WorldEvent{ WorldEventType::Reparented, reparent->GetChild() });
            } else if (const auto* setName = dynamic_cast<const SetNameCommand*>(cmd.get())) {
                m_events.Emit(WorldEvent{ WorldEventType::ComponentChanged, setName->GetEntity() });
            } else if (const auto* setRegion = dynamic_cast<const SetRegionDataCommand*>(cmd.get())) {
                m_events.Emit(WorldEvent{ WorldEventType::ComponentChanged, setRegion->GetEntity() });
            } else if (const auto* attachRules = dynamic_cast<const AttachRegionRulesCommand*>(cmd.get())) {
                m_events.Emit(WorldEvent{ WorldEventType::ComponentChanged, attachRules->GetEntity() });
            } else if (const auto* enableSystem = dynamic_cast<const EnableSystemInRegionCommand*>(cmd.get())) {
                m_events.Emit(WorldEvent{ WorldEventType::ComponentChanged, enableSystem->GetEntity() });
            } else if (const auto* setTransform = dynamic_cast<const SetTransformCommand*>(cmd.get())) {
                m_events.Emit(WorldEvent{ WorldEventType::ComponentChanged, setTransform->GetEntity() });
            } else if (const auto* comp = dynamic_cast<const ComponentCommandBase*>(cmd.get())) {
                m_events.Emit(WorldEvent{ comp->GetComponentEventType(), comp->GetEntity() });
            }
        } else {
            allOk = false;
        }
    }
    result.success = allOk;
    return result;
}

bool World::ApplyPendingCommands() {
    auto commands = m_buffer.TakeAll();
    bool allOk = true;
    for (auto& command : commands) {
        if (!command) {
            continue;
        }
        if (!m_validator.Validate(m_state, *command)) {
            allOk = false;
            continue;
        }
        if (command->Apply(m_state)) {
            if (const auto* create = dynamic_cast<const CreateEntityCommand*>(command.get())) {
                if (create->GetEntity().has_value()) {
                    m_events.Emit(WorldEvent{ WorldEventType::EntityCreated, *create->GetEntity() });
                }
            } else if (const auto* createRegion = dynamic_cast<const CreateRegionCommand*>(command.get())) {
                if (createRegion->GetEntity().has_value()) {
                    m_events.Emit(WorldEvent{ WorldEventType::EntityCreated, *createRegion->GetEntity() });
                    m_events.Emit(WorldEvent{ WorldEventType::ComponentAdded, *createRegion->GetEntity() });
                }
            } else if (const auto* destroy = dynamic_cast<const DestroyEntityCommand*>(command.get())) {
                m_events.Emit(WorldEvent{ WorldEventType::EntityDestroyed, destroy->GetEntity() });
            } else if (const auto* reparent = dynamic_cast<const ReparentEntityCommand*>(command.get())) {
                m_events.Emit(WorldEvent{ WorldEventType::Reparented, reparent->GetChild() });
            } else if (const auto* setName = dynamic_cast<const SetNameCommand*>(command.get())) {
                m_events.Emit(WorldEvent{ WorldEventType::ComponentChanged, setName->GetEntity() });
            } else if (const auto* setRegion = dynamic_cast<const SetRegionDataCommand*>(command.get())) {
                m_events.Emit(WorldEvent{ WorldEventType::ComponentChanged, setRegion->GetEntity() });
            } else if (const auto* attachRules = dynamic_cast<const AttachRegionRulesCommand*>(command.get())) {
                m_events.Emit(WorldEvent{ WorldEventType::ComponentChanged, attachRules->GetEntity() });
            } else if (const auto* enableSystem = dynamic_cast<const EnableSystemInRegionCommand*>(command.get())) {
                m_events.Emit(WorldEvent{ WorldEventType::ComponentChanged, enableSystem->GetEntity() });
            } else if (const auto* setTransform = dynamic_cast<const SetTransformCommand*>(command.get())) {
                m_events.Emit(WorldEvent{ WorldEventType::ComponentChanged, setTransform->GetEntity() });
            } else if (const auto* comp = dynamic_cast<const ComponentCommandBase*>(command.get())) {
                m_events.Emit(WorldEvent{ comp->GetComponentEventType(), comp->GetEntity() });
            }
        } else {
            allOk = false;
        }
    }
    return allOk;
}

void World::ApplyTransaction(Transaction&& transaction) {
    auto commands = transaction.TakeCommands();
    std::vector<std::unique_ptr<Command>> appliedCommands;
    appliedCommands.reserve(commands.size());
    for (auto& command : commands) {
        if (!command) {
            continue;
        }
        if (!m_validator.Validate(m_state, *command)) {
            continue;
        }
        if (command->Apply(m_state)) {
            if (const auto* create = dynamic_cast<const CreateEntityCommand*>(command.get())) {
                if (create->GetEntity().has_value()) {
                    m_events.Emit(WorldEvent{ WorldEventType::EntityCreated, *create->GetEntity() });
                }
            } else if (const auto* createRegion = dynamic_cast<const CreateRegionCommand*>(command.get())) {
                if (createRegion->GetEntity().has_value()) {
                    m_events.Emit(WorldEvent{ WorldEventType::EntityCreated, *createRegion->GetEntity() });
                    m_events.Emit(WorldEvent{ WorldEventType::ComponentAdded, *createRegion->GetEntity() });
                }
            } else if (const auto* destroy = dynamic_cast<const DestroyEntityCommand*>(command.get())) {
                m_events.Emit(WorldEvent{ WorldEventType::EntityDestroyed, destroy->GetEntity() });
            } else if (const auto* reparent = dynamic_cast<const ReparentEntityCommand*>(command.get())) {
                m_events.Emit(WorldEvent{ WorldEventType::Reparented, reparent->GetChild() });
            } else if (const auto* setName = dynamic_cast<const SetNameCommand*>(command.get())) {
                m_events.Emit(WorldEvent{ WorldEventType::ComponentChanged, setName->GetEntity() });
            } else if (const auto* setRegion = dynamic_cast<const SetRegionDataCommand*>(command.get())) {
                m_events.Emit(WorldEvent{ WorldEventType::ComponentChanged, setRegion->GetEntity() });
            } else if (const auto* attachRules = dynamic_cast<const AttachRegionRulesCommand*>(command.get())) {
                m_events.Emit(WorldEvent{ WorldEventType::ComponentChanged, attachRules->GetEntity() });
            } else if (const auto* enableSystem = dynamic_cast<const EnableSystemInRegionCommand*>(command.get())) {
                m_events.Emit(WorldEvent{ WorldEventType::ComponentChanged, enableSystem->GetEntity() });
            } else if (const auto* comp = dynamic_cast<const ComponentCommandBase*>(command.get())) {
                m_events.Emit(WorldEvent{ comp->GetComponentEventType(), comp->GetEntity() });
            }
            appliedCommands.push_back(std::move(command));
        }
    }
    Transaction applied;
    for (auto& command : appliedCommands) {
        applied.Add(std::move(command));
    }
    m_transactions.RecordApplied(std::move(applied));
}

void World::Update() {
    m_scheduler.Run(0.0f);
}

void World::Update(float deltaSeconds) {
    m_simulationTime += deltaSeconds;
    CopyTransformsToPrevious();
    m_scheduler.Run(deltaSeconds);
}

bool World::UndoLastTransaction() {
    return m_transactions.Undo(m_state);
}

bool World::RedoLastTransaction() {
    return m_transactions.Redo(m_state);
}

WorldView World::GetView() const {
    return WorldView(m_state);
}

WorldScheduler& World::Scheduler() { return m_scheduler; }
const WorldScheduler& World::Scheduler() const { return m_scheduler; }

void World::SetCaptureSnapshotCallback(std::function<void()> fn) {
    m_systemGraph.SetCaptureSnapshotCallback(std::move(fn));
}

void World::SetPostPhysicsStepCallback(std::function<void()> fn) {
    m_systemGraph.SetPostPhysicsStepCallback(std::move(fn));
}

void World::FlushSimulation() {
    m_systemGraph.RunSimulationPhaseOnce(0.0f);
}

void World::FlushTransforms() {
    m_transformSystem.Update(m_state);
}

TransactionManager& World::Transactions() { return m_transactions; }
const TransactionManager& World::Transactions() const { return m_transactions; }

EventDispatcher& World::Events() { return m_events; }
const EventDispatcher& World::Events() const { return m_events; }

std::vector<EventLogEntry> World::GetEventsSince(uint64_t sinceSequenceId) const {
    return m_eventLog.GetEventsSince(sinceSequenceId);
}

std::vector<EventLogEntry> World::GetEventsSinceTime(double sinceTime) const {
    return m_eventLog.GetEventsSinceTime(sinceTime);
}

std::vector<std::string> World::GetSystemList() const {
    std::unordered_set<std::string> seen;
    const auto& store = m_state.Components<components::SystemBindings>();
    for (const auto& [entity, bindings] : store.All()) {
        (void)entity;
        for (const auto& name : bindings.enabledSystems) {
            seen.insert(name);
        }
    }
    return std::vector<std::string>(seen.begin(), seen.end());
}

std::vector<std::string> World::GetRegionEnabledSystems(EntityId regionEntityId) const {
    const auto& store = m_state.Components<components::SystemBindings>();
    const auto* bindings = store.TryGet(regionEntityId);
    if (!bindings) return {};
    return bindings->enabledSystems;
}

void World::SetGravity(const math::Vec3& gravity) {
    m_gravity = gravity;
}

math::Vec3 World::GetGravity() const {
    return m_gravity;
}

void World::AddForce(EntityId entity, const math::Vec3& force, ForceMode mode) {
    m_pendingForces.push_back(PendingForceEntry{entity, force, mode});
}

void World::AddImpulse(EntityId entity, const math::Vec3& impulse) {
    m_pendingImpulses.push_back(PendingImpulseEntry{entity, impulse});
}

void World::AddTorque(EntityId entity, const math::Vec3& torque) {
    m_pendingTorques.push_back(PendingTorqueEntry{entity, torque});
}

void World::AddAngularImpulse(EntityId entity, const math::Vec3& impulse) {
    m_pendingAngularImpulses.push_back(PendingAngularImpulseEntry{entity, impulse});
}

std::vector<ContactEvent> World::GetAndClearContactEvents() {
    return std::move(m_contactEventQueue);
}

World::ContactListenerId World::AddContactListener(ContactListener listener) {
    ContactListenerId id = m_nextContactListenerId++;
    m_contactListeners[id] = std::move(listener);
    return id;
}

void World::RemoveContactListener(ContactListenerId id) {
    m_contactListeners.erase(id);
}

void World::DrainAndDispatchContactEvents() {
    std::vector<ContactEvent> events = std::move(m_contactEventQueue);
    m_contactEventQueue.clear();
    for (const ContactEvent& e : events)
        for (auto& [id, fn] : m_contactListeners)
            fn(e);
}

void World::CopyTransformsToPrevious() {
    auto& store = m_state.GetTransformStore();
    const auto& rigidBodies = m_state.Components<components::RigidBody>();
    m_previousTransforms.clear();
    for (const auto& [entity, rb] : rigidBodies.All()) {
        const auto* ref = store.TryGet(entity);
        if (ref)
            m_previousTransforms[entity] = static_cast<math::Transform>(*ref);
    }
}

std::optional<math::Transform> World::GetInterpolatedTransform(EntityId entity, float alpha) const {
    const auto& store = m_state.GetTransformStore();
    const auto* ref = store.TryGet(entity);
    if (!ref) return std::nullopt;
    math::Transform current = static_cast<math::Transform>(*ref);
    auto it = m_previousTransforms.find(entity);
    if (it == m_previousTransforms.end())
        return current;
    const math::Transform& prev = it->second;
    math::Transform out;
    out.position = math::Lerp(prev.position, current.position, alpha);
    out.rotation = math::Slerp(prev.rotation, current.rotation, alpha);
    out.scale = math::Lerp(prev.scale, current.scale, alpha);
    return out;
}

} // namespace world
} // namespace eyelib
