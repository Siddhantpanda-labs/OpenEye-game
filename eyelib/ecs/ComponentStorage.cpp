#include "ComponentStorage.h"

namespace eyelib {
namespace ecs {

ComponentStorage::ComponentStorage(ComponentRegistry& registry)
    : m_registryInfo(registry) {}

void* ComponentStorage::AddComponent(Entity e, ComponentTypeId id, const void* data) {
    const auto* ops = m_registryInfo.GetOps(id);
    if (!ops || !ops->add) return nullptr;
    ops->add(m_registry, detail::ToEnTT(e), data);
    return ops->get ? ops->get(m_registry, detail::ToEnTT(e)) : nullptr;
}

void ComponentStorage::RemoveComponent(Entity e, ComponentTypeId id) {
    const auto* ops = m_registryInfo.GetOps(id);
    if (!ops || !ops->remove) return;
    ops->remove(m_registry, detail::ToEnTT(e));
}

void* ComponentStorage::GetComponent(Entity e, ComponentTypeId id) {
    const auto* ops = m_registryInfo.GetOps(id);
    if (!ops || !ops->get) return nullptr;
    return ops->get(m_registry, detail::ToEnTT(e));
}

const void* ComponentStorage::GetComponent(Entity e, ComponentTypeId id) const {
    const auto* ops = m_registryInfo.GetOps(id);
    if (!ops || !ops->getConst) return nullptr;
    return ops->getConst(m_registry, detail::ToEnTT(e));
}

bool ComponentStorage::HasComponent(Entity e, ComponentTypeId id) const {
    const auto* ops = m_registryInfo.GetOps(id);
    if (!ops || !ops->has) return false;
    return ops->has(m_registry, detail::ToEnTT(e));
}

size_t ComponentStorage::GetComponentCount(ComponentTypeId id) const {
    const auto* ops = m_registryInfo.GetOps(id);
    if (!ops || !ops->count) return 0;
    return ops->count(m_registry);
}

} // namespace ecs
} // namespace eyelib
