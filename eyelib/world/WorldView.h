#pragma once

#include "WorldState.h"

namespace eyelib {
namespace world {

class WorldView {
public:
    explicit WorldView(const WorldState& state)
        : m_state(&state) {}

    const WorldState& State() const { return *m_state; }

    const EntityRegistry& Entities() const { return m_state->Entities(); }
    const Hierarchy& GraphHierarchy() const { return m_state->GraphHierarchy(); }
    const Metadata& GraphMetadata() const { return m_state->GraphMetadata(); }
    const Tags& GraphTags() const { return m_state->GraphTags(); }

    template<typename T>
    const ComponentStore<T>& Components() const { return m_state->Components<T>(); }

    const std::unordered_map<size_t, std::unique_ptr<IComponentStore>>& ComponentStores() const {
        return m_state->ComponentStores();
    }

private:
    const WorldState* m_state = nullptr;
};

} // namespace world
} // namespace eyelib
