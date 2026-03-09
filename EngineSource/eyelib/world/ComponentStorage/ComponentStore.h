#pragma once

#include "IComponentStore.h"
#include "eyelib/world/EntityId.h"

#include <unordered_map>

namespace eyelib {
namespace world {

template<typename T>
class ComponentStore final : public IComponentStore {
public:
    T& Add(EntityId entity, const T& value = T()) {
        return m_data[entity] = value;
    }

    void Remove(EntityId entity) override {
        m_data.erase(entity);
    }

    bool Has(EntityId entity) const override {
        return m_data.find(entity) != m_data.end();
    }

    T* TryGet(EntityId entity) {
        auto it = m_data.find(entity);
        if (it == m_data.end()) {
            return nullptr;
        }
        return &it->second;
    }

    const T* TryGet(EntityId entity) const {
        auto it = m_data.find(entity);
        if (it == m_data.end()) {
            return nullptr;
        }
        return &it->second;
    }

    const std::unordered_map<EntityId, T, EntityIdHash>& All() const {
        return m_data;
    }

    std::unordered_map<EntityId, T, EntityIdHash>& AllMutable() {
        return m_data;
    }

    void Clear() override {
        m_data.clear();
    }

private:
    std::unordered_map<EntityId, T, EntityIdHash> m_data;
};

} // namespace world
} // namespace eyelib
