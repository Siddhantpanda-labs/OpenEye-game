#pragma once

#include "asset/core/guid.h"

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace asset {

/**
 * Generic cache for loaded assets by Guid. Thread-safe Store/Get/Has/Remove/Clear.
 */
class AssetCache {
public:
    template<typename T>
    void Store(const Guid& id, const std::shared_ptr<T>& asset, const std::string& type) {
        std::lock_guard<std::mutex> lock(m_mutex);
        Entry e;
        e.data = asset;
        e.type = type;
        m_entries[id] = std::move(e);
    }

    template<typename T>
    std::shared_ptr<T> Get(const Guid& id) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_entries.find(id);
        if (it == m_entries.end()) return nullptr;
        return std::static_pointer_cast<T>(it->second.data);
    }

    bool Has(const Guid& id) const;
    void Remove(const Guid& id);
    void Clear();
    std::string GetType(const Guid& id) const;

private:
    struct Entry {
        std::shared_ptr<void> data;
        std::string type;
    };

    mutable std::mutex m_mutex;
    std::unordered_map<Guid, Entry, Guid::Hash> m_entries;
};

} // namespace asset
