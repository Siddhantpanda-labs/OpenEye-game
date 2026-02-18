#pragma once

#include "WorldState.h"
#include "Commands/CommandBuffer.h"
#include "Commands/CommandValidator.h"
#include "Events/EventDispatcher.h"
#include "Events/EventLog.h"
#include "Transactions/TransactionManager.h"
#include "systems/RegionSystem.h"
#include "WorldView.h"
#include "scheduler/WorldScheduler.h"

#include <memory>
#include <optional>
#include <string>
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

    /** All unique system names that appear in any region's SystemBindings. For AI/system introspection. */
    std::vector<std::string> GetSystemList() const;
    /** Enabled system names for a region entity (that has SystemBindings). Empty if entity has no SystemBindings. */
    std::vector<std::string> GetRegionEnabledSystems(EntityId regionEntityId) const;

private:
    WorldState m_state;
    CommandBuffer m_buffer;
    CommandValidator m_validator;
    EventDispatcher m_events;
    EventLog m_eventLog;
    TransactionManager m_transactions;
    systems::RegionSystem m_regionSystem;
    WorldScheduler m_scheduler;
    double m_simulationTime = 0.0;
};

} // namespace world
} // namespace eyelib
