#include "Scene.h"

#include "eyelib/scene/Scene.h"

namespace eyelib {
namespace api {

class Scene::Impl {
public:
    eyelib::scene::Scene scene{};
};

Scene::Scene() : m_impl(new Impl()) {}

Scene::~Scene() {
    delete m_impl;
    m_impl = nullptr;
}

Entity Scene::CreateEntity(const std::string& name) {
    return m_impl->scene.CreateEntity(name);
}

void Scene::DestroyEntity(Entity e) {
    m_impl->scene.DestroyEntity(e);
}

bool Scene::IsAlive(Entity e) const {
    return m_impl->scene.IsAlive(e);
}

void Scene::Clear() {
    m_impl->scene.Clear();
}

void Scene::SetName(Entity e, const std::string& name) {
    m_impl->scene.SetName(e, name);
}

std::optional<std::string> Scene::GetName(Entity e) const {
    return m_impl->scene.GetName(e);
}

void Scene::SetTransform(Entity e, const eyelib::math::Transform& t) {
    m_impl->scene.SetTransform(e, t);
}

std::optional<eyelib::math::Transform> Scene::GetTransform(Entity e) const {
    return m_impl->scene.GetTransform(e);
}

bool Scene::SetParent(Entity child, Entity parent) {
    return m_impl->scene.SetParent(child, parent);
}

Entity Scene::GetParent(Entity child) const {
    return m_impl->scene.GetParent(child);
}

std::vector<Entity> Scene::GetChildren(Entity parent) const {
    return m_impl->scene.GetChildren(parent);
}

std::vector<Entity> Scene::GetEntities() const {
    return m_impl->scene.GetEntities();
}

} // namespace api
} // namespace eyelib

