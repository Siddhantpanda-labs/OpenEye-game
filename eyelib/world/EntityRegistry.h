#pragma once

#include "EntityId.h"

#include <vector>

namespace eyelib {
namespace world {

class EntityRegistry {
public:
    EntityRegistry() = default;
    ~EntityRegistry() = default;

    EntityId Create();
    void Destroy(EntityId entity);
    bool IsAlive(EntityId entity) const;

    void Clear();

    const std::vector<uint32_t>& Generations() const { return m_generations; }
    const std::vector<uint32_t>& FreeList() const { return m_freeList; }
    void Restore(const std::vector<uint32_t>& generations, const std::vector<uint32_t>& freeList);
    std::vector<EntityId> GetAliveEntities() const;

private:
    std::vector<uint32_t> m_generations;
    std::vector<uint32_t> m_freeList;
};

} // namespace world
} // namespace eyelib
