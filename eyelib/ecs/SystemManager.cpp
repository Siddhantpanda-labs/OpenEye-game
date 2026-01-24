#include "SystemManager.h"

#include <algorithm>
#include <queue>
#include <utility>

namespace eyelib {
namespace ecs {

void SystemManager::RegisterSystem(std::shared_ptr<ISystem> system) {
    if (!system) return;
    const std::string name = system->GetName();
    if (name.empty()) return;
    m_byName[name] = system;
    m_systems.push_back(std::move(system));
    m_dirty = true;
}

std::shared_ptr<ISystem> SystemManager::GetSystem(const std::string& name) const {
    auto it = m_byName.find(name);
    return it != m_byName.end() ? it->second : nullptr;
}

void SystemManager::ResolveOrder() {
    m_ordered.clear();
    if (m_systems.empty()) return;

    const size_t count = m_systems.size();
    std::vector<std::vector<size_t>> dependents(count);
    std::vector<int> indegree(count, 0);

    std::unordered_map<std::string, size_t> indexByName;
    indexByName.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        indexByName[m_systems[i]->GetName()] = i;
    }

    for (size_t i = 0; i < count; ++i) {
        for (const auto& dep : m_systems[i]->GetDependencies()) {
            auto it = indexByName.find(dep);
            if (it == indexByName.end()) continue;
            dependents[it->second].push_back(i);
            indegree[i] += 1;
        }
    }

    std::vector<size_t> ready;
    ready.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        if (indegree[i] == 0) ready.push_back(i);
    }

    auto cmpOrder = [&](size_t a, size_t b) {
        return m_systems[a]->GetUpdateOrder() < m_systems[b]->GetUpdateOrder();
    };

    while (!ready.empty()) {
        std::sort(ready.begin(), ready.end(), cmpOrder);
        const size_t index = ready.front();
        ready.erase(ready.begin());

        m_ordered.push_back(m_systems[index]);
        for (size_t dep : dependents[index]) {
            indegree[dep] -= 1;
            if (indegree[dep] == 0) {
                ready.push_back(dep);
            }
        }
    }

    if (m_ordered.size() != count) {
        m_ordered = m_systems;
        std::sort(m_ordered.begin(), m_ordered.end(), [](const auto& a, const auto& b) {
            return a->GetUpdateOrder() < b->GetUpdateOrder();
        });
    }

    m_dirty = false;
}

void SystemManager::UpdateAll(World& world, float deltaTime) {
    if (m_dirty) ResolveOrder();
    for (auto& system : m_ordered) {
        system->Update(world, deltaTime);
    }
}

} // namespace ecs
} // namespace eyelib
