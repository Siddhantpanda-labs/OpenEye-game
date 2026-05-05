#include "Tags.h"

namespace eyelib {
namespace world {

void Tags::Add(EntityId entity, const std::string& tag) {
    m_tags[entity].insert(tag);
}

void Tags::Remove(EntityId entity, const std::string& tag) {
    auto it = m_tags.find(entity);
    if (it == m_tags.end()) {
        return;
    }
    it->second.erase(tag);
    if (it->second.empty()) {
        m_tags.erase(it);
    }
}

bool Tags::Has(EntityId entity, const std::string& tag) const {
    auto it = m_tags.find(entity);
    if (it == m_tags.end()) {
        return false;
    }
    return it->second.find(tag) != it->second.end();
}

const std::unordered_map<EntityId, std::unordered_set<std::string>, EntityIdHash>& Tags::All() const {
    return m_tags;
}

void Tags::Clear() {
    m_tags.clear();
}

} // namespace world
} // namespace eyelib
