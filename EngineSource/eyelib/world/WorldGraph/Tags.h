#pragma once

#include "eyelib/world/EntityId.h"

#include <string>
#include <unordered_map>
#include <unordered_set>

namespace eyelib {
namespace world {

class Tags {
public:
    void Add(EntityId entity, const std::string& tag);
    void Remove(EntityId entity, const std::string& tag);
    bool Has(EntityId entity, const std::string& tag) const;
    const std::unordered_map<EntityId, std::unordered_set<std::string>, EntityIdHash>& All() const;
    void Clear();

private:
    std::unordered_map<EntityId, std::unordered_set<std::string>, EntityIdHash> m_tags;
};

} // namespace world
} // namespace eyelib
