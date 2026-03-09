#pragma once

#include "IComponentStore.h"
#include "eyelib/math/Transform.h"
#include "eyelib/math/Types.h"
#include "eyelib/math/TransformUtils.h"
#include "eyelib/world/EntityId.h"
#include "eyelib/world/components/Transform.h"

#include "eyelib/world/WorldGraph/Hierarchy.h"

#include <cstddef>
#include <cstdint>
#include <queue>
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

    /** World matrix (derived). Valid after TransformSystem has run. */
    math::Mat4 WorldMatrix() const;
    const math::Mat4& WorldMatrixRef() const;

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

    /** World TRS (derived by TransformSystem). Same order as EntitiesSpan(). */
    std::span<const math::Vec3> WorldPositionsSpan() const { return m_worldPositions; }
    std::span<const math::Quat> WorldRotationsSpan() const { return m_worldRotations; }
    std::span<const math::Vec3> WorldScalesSpan() const { return m_worldScales; }

    /** World matrices (derived from world TRS by TransformSystem). Same order as EntitiesSpan(). */
    std::span<const math::Mat4> WorldMatricesSpan() const { return m_worldMatrices; }
    std::span<math::Mat4> WorldMatricesSpanMutable() { return m_worldMatrices; }

    /** Get world matrix for entity. Derived from cached world TRS. Identity if not found. Valid after TransformSystem run. */
    math::Mat4 GetWorldMatrix(EntityId entity) const;
    /** Get world TRS for entity from cache. Returns false if entity not in store. Valid after TransformSystem run. */
    bool GetWorldTRS(EntityId entity, math::Vec3& outPos, math::Quat& outRot, math::Vec3& outScale) const;
    /** Get store index for entity. Returns static_cast<size_t>(-1) if not found. */
    size_t GetIndex(EntityId entity) const;
    /** Set world TRS and derived matrix by index (used by TransformSystem). Index must be < Size(). */
    void SetWorldTRSByIndex(size_t index, const math::Vec3& worldPos, const math::Quat& worldRot, const math::Vec3& worldScale);
    /** Set world-computed version after computing world TRS (dirty propagation). */
    void SetWorldComputedVersionByIndex(size_t index, uint32_t version);

    /** Dirty propagation: call when local transform is set. */
    void MarkLocalDirty(EntityId entity);
    /** Dirty propagation: call after reparent; marks entity and all descendants in store. */
    void MarkSubtreeDirty(const Hierarchy& hierarchy, EntityId root);

    /** For TransformSystem: version at index (same order as EntitiesSpan). */
    uint32_t GetLocalDirtyVersion(size_t index) const;
    uint32_t GetWorldComputedVersion(size_t index) const;

    // Storage (public for TransformRef and Range; treat as internal)
    std::vector<EntityId> m_entityIds;
    std::vector<math::Vec3> m_positions;
    std::vector<math::Quat> m_rotations;
    std::vector<math::Vec3> m_scales;
    std::vector<math::Vec3> m_worldPositions;
    std::vector<math::Quat> m_worldRotations;
    std::vector<math::Vec3> m_worldScales;
    std::vector<math::Mat4> m_worldMatrices;
    std::vector<uint32_t> m_localDirtyVersion;
    std::vector<uint32_t> m_worldComputedVersion;
    uint32_t m_globalTransformVersion = 0;
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
inline math::Mat4 TransformRef::WorldMatrix() const { return m_store->m_worldMatrices[m_index]; }
inline const math::Mat4& TransformRef::WorldMatrixRef() const { return m_store->m_worldMatrices[m_index]; }
inline TransformRef::operator math::Transform() const { return { Position(), Rotation(), Scale() }; }
inline TransformRef& TransformRef::operator=(const math::Transform& t) {
    Position() = math::SanitizePosition(t.position);
    Rotation() = math::SanitizeRotation(t.rotation);
    Scale() = math::SanitizeScale(t.scale);
    return *this;
}

inline TransformStore::Range::Iterator::value_type TransformStore::Range::Iterator::operator*() const {
    return { m_store->m_entityIds[m_index], TransformRef(m_store, m_index) };
}

