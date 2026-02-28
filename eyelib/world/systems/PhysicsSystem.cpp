#include "PhysicsSystem.h"

#include "eyelib/world/ComponentStorage/TransformStore.h"
#include "eyelib/world/components/RigidBody.h"
#include <glm/gtc/quaternion.hpp>

namespace eyelib {
namespace world {
namespace systems {

void PhysicsSystem::Update(WorldState& state, float dt, const math::Vec3& gravity,
                          std::vector<PendingForceEntry>& pendingForces,
                          std::vector<PendingImpulseEntry>& pendingImpulses,
                          std::vector<PendingTorqueEntry>& pendingTorques,
                          std::vector<PendingAngularImpulseEntry>& pendingAngularImpulses) const {
    auto& transformStore = state.GetTransformStore();
    auto& rigidBodies = state.Components<components::RigidBody>();

    for (const auto& entry : pendingForces) {
        components::RigidBody* rb = rigidBodies.TryGet(entry.entity);
        if (!rb || rb->isKinematic) continue;
        if (entry.mode == ForceMode::Force)
            rb->accumulatedForce += entry.force;
        else
            rb->linearVelocity += entry.force;
    }
    pendingForces.clear();

    for (const auto& entry : pendingImpulses) {
        components::RigidBody* rb = rigidBodies.TryGet(entry.entity);
        if (!rb || rb->isKinematic) continue;
        if (rb->mass > 0.f)
            rb->linearVelocity += entry.impulse / rb->mass;
    }
    pendingImpulses.clear();

    for (const auto& entry : pendingTorques) {
        components::RigidBody* rb = rigidBodies.TryGet(entry.entity);
        if (!rb || rb->isKinematic) continue;
        rb->accumulatedTorque += entry.torque;
    }
    pendingTorques.clear();

    for (const auto& entry : pendingAngularImpulses) {
        components::RigidBody* rb = rigidBodies.TryGet(entry.entity);
        if (!rb || rb->isKinematic) continue;
        if (rb->mass > 0.f)
            rb->angularVelocity += entry.impulse / rb->mass;
    }
    pendingAngularImpulses.clear();

    for (auto [entity, ref] : transformStore.All()) {
        components::RigidBody* rb = rigidBodies.TryGet(entity);
        if (!rb || rb->isKinematic) continue;

        if (rb->mass > 0.f)
            rb->linearVelocity += (rb->accumulatedForce / rb->mass) * dt;
        rb->accumulatedForce = math::Vec3(0.f, 0.f, 0.f);

        if (rb->useGravity)
            rb->linearVelocity += gravity * dt;

        if (rb->mass > 0.f)
            rb->angularVelocity += (rb->accumulatedTorque / rb->mass) * dt;
        rb->accumulatedTorque = math::Vec3(0.f, 0.f, 0.f);

        float angle = glm::length(rb->angularVelocity) * dt;
        if (angle > 1e-9f) {
            math::Vec3 axis = glm::normalize(rb->angularVelocity);
            math::Quat deltaQ = glm::angleAxis(angle, axis);
            ref.Rotation() = ref.Rotation() * deltaQ;
        }

        math::Vec3 pos = ref.Position();
        pos += rb->linearVelocity * dt;
        ref.Position() = pos;
        transformStore.MarkLocalDirty(entity);
    }
}

} // namespace systems
} // namespace world
} // namespace eyelib
