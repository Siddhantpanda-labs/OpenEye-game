#include "eyelib/assets/AssetCache.h"

namespace eyelib {
namespace assets {

bool AssetCache::Has(const AssetId& id) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_entries.find(id) != m_entries.end();
}

void AssetCache::Remove(const AssetId& id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_entries.erase(id);
}

void AssetCache::Clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_entries.clear();
}

std::string AssetCache::GetType(const AssetId& id) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_entries.find(id);
    return it != m_entries.end() ? it->second.type : std::string{};
}

} // namespace assets
} // namespace eyelib
