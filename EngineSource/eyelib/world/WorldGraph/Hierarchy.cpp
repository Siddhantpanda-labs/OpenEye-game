#include "Hierarchy.h"

#include <algorithm>

namespace eyelib {
namespace world {

bool Hierarchy::SetParent(EntityId child, EntityId parent) {
    if (child == parent) {
        return false;
    }
    RemoveParent(child);
    m_parent[child] = parent;
    m_children[parent].push_back(child);
    ++m_version;
    return true;
}

void Hierarchy::RemoveParent(EntityId child) {
    auto it = m_parent.find(child);
    if (it == m_parent.end()) {
        return;
    }
    EntityId parent = it->second;
    m_parent.erase(it);

    auto childrenIt = m_children.find(parent);
    if (childrenIt == m_children.end()) {
        return;
    }
    auto& list = childrenIt->second;
    list.erase(std::remove(list.begin(), list.end(), child), list.end());
    if (list.empty()) {
        m_children.erase(childrenIt);
    }
    ++m_version;
}

EntityId Hierarchy::GetParent(EntityId child) const {
    auto it = m_parent.find(child);
    if (it == m_parent.end()) {
        return InvalidEntity;
    }
    return it->second;
}

std::vector<EntityId> Hierarchy::GetChildren(EntityId parent) const {
    auto it = m_children.find(parent);
    if (it == m_children.end()) {
        return {};
    }
    return it->second;
}

bool Hierarchy::WouldCreateCycle(EntityId child, EntityId parent) const {
    if (child == parent) {
        return true;
    }
    EntityId current = parent;
    while (current != InvalidEntity) {
        if (current == child) {
            return true;
        }
        auto it = m_parent.find(current);
        if (it == m_parent.end()) {
            break;
        }
        current = it->second;
    }
    return false;
}

const std::unordered_map<EntityId, EntityId, EntityIdHash>& Hierarchy::Parents() const {
    return m_parent;
}

void Hierarchy::Clear() {
    m_parent.clear();
    m_children.clear();
    ++m_version;
}

} // namespace world
} // namespace eyelib
