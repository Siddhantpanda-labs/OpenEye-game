#include "Scene.h"

#include "eyelib/core/Log.h"
#include "eyelib/world/EntityRegistry.h"

#include <algorithm>
#include <vector>

namespace eyelib {
namespace scene {

Scene::Scene() = default;

Scene::~Scene() = default;

Entity Scene::CreateEntity(const std::string& name) {
    return m_world.CreateEntity(name.empty() ? "Entity" : name);
}

void Scene::DestroyEntity(Entity e) {
    if (!IsAlive(e)) return;

    const auto kids = GetChildren(e);
    for (Entity c : kids) {
        DestroyEntity(c);
    }

    m_world.DestroyEntity(e);
}

bool Scene::IsAlive(Entity e) const {
    return m_world.IsAlive(e);
}

void Scene::Clear() {
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

    Entity p = parent;
    while (p != NullEntity) {
        if (p == child) return false;
        p = GetParent(p);
    }

    return m_world.SetParent(child, parent, false);
}

Entity Scene::GetParent(Entity child) const {
    return m_world.GetParent(child);
}

std::vector<Entity> Scene::GetChildren(Entity parent) const {
    return m_world.GetChildren(parent);
}

std::vector<Entity> Scene::GetEntities() const {
    return m_world.GetState().Entities().GetAliveEntities();
}

std::vector<Entity> Scene::GetRootEntities() const {
    std::vector<Entity> roots;
    for (Entity e : GetEntities()) {
        if (GetParent(e) == NullEntity) roots.push_back(e);
    }
    return roots;
}

std::string Scene::GetEntityPath(Entity entity) const {
    if (!IsAlive(entity)) return {};
    std::vector<std::string> parts;
    Entity current = entity;
    while (current != NullEntity) {
        std::string name = GetName(current).value_or("Entity");
        if (name.empty()) name = "Entity";
        parts.push_back(name);
        current = GetParent(current);
    }
    std::string path;
    for (auto it = parts.rbegin(); it != parts.rend(); ++it) {
        if (!path.empty()) path += "/";
        path += *it;
    }
    return path;
}

Entity Scene::FindEntity(const std::string& path) const {
    if (path.empty()) return NullEntity;
    std::vector<std::string> parts;
    size_t start = 0;
    while (start < path.size()) {
        size_t sep = path.find('/', start);
        if (sep == std::string::npos) sep = path.size();
        parts.push_back(path.substr(start, sep - start));
        start = sep + 1;
    }
    if (parts.empty()) return NullEntity;

    Entity current = NullEntity;
    for (Entity root : GetRootEntities()) {
        if (GetName(root).value_or("Entity") == parts[0]) {
            current = root;
            break;
        }
    }
    if (current == NullEntity) return NullEntity;

    for (size_t i = 1; i < parts.size(); ++i) {
        Entity next = NullEntity;
        for (Entity child : GetChildren(current)) {
            if (GetName(child).value_or("Entity") == parts[i]) {
                next = child;
                break;
            }
        }
        if (next == NullEntity) return NullEntity;
        current = next;
    }
    return current;
}

size_t Scene::GetSiblingCount(Entity entity) const {
    if (!IsAlive(entity)) return 0;
    Entity parent = GetParent(entity);
    if (parent == NullEntity) {
        const auto roots = GetRootEntities();
        return roots.size() > 0 ? roots.size() - 1 : 0;
    }
    const auto siblings = GetChildren(parent);
    return siblings.size() > 0 ? siblings.size() - 1 : 0;
}

size_t Scene::GetChildCount(Entity entity) const {
    return GetChildren(entity).size();
}

bool Scene::MoveEntity(Entity entity, Entity newParent) {
    return SetParent(entity, newParent);
}

namespace {
static Entity DuplicateRecursive(Scene& scene, Entity src, Entity parent) {
    const std::string name = scene.GetName(src).value_or("Entity") + " Copy";
    Entity dst = scene.CreateEntity(name);
    if (parent != NullEntity) scene.SetParent(dst, parent);

    if (auto t = scene.GetTransform(src)) {
        scene.SetTransform(dst, *t);
    }

    // TODO: clone other components via world::ComponentRegistry when needed
    for (Entity child : scene.GetChildren(src)) {
        DuplicateRecursive(scene, child, dst);
    }
    return dst;
}
} // namespace

Entity Scene::DuplicateEntity(Entity entity) {
    if (!IsAlive(entity)) return NullEntity;
    Entity parent = GetParent(entity);
    return DuplicateRecursive(*this, entity, parent);
}

} // namespace scene
} // namespace eyelib
