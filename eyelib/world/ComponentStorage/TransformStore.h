#pragma once

#include "IComponentStore.h"
#include "eyelib/math/Transform.h"
#include "eyelib/math/Types.h"
#include "eyelib/world/EntityId.h"
#include "eyelib/world/components/Transform.h"

#include <cstddef>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

namespace eyelib {
namespace world {

/** Tag type to request the SoA TransformStore from WorldState::Components<> / WorldView::Components<>. */
struct TransformTag {};

class TransformStore;

/** Proxy for a single transform in TransformStore. Refers to SoA slots by index. */
class TransformRef {
public:
    TransformRef() = default;
    TransformRef(TransformStore* store, size_t index) : m_store(store), m_index(index) {}

    math::Vec3& Position();
    const math::Vec3& Position() const;
    math::Quat& Rotation();
    const math::Quat& Rotation() const;
    math::Vec3& Scale();
    const math::Vec3& Scale() const;

    math::Vec3& position() { return Position(); }
    const math::Vec3& position() const { return Position(); }
    math::Quat& rotation() { return Rotation(); }
    const math::Quat& rotation() const { return Rotation(); }
    math::Vec3& scale() { return Scale(); }
    const math::Vec3& scale() const { return Scale(); }

    operator math::Transform() const;
    TransformRef& operator=(const math::Transform& t);

private:
    TransformStore* m_store = nullptr;
    size_t m_index = 0;
};

/** SoA component store for Transform: contiguous position[], rotation[], scale[] for cache-friendly iteration. */
class TransformStore final : public IComponentStore {
public:
    using Transform = components::Transform;

    TransformRef Add(EntityId entity, const math::Transform& value = math::Transform{});
    void Remove(EntityId entity) override;
    bool Has(EntityId entity) const override;
    void Clear() override;

    TransformRef* TryGet(EntityId entity);
    const TransformRef* TryGet(EntityId entity) const;

    /** Lazy range over (EntityId, TransformRef). No allocation. */
    class Range {
    public:
        class Iterator {
        public:
            using value_type = std::pair<EntityId, TransformRef>;
            value_type operator*() const;
            Iterator& operator++() { ++m_index; return *this; }
            bool operator!=(const Iterator& o) const { return m_index != o.m_index; }
        private:
            friend class Range;
            Iterator(TransformStore* s, size_t i) : m_store(s), m_index(i) {}
            TransformStore* m_store = nullptr;
            size_t m_index = 0;
        };
        explicit Range(TransformStore* store) : m_store(store) {}
        Iterator begin() const { return Iterator(m_store, 0); }
        Iterator end() const { return Iterator(m_store, m_store->Size()); }
    private:
        TransformStore* m_store = nullptr;
    };

    Range All() { return Range(this); }
    Range All() const { return Range(const_cast<TransformStore*>(this)); }

    size_t Size() const { return m_entityIds.size(); }

    /** Zero-copy spans over contiguous SoA data. Valid until next Add/Remove/Clear. */
    std::span<const EntityId> EntitiesSpan() const { return m_entityIds; }
    std::span<const math::Vec3> PositionsSpan() const { return m_positions; }
    std::span<const math::Quat> RotationsSpan() const { return m_rotations; }
    std::span<const math::Vec3> ScalesSpan() const { return m_scales; }
    std::span<math::Vec3> PositionsSpanMutable() { return m_positions; }
    std::span<math::Quat> RotationsSpanMutable() { return m_rotations; }
    std::span<math::Vec3> ScalesSpanMutable() { return m_scales; }

    // Storage (public for TransformRef and Range; treat as internal)
    std::vector<EntityId> m_entityIds;
    std::vector<math::Vec3> m_positions;
    std::vector<math::Quat> m_rotations;
    std::vector<math::Vec3> m_scales;
    std::unordered_map<EntityId, size_t, EntityIdHash> m_entityToIndex;

private:
    mutable TransformRef m_cachedRef;
};

// TransformRef method definitions (after TransformStore is complete)
inline math::Vec3& TransformRef::Position() { return m_store->m_positions[m_index]; }
inline const math::Vec3& TransformRef::Position() const { return m_store->m_positions[m_index]; }
inline math::Quat& TransformRef::Rotation() { return m_store->m_rotations[m_index]; }
inline const math::Quat& TransformRef::Rotation() const { return m_store->m_rotations[m_index]; }
inline math::Vec3& TransformRef::Scale() { return m_store->m_scales[m_index]; }
inline const math::Vec3& TransformRef::Scale() const { return m_store->m_scales[m_index]; }
inline TransformRef::operator math::Transform() const { return { Position(), Rotation(), Scale() }; }
inline TransformRef& TransformRef::operator=(const math::Transform& t) {
    Position() = t.position;
    Rotation() = t.rotation;
    Scale() = t.scale;
    return *this;
}

inline TransformStore::Range::Iterator::value_type TransformStore::Range::Iterator::operator*() const {
    return { m_store->m_entityIds[m_index], TransformRef(m_store, m_index) };
}

inline TransformRef TransformStore::Add(EntityId entity, const math::Transform& value) {
    auto it = m_entityToIndex.find(entity);
    if (it != m_entityToIndex.end()) {
        const size_t i = it->second;
        m_positions[i] = value.position;
        m_rotations[i] = value.rotation;
        m_scales[i] = value.scale;
        m_cachedRef = TransformRef(this, i);
        return TransformRef(this, i);
    }
    const size_t i = m_entityIds.size();
    m_entityIds.push_back(entity);
    m_positions.push_back(value.position);
    m_rotations.push_back(value.rotation);
    m_scales.push_back(value.scale);
    m_entityToIndex[entity] = i;
    return TransformRef(this, i);
}

inline void TransformStore::Remove(EntityId entity) {
    auto it = m_entityToIndex.find(entity);
    if (it == m_entityToIndex.end()) {
        return;
    }
    const size_t i = it->second;
    const size_t last = m_entityIds.size() - 1;
    if (i != last) {
        const EntityId moved = m_entityIds[last];
        m_entityIds[i] = moved;
        m_positions[i] = std::move(m_positions[last]);
        m_rotations[i] = std::move(m_rotations[last]);
        m_scales[i] = std::move(m_scales[last]);
        m_entityToIndex[moved] = i;
    }
    m_entityIds.pop_back();
    m_positions.pop_back();
    m_rotations.pop_back();
    m_scales.pop_back();
    m_entityToIndex.erase(it);
}

inline bool TransformStore::Has(EntityId entity) const {
    return m_entityToIndex.find(entity) != m_entityToIndex.end();
}

inline void TransformStore::Clear() {
    m_entityIds.clear();
    m_positions.clear();
    m_rotations.clear();
    m_scales.clear();
    m_entityToIndex.clear();
}

inline TransformRef* TransformStore::TryGet(EntityId entity) {
    auto it = m_entityToIndex.find(entity);
    if (it == m_entityToIndex.end()) {
        return nullptr;
    }
    m_cachedRef = TransformRef(this, it->second);
    return &m_cachedRef;
}

inline const TransformRef* TransformStore::TryGet(EntityId entity) const {
    auto it = m_entityToIndex.find(entity);
    if (it == m_entityToIndex.end()) {
        return nullptr;
    }
    m_cachedRef = TransformRef(const_cast<TransformStore*>(this), it->second);
    return &m_cachedRef;
}

} // namespace world
} // namespace eyelib
