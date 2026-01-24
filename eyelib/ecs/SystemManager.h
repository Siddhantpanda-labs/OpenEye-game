#pragma once

#include "System.h"

#include <memory>
#include <unordered_map>
#include <vector>

namespace eyelib {
namespace ecs {

class SystemManager {
public:
    void RegisterSystem(std::shared_ptr<ISystem> system);
    void UpdateAll(World& world, float deltaTime);

    std::shared_ptr<ISystem> GetSystem(const std::string& name) const;

private:
    void ResolveOrder();

    bool m_dirty = true;
    std::vector<std::shared_ptr<ISystem>> m_systems;
    std::vector<std::shared_ptr<ISystem>> m_ordered;
    std::unordered_map<std::string, std::shared_ptr<ISystem>> m_byName;
};

} // namespace ecs
} // namespace eyelib
