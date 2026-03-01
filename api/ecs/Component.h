#pragma once

#include "Components.h"
#include "World.h"
#include "eyelib/ecs/World.h"

#include "eyelib/ecs/components/All.h"

#include <optional>

namespace eyelib {
namespace api {

namespace detail {

template<typename T>
struct ComponentConverter;

template<>
struct ComponentConverter<Transform> {
    using Internal = eyelib::ecs::components::Transform;
    static Internal ToInternal(const Transform& t) {
        Internal out;
        out.position = t.position;
        out.rotation = t.rotation;
        out.scale = t.scale;
        out.parent = t.parent;
        out.dirty = t.dirty;
        return out;
    }
    static Transform FromInternal(const Internal& t) {
        Transform out;
        out.position = t.position;
        out.rotation = t.rotation;
        out.scale = t.scale;
        out.parent = t.parent;
        out.dirty = t.dirty;
        return out;
    }
};

template<>
struct ComponentConverter<Name> {
    using Internal = eyelib::ecs::components::Name;
    static Internal ToInternal(const Name& n) { return Internal{ n.name }; }
    static Name FromInternal(const Internal& n) { return Name{ n.name }; }
};

template<>
struct ComponentConverter<Renderable> {
    using Internal = eyelib::ecs::components::Renderable;
    static Internal ToInternal(const Renderable& r) {
        Internal out;
        out.visible = r.visible;
        out.activeLOD = r.activeLOD;
        out.mesh = r.mesh ? r.mesh->GetInternal() : nullptr;
        out.material = r.material ? r.material->GetInternal() : nullptr;
        out.lodMeshes.reserve(r.lodMeshes.size());
        for (const auto& mesh : r.lodMeshes) {
            out.lodMeshes.push_back(mesh ? mesh->GetInternal() : nullptr);
        }
        return out;
    }
    static Renderable FromInternal(const Internal& r) {
        Renderable out;
        out.visible = r.visible;
        out.activeLOD = r.activeLOD;
        out.mesh = r.mesh ? Mesh::WrapInternal(r.mesh) : nullptr;
        out.material = r.material ? Material::WrapInternal(r.material) : nullptr;
        out.lodMeshes.reserve(r.lodMeshes.size());
        for (const auto& mesh : r.lodMeshes) {
            out.lodMeshes.push_back(mesh ? Mesh::WrapInternal(mesh) : nullptr);
        }
        return out;
    }
};

template<>
struct ComponentConverter<Camera> {
    using Internal = eyelib::ecs::components::Camera;
    static Internal ToInternal(const Camera& c) {
        Internal out;
        out.isPrimary = c.isPrimary;
        out.isPerspective = c.isPerspective;
        out.fov = c.fov;
        out.aspect = c.aspect;
        out.nearPlane = c.nearPlane;
        out.farPlane = c.farPlane;
        out.orthoSize = c.orthoSize;
        return out;
    }
    static Camera FromInternal(const Internal& c) {
        Camera out;
        out.isPrimary = c.isPrimary;
        out.isPerspective = c.isPerspective;
        out.fov = c.fov;
        out.aspect = c.aspect;
        out.nearPlane = c.nearPlane;
        out.farPlane = c.farPlane;
        out.orthoSize = c.orthoSize;
        return out;
    }
};

template<>
struct ComponentConverter<Light> {
    using Internal = eyelib::ecs::components::Light;
    static Internal ToInternal(const Light& l) {
        Internal out;
        out.type = static_cast<Internal::Type>(l.type);
        out.color = l.color;
        out.intensity = l.intensity;
        out.range = l.range;
        out.innerConeAngle = l.innerConeAngle;
        out.outerConeAngle = l.outerConeAngle;
        out.castShadows = l.castShadows;
        return out;
    }
    static Light FromInternal(const Internal& l) {
        Light out;
        out.type = static_cast<Light::Type>(l.type);
        out.color = l.color;
        out.intensity = l.intensity;
        out.range = l.range;
        out.innerConeAngle = l.innerConeAngle;
        out.outerConeAngle = l.outerConeAngle;
        out.castShadows = l.castShadows;
        return out;
    }
};

template<>
struct ComponentConverter<RigidBody> {
    using Internal = eyelib::ecs::components::RigidBody;
    static Internal ToInternal(const RigidBody& r) {
        Internal out;
        out.syncTransform = r.syncTransform;
        return out;
    }
    static RigidBody FromInternal(const Internal& r) {
        RigidBody out;
        out.syncTransform = r.syncTransform;
        return out;
    }
};

template<>
struct ComponentConverter<AnimatorComponent> {
    using Internal = eyelib::ecs::components::Animator;
    static Internal ToInternal(const AnimatorComponent& a) {
        Internal out;
        out.animator = a.animator ? a.animator->GetInternal() : nullptr;
        out.skeleton = a.skeleton ? a.skeleton->GetInternal() : nullptr;
        return out;
    }
    static AnimatorComponent FromInternal(const Internal& a) {
        AnimatorComponent out;
        out.animator = a.animator ? api::Animator::WrapInternal(a.animator) : nullptr;
        out.skeleton = a.skeleton ? api::Skeleton::WrapInternal(a.skeleton) : nullptr;
        return out;
    }
};

template<>
struct ComponentConverter<AABB> {
    using Internal = eyelib::ecs::components::AABB;
    static Internal ToInternal(const AABB& a) {
        Internal out;
        out.min = a.min;
        out.max = a.max;
        out.isVisible = a.isVisible;
        return out;
    }
    static AABB FromInternal(const Internal& a) {
        AABB out;
        out.min = a.min;
        out.max = a.max;
        out.isVisible = a.isVisible;
        return out;
    }
};

} // namespace detail

template<typename T>
void AddComponent(World& world, Entity e, const T& component) {
    using Internal = typename detail::ComponentConverter<T>::Internal;
    world.GetInternal()->AddComponent<Internal>(e, detail::ComponentConverter<T>::ToInternal(component));
}

template<typename T>
void RemoveComponent(World& world, Entity e) {
    using Internal = typename detail::ComponentConverter<T>::Internal;
    world.GetInternal()->RemoveComponent<Internal>(e);
}

template<typename T>
bool HasComponent(const World& world, Entity e) {
    using Internal = typename detail::ComponentConverter<T>::Internal;
    return world.GetInternal()->HasComponent<Internal>(e);
}

template<typename T>
std::optional<T> GetComponent(const World& world, Entity e) {
    using Internal = typename detail::ComponentConverter<T>::Internal;
    const auto* comp = world.GetInternal()->TryGetComponent<Internal>(e);
    if (!comp) return std::nullopt;
    return detail::ComponentConverter<T>::FromInternal(*comp);
}

} // namespace api
} // namespace eyelib
