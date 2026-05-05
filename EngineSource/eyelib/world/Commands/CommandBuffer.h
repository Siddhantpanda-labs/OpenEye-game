#pragma once

#include "Command.h"

#include <memory>
#include <vector>

namespace eyelib {
namespace world {

class CommandBuffer {
public:
    void Enqueue(std::unique_ptr<Command> command);
    bool Empty() const;
    void Clear();

    std::vector<std::unique_ptr<Command>> TakeAll();

private:
    std::vector<std::unique_ptr<Command>> m_commands;
};

} // namespace world
} // namespace eyelib
