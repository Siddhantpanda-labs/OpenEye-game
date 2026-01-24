#pragma once

#include "eyelib/ecs/Entity.h"
#include "eyelib/math/Transform.h"
#include "eyelib/math/Types.h"

namespace eyelib {
namespace ecs {
class World;
namespace components {

struct Transform {
    math::Vec3 position{0.0f, 0.0f, 0.0f};
    math::Quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    math::Vec3 scale{1.0f, 1.0f, 1.0f};
    Entity parent = NullEntity;

    mutable math::Mat4 worldMatrix{1.0f};
    mutable bool dirty = true;

    math::Mat4 GetLocalMatrix() const;
    math::Mat4 GetWorldMatrix(const World& world) const;

    void MarkDirty() { dirty = true; }
    math::Transform ToTransform() const;
    void SetFromTransform(const math::Transform& t);
};

} // namespace components
} // namespace ecs
} // namespace eyelib