inline TransformRef TransformStore::Add(EntityId entity, const math::Transform& value) {
    const math::Vec3 safePos = math::SanitizePosition(value.position);
    const math::Quat safeRotation = math::SanitizeRotation(value.rotation);
    const math::Vec3 safeScale = math::SanitizeScale(value.scale);
    auto it = m_entityToIndex.find(entity);
    if (it != m_entityToIndex.end()) {
        const size_t i = it->second;
        m_positions[i] = safePos;
        m_rotations[i] = safeRotation;
        m_scales[i] = safeScale;
        m_globalTransformVersion++;
        m_localDirtyVersion[i] = m_globalTransformVersion;
        m_cachedRef = TransformRef(this, i);
        return TransformRef(this, i);
    }
    const size_t i = m_entityIds.size();
    m_entityIds.push_back(entity);
    m_positions.push_back(safePos);
    m_rotations.push_back(safeRotation);
    m_scales.push_back(safeScale);
    m_worldPositions.push_back(safePos);
    m_worldRotations.push_back(safeRotation);
    m_worldScales.push_back(safeScale);
    m_worldMatrices.push_back(math::Compose(safePos, safeRotation, safeScale));
    m_globalTransformVersion++;
    m_localDirtyVersion.push_back(m_globalTransformVersion);
    m_worldComputedVersion.push_back(0u);
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
        m_worldPositions[i] = std::move(m_worldPositions[last]);
        m_worldRotations[i] = std::move(m_worldRotations[last]);
        m_worldScales[i] = std::move(m_worldScales[last]);
        m_worldMatrices[i] = std::move(m_worldMatrices[last]);
        m_localDirtyVersion[i] = std::move(m_localDirtyVersion[last]);
        m_worldComputedVersion[i] = std::move(m_worldComputedVersion[last]);
        m_entityToIndex[moved] = i;
    }
    m_entityIds.pop_back();
    m_positions.pop_back();
    m_rotations.pop_back();
    m_scales.pop_back();
    m_worldPositions.pop_back();
    m_worldRotations.pop_back();
    m_worldScales.pop_back();
    m_worldMatrices.pop_back();
    m_localDirtyVersion.pop_back();
    m_worldComputedVersion.pop_back();
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
    m_worldPositions.clear();
    m_worldRotations.clear();
    m_worldScales.clear();
    m_worldMatrices.clear();
    m_localDirtyVersion.clear();
    m_worldComputedVersion.clear();
    m_entityToIndex.clear();
}

inline math::Mat4 TransformStore::GetWorldMatrix(EntityId entity) const {
    auto it = m_entityToIndex.find(entity);
    if (it == m_entityToIndex.end()) return math::Mat4(1.0f);
    return m_worldMatrices[it->second];
}

inline bool TransformStore::GetWorldTRS(EntityId entity, math::Vec3& outPos, math::Quat& outRot, math::Vec3& outScale) const {
    auto it = m_entityToIndex.find(entity);
    if (it == m_entityToIndex.end()) return false;
    const size_t i = it->second;
    outPos = m_worldPositions[i];
    outRot = m_worldRotations[i];
    outScale = m_worldScales[i];
    return true;
}

inline size_t TransformStore::GetIndex(EntityId entity) const {
    auto it = m_entityToIndex.find(entity);
    return it == m_entityToIndex.end() ? static_cast<size_t>(-1) : it->second;
}

inline void TransformStore::SetWorldTRSByIndex(size_t index, const math::Vec3& worldPos, const math::Quat& worldRot, const math::Vec3& worldScale) {
    if (index >= m_worldPositions.size()) return;
    const math::Vec3 p = math::SanitizePosition(worldPos);
    const math::Quat r = math::SanitizeRotation(worldRot);
    const math::Vec3 s = math::SanitizeScale(worldScale);
    m_worldPositions[index] = p;
    m_worldRotations[index] = r;
    m_worldScales[index] = s;
    m_worldMatrices[index] = math::Compose(p, r, s);
}

inline void TransformStore::SetWorldComputedVersionByIndex(size_t index, uint32_t version) {
    if (index < m_worldComputedVersion.size()) m_worldComputedVersion[index] = version;
}

inline uint32_t TransformStore::GetLocalDirtyVersion(size_t index) const {
    return index < m_localDirtyVersion.size() ? m_localDirtyVersion[index] : 0u;
}

inline uint32_t TransformStore::GetWorldComputedVersion(size_t index) const {
    return index < m_worldComputedVersion.size() ? m_worldComputedVersion[index] : 0u;
}

inline void TransformStore::MarkLocalDirty(EntityId entity) {
    auto it = m_entityToIndex.find(entity);
    if (it == m_entityToIndex.end()) return;
    m_globalTransformVersion++;
    m_localDirtyVersion[it->second] = m_globalTransformVersion;
}

inline void TransformStore::MarkSubtreeDirty(const Hierarchy& hierarchy, EntityId root) {
    m_globalTransformVersion++;
    std::queue<EntityId> queue;
    queue.push(root);
    while (!queue.empty()) {
        EntityId entity = queue.front();
        queue.pop();
        auto it = m_entityToIndex.find(entity);
        if (it != m_entityToIndex.end())
            m_localDirtyVersion[it->second] = m_globalTransformVersion;
        for (EntityId child : hierarchy.GetChildren(entity))
            queue.push(child);
    }
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
