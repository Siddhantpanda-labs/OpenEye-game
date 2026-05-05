#pragma once

#include "eyelib/world/Commands/Command.h"

#include <memory>
#include <vector>

namespace eyelib {
namespace world {

class Transaction {
public:
    void Add(std::unique_ptr<Command> command);
    std::vector<std::unique_ptr<Command>> TakeCommands();
    bool Apply(WorldState& state);
    bool Revert(WorldState& state);

private:
    std::vector<std::unique_ptr<Command>> m_commands;
};

} // namespace world
} // namespace eyelib
