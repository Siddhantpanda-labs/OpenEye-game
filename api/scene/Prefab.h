#pragma once

#include "Scene.h"

#include "api/ecs/Component.h"
#include "eyelib/ecs/ComponentRegistry.h"
#include "eyelib/scene/Prefab.h"
#include "eyelib/scene/PrefabInstantiator.h"
#include "eyelib/scene/PrefabVariant.h"

#include "entt/entt.hpp"

#include "eyelib/math/Types.h"

#include <cstdint>
#include <optional>
#include <string>

namespace eyelib {
namespace api {

using PrefabHandle = uint32_t;
using PrefabVariantHandle = uint32_t;

class Prefab {
public:
    static PrefabHandle CreatePrefab(const std::string& name);
    static PrefabHandle CreateFromEntity(Scene& scene, Entity entity, const std::string& name);
    static PrefabHandle CreateFromHierarchy(Scene& scene, Entity root, const std::string& name);
    static bool IsValid(PrefabHandle handle);
    static void DestroyPrefab(PrefabHandle handle);

    static void SetName(PrefabHandle handle, const std::string& name);
    static std::string GetName(PrefabHandle handle);

    template<typename T>
    static void AddComponent(PrefabHandle handle, const T& component) {
        using Internal = typename detail::ComponentConverter<T>::Internal;
        const auto internal = detail::ComponentConverter<T>::ToInternal(component);
        const auto type = entt::type_hash<Internal>::value();
        eyelib::scene::Prefab::AddComponent(handle, type, &internal);
    }

    template<typename T>
    static std::optional<T> GetComponent(PrefabHandle handle) {
        using Internal = typename detail::ComponentConverter<T>::Internal;
        auto type = entt::type_hash<Internal>::value();
        void* ptr = eyelib::scene::Prefab::GetComponent(handle, type);
        if (!ptr) return std::nullopt;
        return detail::ComponentConverter<T>::FromInternal(*static_cast<Internal*>(ptr));
    }

    static bool SavePrefab(PrefabHandle handle, const std::string& path);
    static PrefabHandle LoadPrefab(const std::string& path);

    static Entity Instantiate(Scene& scene, PrefabHandle prefab, const math::Vec3& position, const math::Quat& rotation);
    static Entity Instantiate(Scene& scene, PrefabHandle prefab, Entity parent);
    static void DestroyInstance(Scene& scene, Entity entity);
};

class PrefabVariant {
public:
    static PrefabVariantHandle CreateVariant(PrefabHandle basePrefab);
    static PrefabHandle GetBasePrefab(PrefabVariantHandle handle);

    template<typename T>
    static void OverrideComponent(PrefabVariantHandle handle, const T& component) {
        using Internal = typename detail::ComponentConverter<T>::Internal;
        const auto internal = detail::ComponentConverter<T>::ToInternal(component);
        const auto type = entt::type_hash<Internal>::value();
        eyelib::scene::PrefabVariant::OverrideComponent(handle, type, &internal);
    }
};

} // namespace api
} // namespace eyelib
