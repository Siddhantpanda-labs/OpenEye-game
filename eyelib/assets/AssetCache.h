#pragma once

#include "eyelib/assets/AssetId.h"

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace eyelib {
namespace assets {

class AssetCache {
public:
    template<typename T>
    void Store(const AssetId& id, const std::shared_ptr<T>& asset, const std::string& type) {
        std::lock_guard<std::mutex> lock(m_mutex);
        Entry e;
        e.data = asset;
        e.type = type;
        m_entries[id] = std::move(e);
    }

    template<typename T>
    std::shared_ptr<T> Get(const AssetId& id) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_entries.find(id);
        if (it == m_entries.end()) return nullptr;
        return std::static_pointer_cast<T>(it->second.data);
    }

    bool Has(const AssetId& id) const;
    void Remove(const AssetId& id);
    void Clear();
    std::string GetType(const AssetId& id) const;

private:
    struct Entry {
        std::shared_ptr<void> data;
        std::string type;
    };

    mutable std::mutex m_mutex;
    std::unordered_map<AssetId, Entry, AssetId::Hash> m_entries;
};

} // namespace assets
} // namespace eyelib
