#pragma once

#include "eyelib/world/EntityId.h"

#include <optional>
#include <string>
#include <unordered_map>

namespace eyelib {
namespace world {

struct MetadataEntry {
    std::string name;
    uint32_t flags = 0;
};

class Metadata {
public:
    void Set(EntityId entity, const MetadataEntry& entry);
    void Remove(EntityId entity);
    std::optional<MetadataEntry> Get(EntityId entity) const;
    const std::unordered_map<EntityId, MetadataEntry, EntityIdHash>& All() const;

    void Clear();

private:
    std::unordered_map<EntityId, MetadataEntry, EntityIdHash> m_data;
};

} // namespace world
} // namespace eyelib
