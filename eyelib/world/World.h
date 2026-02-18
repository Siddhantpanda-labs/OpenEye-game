#pragma once

#include "WorldState.h"
#include "Commands/CommandBuffer.h"
#include "Commands/CommandValidator.h"
#include "Events/EventDispatcher.h"
#include "Transactions/TransactionManager.h"
#include "systems/RegionSystem.h"
#include "WorldView.h"
#include "scheduler/WorldScheduler.h"

#include <memory>
#include <optional>

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

private:
    WorldState m_state;
    CommandBuffer m_buffer;
    CommandValidator m_validator;
    EventDispatcher m_events;
    TransactionManager m_transactions;
    systems::RegionSystem m_regionSystem;
    WorldScheduler m_scheduler;
};

} // namespace world
} // namespace eyelib
