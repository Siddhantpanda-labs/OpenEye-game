#include "Transaction.h"

namespace eyelib {
namespace world {

void Transaction::Add(std::unique_ptr<Command> command) {
    if (command) {
        m_commands.push_back(std::move(command));
    }
}

std::vector<std::unique_ptr<Command>> Transaction::TakeCommands() {
    std::vector<std::unique_ptr<Command>> out;
    out.swap(m_commands);
    return out;
}

bool Transaction::Apply(WorldState& state) {
    bool ok = true;
    for (auto& command : m_commands) {
        if (!command) {
            continue;
        }
        ok = command->Apply(state) && ok;
    }
    return ok;
}

bool Transaction::Revert(WorldState& state) {
    bool ok = true;
    for (auto it = m_commands.rbegin(); it != m_commands.rend(); ++it) {
        if (!(*it)) {
            continue;
        }
        ok = (*it)->Revert(state) && ok;
    }
    return ok;
}

} // namespace world
} // namespace eyelib
