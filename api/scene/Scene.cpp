#include "Scene.h"

#include "eyelib/scene/Scene.h"

namespace eyelib {
namespace api {

class Scene::Impl {
public:
    eyelib::scene::Scene* scene = nullptr;
    bool owns = true;
};

Scene::Scene() : m_impl(new Impl()) {
    m_impl->scene = new eyelib::scene::Scene();
}

Scene::~Scene() {
    if (m_impl->owns) {
        delete m_impl->scene;
    }
    delete m_impl;
    m_impl = nullptr;
}

Entity Scene::CreateEntity(const std::string& name) {
    return m_impl->scene->CreateEntity(name);
}

void Scene::DestroyEntity(Entity e) {
    m_impl->scene->DestroyEntity(e);
}

bool Scene::IsAlive(Entity e) const {
    return m_impl->scene->IsAlive(e);
}

void Scene::Clear() {
    m_impl->scene->Clear();
}

void Scene::SetName(Entity e, const std::string& name) {
    m_impl->scene->SetName(e, name);
}

std::optional<std::string> Scene::GetName(Entity e) const {
    return m_impl->scene->GetName(e);
}

void Scene::SetTransform(Entity e, const eyelib::math::Transform& t) {
    m_impl->scene->SetTransform(e, t);
}

std::optional<eyelib::math::Transform> Scene::GetTransform(Entity e) const {
    return m_impl->scene->GetTransform(e);
}

bool Scene::SetParent(Entity child, Entity parent) {
    return m_impl->scene->SetParent(child, parent);
}

Entity Scene::GetParent(Entity child) const {
    return m_impl->scene->GetParent(child);
}

std::vector<Entity> Scene::GetChildren(Entity parent) const {
    return m_impl->scene->GetChildren(parent);
}

std::vector<Entity> Scene::GetEntities() const {
    return m_impl->scene->GetEntities();
}

std::vector<Entity> Scene::GetRootEntities() const {
    return m_impl->scene->GetRootEntities();
}

std::string Scene::GetEntityPath(Entity entity) const {
    return m_impl->scene->GetEntityPath(entity);
}

Entity Scene::FindEntity(const std::string& path) const {
    return m_impl->scene->FindEntity(path);
}

size_t Scene::GetSiblingCount(Entity entity) const {
    return m_impl->scene->GetSiblingCount(entity);
}

size_t Scene::GetChildCount(Entity entity) const {
    return m_impl->scene->GetChildCount(entity);
}

bool Scene::MoveEntity(Entity entity, Entity newParent) {
    return m_impl->scene->MoveEntity(entity, newParent);
}

Entity Scene::DuplicateEntity(Entity entity) {
    return m_impl->scene->DuplicateEntity(entity);
}

eyelib::scene::Scene* Scene::GetInternal() const {
    return m_impl->scene;
}

std::shared_ptr<Scene> Scene::WrapInternal(eyelib::scene::Scene* scene) {
    if (!scene) return nullptr;
    auto apiScene = std::make_shared<Scene>();
    if (apiScene->m_impl->owns) {
        delete apiScene->m_impl->scene;
    }
    apiScene->m_impl->scene = scene;
    apiScene->m_impl->owns = false;
    return apiScene;
}

} // namespace api
} // namespace eyelib

