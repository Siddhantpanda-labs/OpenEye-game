#include "Scene.h"

#include "eyelib/core/Log.h"
#include "eyelib/ecs/ComponentStorage.h"
#include "eyelib/ecs/components/All.h"

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
static void CloneComponents(ecs::World& world, Entity src, Entity dst) {
    auto& registry = world.Components();
    auto& storage = world.Storage().Registry();
    const auto& types = registry.GetComponentTypes();

    const auto nameId = registry.GetComponentType<ecs::components::Name>();
    const auto transformId = registry.GetComponentType<ecs::components::Transform>();

    for (auto type : types) {
        if (type == nameId || type == transformId) continue;
        const auto* ops = registry.GetOps(type);
        if (!ops || !ops->get || !ops->add) continue;
        void* srcPtr = ops->get(storage, ecs::detail::ToEnTT(src));
        if (!srcPtr) continue;
        ops->add(storage, ecs::detail::ToEnTT(dst), srcPtr);
    }
}

static Entity DuplicateRecursive(Scene& scene, Entity src, Entity parent) {
    const std::string name = scene.GetName(src).value_or("Entity") + " Copy";
    Entity dst = scene.CreateEntity(name);
    if (parent != NullEntity) scene.SetParent(dst, parent);

    if (auto t = scene.GetTransform(src)) {
        scene.SetTransform(dst, *t);
    }

    CloneComponents(scene.World(), src, dst);

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

