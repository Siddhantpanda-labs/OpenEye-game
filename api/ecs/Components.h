#pragma once

#include "Entity.h"
#include "eyelib/math/Transform.h"
#include "eyelib/math/Types.h"
#include "api/animation/Animator.h"
#include "api/animation/Skeleton.h"
#include "api/render/Material.h"
#include "api/render/Mesh.h"
#include "eyelib/physics/PhysicsWorld.h"
#include "eyelib/physics/shapes/CollisionShapeFactory.h"

#include <memory>
#include <string>
#include <vector>

namespace eyelib {
namespace api {

struct Transform {
    math::Vec3 position{0.0f, 0.0f, 0.0f};
    math::Quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    math::Vec3 scale{1.0f, 1.0f, 1.0f};
    Entity parent = NullEntity;
    bool dirty = true;
};

struct Name {
    std::string name;
};

struct Renderable {
    std::shared_ptr<Mesh> mesh;
    std::shared_ptr<Material> material;
    bool visible = true;
    std::vector<std::shared_ptr<Mesh>> lodMeshes;
    size_t activeLOD = 0;
};

struct Camera {
    bool isPrimary = false;
    bool isPerspective = true;
    float fov = 45.0f;
    float aspect = 16.0f / 9.0f;
    float nearPlane = 0.1f;
    float farPlane = 1000.0f;
    float orthoSize = 10.0f;
};

struct Light {
    enum class Type {
        Directional = 0,
        Point = 1,
        Spot = 2
    };
    Type type = Type::Directional;
    math::Vec3 color{1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;
    float range = 10.0f;
    float innerConeAngle = 15.0f;
    float outerConeAngle = 30.0f;
    bool castShadows = true;
};

struct RigidBody {
    physics::RigidBodyHandle bodyHandle = 0;
    physics::CollisionShapeHandle shapeHandle = 0;
    bool syncTransform = true;
};

struct AnimatorComponent {
    std::shared_ptr<api::Animator> animator;
    std::shared_ptr<api::Skeleton> skeleton;
};

struct AABB {
    math::Vec3 min{-0.5f, -0.5f, -0.5f};
    math::Vec3 max{0.5f, 0.5f, 0.5f};
    bool isVisible = true;
};

} // namespace api
} // namespace eyelib
