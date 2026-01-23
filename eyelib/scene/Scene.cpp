#include "Scene.h"

#include "eyelib/core/Log.h"

#include <algorithm>
#include <unordered_map>
#include <vector>

namespace eyelib {
namespace scene {

class Scene::Impl {
public:
    std::unordered_map<Entity, Entity> parent{};
    std::unordered_map<Entity, std::vector<Entity>> children{};
    std::vector<Entity> entities{};
};

Scene::Scene() : m_impl(new Impl()) {}

Scene::~Scene() {
    delete m_impl;
    m_impl = nullptr;
}

Entity Scene::CreateEntity(const std::string& name) {
    Entity e = m_world.CreateEntity();
    SetName(e, name);

    m_impl->parent[e] = NullEntity;
    m_impl->children.emplace(e, std::vector<Entity>{});
    m_impl->entities.push_back(e);
    return e;
}

void Scene::DestroyEntity(Entity e) {
    if (!IsAlive(e)) return;

    // Destroy descendants first (copy list to avoid mutation issues)
    const auto kids = GetChildren(e);
    for (Entity c : kids) {
        DestroyEntity(c);
    }

    // Detach from parent
    const Entity p = GetParent(e);
    if (p != NullEntity) {
        auto it = m_impl->children.find(p);
        if (it != m_impl->children.end()) {
            auto& vec = it->second;
            vec.erase(std::remove(vec.begin(), vec.end(), e), vec.end());
        }
    }

    m_impl->parent.erase(e);
    m_impl->children.erase(e);
    m_impl->entities.erase(std::remove(m_impl->entities.begin(), m_impl->entities.end(), e), m_impl->entities.end());
    m_world.DestroyEntity(e);
}

bool Scene::IsAlive(Entity e) const {
    return m_world.IsAlive(e);
}

void Scene::Clear() {
    // Copy because DestroyEntity mutates internal structures.
    const auto ents = GetEntities();
    for (Entity e : ents) {
        if (IsAlive(e)) DestroyEntity(e);
    }
}

void Scene::SetName(Entity e, const std::string& name) {
    m_world.SetName(e, name);
}

std::optional<std::string> Scene::GetName(Entity e) const {
    return m_world.GetName(e);
}

void Scene::SetTransform(Entity e, const math::Transform& t) {
    m_world.SetTransform(e, t);
}

std::optional<math::Transform> Scene::GetTransform(Entity e) const {
    return m_world.GetTransform(e);
}

bool Scene::SetParent(Entity child, Entity parent) {
    if (child == NullEntity) return false;
    if (parent == child) return false;
    if (!IsAlive(child)) return false;
    if (parent != NullEntity && !IsAlive(parent)) return false;

    // Prevent cycles: walk up from parent to root and ensure we don't hit child.
    Entity p = parent;
    while (p != NullEntity) {
        if (p == child) return false;
        p = GetParent(p);
    }

    // Remove from old parent
    const Entity oldParent = GetParent(child);
    if (oldParent != NullEntity) {
        auto it = m_impl->children.find(oldParent);
        if (it != m_impl->children.end()) {
            auto& vec = it->second;
            vec.erase(std::remove(vec.begin(), vec.end(), child), vec.end());
        }
    }

    // Set new parent
    m_impl->parent[child] = parent;
    if (parent != NullEntity) {
        m_impl->children[parent].push_back(child);
    }
    return true;
}

Entity Scene::GetParent(Entity child) const {
    auto it = m_impl->parent.find(child);
    return (it != m_impl->parent.end()) ? it->second : NullEntity;
}

std::vector<Entity> Scene::GetChildren(Entity parent) const {
    auto it = m_impl->children.find(parent);
    if (it == m_impl->children.end()) return {};
    return it->second;
}

std::vector<Entity> Scene::GetEntities() const {
    std::vector<Entity> out;
    out.reserve(m_impl->entities.size());
    for (Entity e : m_impl->entities) {
        if (IsAlive(e)) out.push_back(e);
    }
    return out;
}

} // namespace scene
} // namespace eyelib

