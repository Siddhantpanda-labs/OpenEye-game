#pragma once

#include "eyelib/world/EntityId.h"

namespace eyelib {
namespace world {

enum class WorldEventType {
    EntityCreated,
    EntityDestroyed,
    ComponentAdded,
    ComponentRemoved,
    ComponentChanged,
    Reparented
};

struct WorldEvent {
    WorldEventType type = WorldEventType::EntityCreated;
    EntityId entity = InvalidEntity;
};

} // namespace world
} // namespace eyelib
