#pragma once

#include <cstddef>
#include <cstdint>

namespace eyelib {
namespace world {

struct EntityId {
    uint32_t index = 0;
    uint32_t generation = 0;

    bool operator==(const EntityId& other) const {
        return index == other.index && generation == other.generation;
    }
    bool operator!=(const EntityId& other) const {
        return !(*this == other);
    }
};

struct EntityIdHash {
    size_t operator()(const EntityId& id) const noexcept {
        return (static_cast<size_t>(id.index) << 32) ^ static_cast<size_t>(id.generation);
    }
};

static constexpr EntityId InvalidEntity { 0xFFFFFFFFu, 0xFFFFFFFFu };

} // namespace world
} // namespace eyelib
