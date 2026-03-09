#include "TransformSystem.h"

#include "eyelib/world/World.h"
#include "eyelib/math/TransformUtils.h"
#include "eyelib/world/ComponentStorage/TransformStore.h"
#include "eyelib/world/WorldGraph/Hierarchy.h"

#include <queue>
#include <unordered_set>

namespace eyelib {
namespace world {
namespace systems {

void TransformSystem::Update(World& world, float /*dt*/) {
    Update(world.GetState());
}

void TransformSystem::RebuildLinearizedOrder(WorldState& state) const {
    const auto& store = state.GetTransformStore();
    const auto& hierarchy = state.GraphHierarchy();
    std::unordered_set<EntityId, EntityIdHash> transformEntities;
    for (const auto& [entity, ref] : store.All())
        transformEntities.insert(entity);
    m_linearizedOrder.clear();
    m_linearizedOrder.reserve(store.Size());
    std::queue<EntityId> queue;
    for (const auto& [entity, ref] : store.All()) {
        const EntityId parent = hierarchy.GetParent(entity);
        if (parent == InvalidEntity || transformEntities.count(parent) == 0)
            queue.push(entity);
    }
    while (!queue.empty()) {
        const EntityId entity = queue.front();
        queue.pop();
        m_linearizedOrder.push_back(entity);
        for (EntityId child : hierarchy.GetChildren(entity))
            if (transformEntities.count(child)) queue.push(child);
    }
}

void TransformSystem::Update(WorldState& state) const {
    auto& store = state.GetTransformStore();
    const auto& hierarchy = state.GraphHierarchy();
    if (store.Size() == 0) return;

    const uint32_t hierarchyVersion = hierarchy.GetVersion();
    const size_t transformCount = store.Size();
    if (m_linearizedOrder.size() != transformCount || hierarchyVersion != m_lastHierarchyVersion || transformCount != m_lastTransformCount) {
        RebuildLinearizedOrder(state);
        m_lastHierarchyVersion = hierarchyVersion;
        m_lastTransformCount = transformCount;
    }

    std::unordered_set<EntityId, EntityIdHash> transformEntities;
    for (const auto& [entity, ref] : store.All())
        transformEntities.insert(entity);

    m_updatedThisPass.assign(transformCount, 0);

    for (EntityId entity : m_linearizedOrder) {
        const size_t index = store.GetIndex(entity);
        if (index == static_cast<size_t>(-1)) continue;
        const uint32_t localVer = store.GetLocalDirtyVersion(index);
        const uint32_t worldVer = store.GetWorldComputedVersion(index);
        const EntityId parent = hierarchy.GetParent(entity);
        const size_t parentIndex = parent != InvalidEntity && transformEntities.count(parent) ? store.GetIndex(parent) : static_cast<size_t>(-1);
        const bool parentUpdated = parentIndex != static_cast<size_t>(-1) && parentIndex < m_updatedThisPass.size() && m_updatedThisPass[parentIndex];
        const bool needUpdate = (localVer != worldVer) || parentUpdated;

        if (!needUpdate) continue;

        const auto* ref = store.TryGet(entity);
        if (!ref) continue;
        const math::Vec3 localPos = ref->Position();
        const math::Quat localRot = ref->Rotation();
        const math::Vec3 localScale = ref->Scale();
        math::Vec3 worldPos, worldScale;
        math::Quat worldRot;
        if (parent == InvalidEntity || transformEntities.count(parent) == 0) {
            worldPos = localPos;
            worldRot = localRot;
            worldScale = localScale;
        } else {
            const auto wPos = store.WorldPositionsSpan();
            const auto wRot = store.WorldRotationsSpan();
            const auto wScale = store.WorldScalesSpan();
            math::ComposeTRS(wPos[parentIndex], wRot[parentIndex], wScale[parentIndex],
                            localPos, localRot, localScale, worldPos, worldRot, worldScale);
        }
        store.SetWorldTRSByIndex(index, worldPos, worldRot, worldScale);
        store.SetWorldComputedVersionByIndex(index, localVer);
        m_updatedThisPass[index] = 1;
    }
}

} // namespace systems
} // namespace world
} // namespace eyelib
