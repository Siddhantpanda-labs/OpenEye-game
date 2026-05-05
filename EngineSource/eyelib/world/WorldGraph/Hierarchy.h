#pragma once

#include "eyelib/world/EntityId.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace eyelib {
namespace world {

class Hierarchy {
public:
    bool SetParent(EntityId child, EntityId parent);
    void RemoveParent(EntityId child);

    EntityId GetParent(EntityId child) const;
    std::vector<EntityId> GetChildren(EntityId parent) const;
    bool WouldCreateCycle(EntityId child, EntityId parent) const;

    const std::unordered_map<EntityId, EntityId, EntityIdHash>& Parents() const;

    void Clear();

    /** Bumped on SetParent, RemoveParent, Clear. Used to invalidate linearized order cache. */
    uint32_t GetVersion() const { return m_version; }

private:
    std::unordered_map<EntityId, EntityId, EntityIdHash> m_parent;
    std::unordered_map<EntityId, std::vector<EntityId>, EntityIdHash> m_children;
    uint32_t m_version = 0;
};

} // namespace world
} // namespace eyelib
