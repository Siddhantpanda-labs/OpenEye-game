#pragma once

#include "eyelib/world/EntityId.h"

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

private:
    std::unordered_map<EntityId, EntityId, EntityIdHash> m_parent;
    std::unordered_map<EntityId, std::vector<EntityId>, EntityIdHash> m_children;
};

} // namespace world
} // namespace eyelib
