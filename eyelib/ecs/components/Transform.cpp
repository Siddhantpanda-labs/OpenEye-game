#include "Transform.h"

#include "eyelib/ecs/World.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace eyelib {
namespace ecs {
namespace components {

math::Mat4 Transform::GetLocalMatrix() const {
    math::Mat4 transform(1.0f);
    transform = glm::translate(transform, position);
    transform *= glm::mat4_cast(rotation);
    transform = glm::scale(transform, scale);
    return transform;
}

math::Mat4 Transform::GetWorldMatrix(const World& world) const {
    if (!dirty) return worldMatrix;
    math::Mat4 local = GetLocalMatrix();
    if (parent != NullEntity && world.IsAlive(parent)) {
        if (const auto* parentTransform = world.TryGetComponent<Transform>(parent)) {
            worldMatrix = parentTransform->GetWorldMatrix(world) * local;
            dirty = false;
            return worldMatrix;
        }
    }
    worldMatrix = local;
    dirty = false;
    return worldMatrix;
}

math::Transform Transform::ToTransform() const {
    math::Transform t;
    t.position = position;
    t.rotation = rotation;
    t.scale = scale;
    return t;
}

void Transform::SetFromTransform(const math::Transform& t) {
    position = t.position;
    rotation = t.rotation;
    scale = t.scale;
    MarkDirty();
}

} // namespace components
} // namespace ecs
} // namespace eyelib
