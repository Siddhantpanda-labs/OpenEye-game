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

Scene::Entity Scene::CreateEntity(const std::string& name) {
    return m_impl->scene->CreateEntity(name);
}

void Scene::DestroyEntity(Scene::Entity e) {
    m_impl->scene->DestroyEntity(e);
}

bool Scene::IsAlive(Scene::Entity e) const {
    return m_impl->scene->IsAlive(e);
}

void Scene::Clear() {
    m_impl->scene->Clear();
}

void Scene::SetName(Scene::Entity e, const std::string& name) {
    m_impl->scene->SetName(e, name);
}

std::optional<std::string> Scene::GetName(Scene::Entity e) const {
    return m_impl->scene->GetName(e);
}

void Scene::SetTransform(Scene::Entity e, const eyelib::math::Transform& t) {
    m_impl->scene->SetTransform(e, t);
}

std::optional<eyelib::math::Transform> Scene::GetTransform(Scene::Entity e) const {
    return m_impl->scene->GetTransform(e);
}

bool Scene::SetParent(Scene::Entity child, Scene::Entity parent) {
    return m_impl->scene->SetParent(child, parent);
}

Scene::Entity Scene::GetParent(Scene::Entity child) const {
    return m_impl->scene->GetParent(child);
}

std::vector<Scene::Entity> Scene::GetChildren(Scene::Entity parent) const {
    return m_impl->scene->GetChildren(parent);
}

std::vector<Scene::Entity> Scene::GetEntities() const {
    return m_impl->scene->GetEntities();
}

std::vector<Scene::Entity> Scene::GetRootEntities() const {
    return m_impl->scene->GetRootEntities();
}

std::string Scene::GetEntityPath(Scene::Entity entity) const {
    return m_impl->scene->GetEntityPath(entity);
}

Scene::Entity Scene::FindEntity(const std::string& path) const {
    return m_impl->scene->FindEntity(path);
}

size_t Scene::GetSiblingCount(Scene::Entity entity) const {
    return m_impl->scene->GetSiblingCount(entity);
}

size_t Scene::GetChildCount(Scene::Entity entity) const {
    return m_impl->scene->GetChildCount(entity);
}

bool Scene::MoveEntity(Scene::Entity entity, Scene::Entity newParent) {
    return m_impl->scene->MoveEntity(entity, newParent);
}

Scene::Entity Scene::DuplicateEntity(Scene::Entity entity) {
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

