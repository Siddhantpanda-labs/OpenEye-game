#pragma once

#include "Command.h"

#include <unordered_set>

namespace eyelib {
namespace world {

class CommandValidator {
public:
    bool Validate(const WorldState& state, const Command& command) const;

    void AddRequiredComponent(const std::string& componentName);
    void ClearRequiredComponents();

private:
    bool HasRequiredComponents(const WorldState& state, EntityId entity) const;
    std::unordered_set<std::string> m_requiredComponents;
};

} // namespace world
} // namespace eyelib
