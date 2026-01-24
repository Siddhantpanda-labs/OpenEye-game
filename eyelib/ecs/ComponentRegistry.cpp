#include "ComponentRegistry.h"

namespace eyelib {
namespace ecs {

size_t ComponentRegistry::GetComponentSize(ComponentTypeId id) const {
    auto it = m_info.find(id);
    return it != m_info.end() ? it->second.size : 0;
}

size_t ComponentRegistry::GetComponentAlignment(ComponentTypeId id) const {
    auto it = m_info.find(id);
    return it != m_info.end() ? it->second.alignment : 0;
}

const std::string& ComponentRegistry::GetComponentName(ComponentTypeId id) const {
    static const std::string kEmpty{};
    auto it = m_info.find(id);
    return it != m_info.end() ? it->second.name : kEmpty;
}

ComponentTypeId ComponentRegistry::FindComponentType(const std::string& name) const {
    auto it = m_nameToType.find(name);
    return it != m_nameToType.end() ? it->second : ComponentTypeId{};
}

const ComponentOps* ComponentRegistry::GetOps(ComponentTypeId id) const {
    auto it = m_info.find(id);
    return it != m_info.end() ? &it->second.ops : nullptr;
}

} // namespace ecs
} // namespace eyelib
