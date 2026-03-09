#include "CommandBuffer.h"

namespace eyelib {
namespace world {

void CommandBuffer::Enqueue(std::unique_ptr<Command> command) {
    if (command) {
        m_commands.push_back(std::move(command));
    }
}

bool CommandBuffer::Empty() const {
    return m_commands.empty();
}

void CommandBuffer::Clear() {
    m_commands.clear();
}

std::vector<std::unique_ptr<Command>> CommandBuffer::TakeAll() {
    std::vector<std::unique_ptr<Command>> out;
    out.swap(m_commands);
    return out;
}

} // namespace world
} // namespace eyelib
