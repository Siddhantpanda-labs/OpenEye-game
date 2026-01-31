#pragma once

#include "eyelib/world/WorldState.h"

#include <string>

namespace eyelib {
namespace world {

class Command {
public:
    virtual ~Command() = default;

    virtual bool Apply(WorldState& state) = 0;
    virtual bool Revert(WorldState& state) = 0;
    virtual std::string GetName() const = 0;
};

} // namespace world
} // namespace eyelib
