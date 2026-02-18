#include "World.h"

#include "Commands/CommandTypes.h"
#include "Events/WorldEvents.h"

namespace eyelib {
namespace world {

World::World() {
    m_scheduler.ClearSteps();
    m_scheduler.SetFixedTimestep(1.0f / 60.0f);
    m_scheduler.AddStep("mutation", "core", 0, [this](float /*dt*/) { ApplyPendingCommands(); });
    m_scheduler.AddStep("region", "core", 10, [this](float /*dt*/) { m_regionSystem.Update(m_state); });
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

TransactionManager& World::Transactions() { return m_transactions; }
const TransactionManager& World::Transactions() const { return m_transactions; }

EventDispatcher& World::Events() { return m_events; }
const EventDispatcher& World::Events() const { return m_events; }

} // namespace world
} // namespace eyelib
