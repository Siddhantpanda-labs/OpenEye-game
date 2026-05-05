#pragma once

#include "Scene.h"

#include "eyelib/scene/Prefab.h"
#include "eyelib/scene/PrefabInstantiator.h"
#include "eyelib/scene/PrefabVariant.h"

#include "eyelib/math/Types.h"

#include <cstdint>
#include <string>

namespace eyelib {
namespace api {

using PrefabHandle = uint32_t;
using PrefabVariantHandle = uint32_t;

class Prefab {
public:
    static PrefabHandle CreatePrefab(const std::string& name);
    static PrefabHandle CreateFromEntity(Scene& scene, Scene::Entity entity, const std::string& name);
    static PrefabHandle CreateFromHierarchy(Scene& scene, Scene::Entity root, const std::string& name);
    static bool IsValid(PrefabHandle handle);
    static void DestroyPrefab(PrefabHandle handle);

    static void SetName(PrefabHandle handle, const std::string& name);
    static std::string GetName(PrefabHandle handle);

    static bool SavePrefab(PrefabHandle handle, const std::string& path);
    static PrefabHandle LoadPrefab(const std::string& path);

    static Scene::Entity Instantiate(Scene& scene, PrefabHandle prefab, const math::Vec3& position, const math::Quat& rotation);
    static Scene::Entity Instantiate(Scene& scene, PrefabHandle prefab, Scene::Entity parent);
    static void DestroyInstance(Scene& scene, Scene::Entity entity);
};

class PrefabVariant {
public:
    static PrefabVariantHandle CreateVariant(PrefabHandle basePrefab);
    static PrefabHandle GetBasePrefab(PrefabVariantHandle handle);
};

} // namespace api
} // namespace eyelib
