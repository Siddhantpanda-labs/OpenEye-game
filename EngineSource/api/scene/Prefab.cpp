#include "Prefab.h"

namespace eyelib {
namespace api {

PrefabHandle Prefab::CreatePrefab(const std::string& name) {
    return eyelib::scene::Prefab::CreatePrefab(name);
}

PrefabHandle Prefab::CreateFromEntity(Scene& scene, Scene::Entity entity, const std::string& name) {
    return eyelib::scene::Prefab::CreateFromEntity(*scene.GetInternal(), entity, name);
}

PrefabHandle Prefab::CreateFromHierarchy(Scene& scene, Scene::Entity root, const std::string& name) {
    return eyelib::scene::Prefab::CreateFromHierarchy(*scene.GetInternal(), root, name);
}

bool Prefab::IsValid(PrefabHandle handle) {
    return eyelib::scene::Prefab::IsValid(handle);
}

void Prefab::DestroyPrefab(PrefabHandle handle) {
    eyelib::scene::Prefab::DestroyPrefab(handle);
}

void Prefab::SetName(PrefabHandle handle, const std::string& name) {
    eyelib::scene::Prefab::SetName(handle, name);
}

std::string Prefab::GetName(PrefabHandle handle) {
    return eyelib::scene::Prefab::GetName(handle);
}

bool Prefab::SavePrefab(PrefabHandle handle, const std::string& path) {
    return eyelib::scene::Prefab::SavePrefab(handle, path);
}

PrefabHandle Prefab::LoadPrefab(const std::string& path) {
    return eyelib::scene::Prefab::LoadPrefab(path);
}

Scene::Entity Prefab::Instantiate(Scene& scene, PrefabHandle prefab, const math::Vec3& position, const math::Quat& rotation) {
    return eyelib::scene::PrefabInstantiator::Instantiate(*scene.GetInternal(), prefab, position, rotation);
}

Scene::Entity Prefab::Instantiate(Scene& scene, PrefabHandle prefab, Scene::Entity parent) {
    return eyelib::scene::PrefabInstantiator::Instantiate(*scene.GetInternal(), prefab, parent);
}

void Prefab::DestroyInstance(Scene& scene, Scene::Entity entity) {
    eyelib::scene::PrefabInstantiator::DestroyInstance(*scene.GetInternal(), entity);
}

PrefabVariantHandle PrefabVariant::CreateVariant(PrefabHandle basePrefab) {
    return eyelib::scene::PrefabVariant::CreateVariant(basePrefab);
}

PrefabHandle PrefabVariant::GetBasePrefab(PrefabVariantHandle handle) {
    return eyelib::scene::PrefabVariant::GetBasePrefab(handle);
}

} // namespace api
} // namespace eyelib
