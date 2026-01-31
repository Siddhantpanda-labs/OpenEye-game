#include "Metadata.h"

namespace eyelib {
namespace world {

void Metadata::Set(EntityId entity, const MetadataEntry& entry) {
    m_data[entity] = entry;
}

void Metadata::Remove(EntityId entity) {
    m_data.erase(entity);
}

std::optional<MetadataEntry> Metadata::Get(EntityId entity) const {
    auto it = m_data.find(entity);
    if (it == m_data.end()) {
        return std::nullopt;
    }
    return it->second;
}

const std::unordered_map<EntityId, MetadataEntry, EntityIdHash>& Metadata::All() const {
    return m_data;
}

void Metadata::Clear() {
    m_data.clear();
}

} // namespace world
} // namespace eyelib
