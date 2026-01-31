#include "EntityRegistry.h"

namespace eyelib {
namespace world {

EntityId EntityRegistry::Create() {
    if (!m_freeList.empty()) {
        const uint32_t index = m_freeList.back();
        m_freeList.pop_back();
        return EntityId{index, m_generations[index]};
    }
    const uint32_t index = static_cast<uint32_t>(m_generations.size());
    m_generations.push_back(0);
    return EntityId{index, 0};
}

void EntityRegistry::Destroy(EntityId entity) {
    if (!IsAlive(entity)) {
        return;
    }
    ++m_generations[entity.index];
    m_freeList.push_back(entity.index);
}

bool EntityRegistry::IsAlive(EntityId entity) const {
    if (entity.index >= m_generations.size()) {
        return false;
    }
    return m_generations[entity.index] == entity.generation;
}

void EntityRegistry::Clear() {
    m_generations.clear();
    m_freeList.clear();
}

void EntityRegistry::Restore(const std::vector<uint32_t>& generations, const std::vector<uint32_t>& freeList) {
    m_generations = generations;
    m_freeList = freeList;
}

std::vector<EntityId> EntityRegistry::GetAliveEntities() const {
    std::vector<EntityId> alive;
    if (m_generations.empty()) {
        return alive;
    }
    std::vector<bool> isFree(m_generations.size(), false);
    for (uint32_t index : m_freeList) {
        if (index < isFree.size()) {
            isFree[index] = true;
        }
    }
    alive.reserve(m_generations.size());
    for (uint32_t index = 0; index < m_generations.size(); ++index) {
        if (isFree[index]) {
            continue;
        }
        alive.push_back(EntityId{index, m_generations[index]});
    }
    return alive;
}

} // namespace world
} // namespace eyelib
