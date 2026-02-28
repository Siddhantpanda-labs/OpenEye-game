#include "CollisionResponseSystem.h"

#include "eyelib/world/components/RigidBody.h"
#include <glm/geometric.hpp>

namespace eyelib {
namespace world {
namespace systems {

void CollisionResponseSystem::Update(WorldState& state, const std::vector<ContactEvent>& contacts, std::function<void(const ContactEvent&)> enqueueContact) {
    auto& rigidBodies = state.Components<components::RigidBody>();

    for (const ContactEvent& evt : contacts) {
        enqueueContact(evt);

        components::RigidBody* rbA = rigidBodies.TryGet(evt.entityA);
        components::RigidBody* rbB = rigidBodies.TryGet(evt.entityB);
        if (!rbA && !rbB) continue;
        if (rbA && rbA->isKinematic && rbB && rbB->isKinematic) continue;

        math::Vec3 n = evt.normal;
        math::Vec3 vA = rbA && !rbA->isKinematic ? rbA->linearVelocity : math::Vec3(0.f);
        math::Vec3 vB = rbB && !rbB->isKinematic ? rbB->linearVelocity : math::Vec3(0.f);
        float vRelN = glm::dot(vB - vA, n);
        if (vRelN >= 0.f) continue;

        float invMassA = (rbA && !rbA->isKinematic && rbA->mass > 0.f) ? 1.f / rbA->mass : 0.f;
        float invMassB = (rbB && !rbB->isKinematic && rbB->mass > 0.f) ? 1.f / rbB->mass : 0.f;
        float invSum = invMassA + invMassB;
        if (invSum < 1e-9f) continue;

        float restA = rbA ? rbA->restitution : 0.5f;
        float restB = rbB ? rbB->restitution : 0.5f;
        float restitution = (restA + restB) * 0.5f;

        float impulseMag = -(1.f + restitution) * vRelN / invSum;
        math::Vec3 impulse = impulseMag * n;

        if (rbA && !rbA->isKinematic)
            rbA->linearVelocity += impulse * invMassA;
        if (rbB && !rbB->isKinematic)
            rbB->linearVelocity -= impulse * invMassB;
    }
}

} // namespace systems
} // namespace world
} // namespace eyelib
