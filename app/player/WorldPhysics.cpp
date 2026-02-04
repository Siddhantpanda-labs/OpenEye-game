#include "WorldPhysics.h"

#include "api/core/All.h"

namespace openeye::player {

bool WorldPhysicsBridge::Initialize() {
    if (m_ready) {
        return true;
    }
    if (!m_physics.Initialize()) {
        return false;
    }
    m_ready = true;
    return true;
}

void WorldPhysicsBridge::Shutdown() {
    if (!m_ready) {
        return;
    }
    for (auto& binding : m_bindings) {
        if (binding.body != 0) {
            eyelib::api::physics::RigidBody::Destroy(m_physics, binding.body);
        }
        if (binding.shape != 0) {
            eyelib::api::physics::CollisionShape::Destroy(binding.shape);
        }
    }
    m_bindings.clear();
    m_physics.Shutdown();
    m_ready = false;
}

const eyelib::api::world::World::RigidBodyEntry* WorldPhysicsBridge::FindRigidBody(
    std::span<const eyelib::api::world::World::RigidBodyEntry> rigidBodies,
    ::eyelib::world::EntityId entity) const {
    for (const auto& entry : rigidBodies) {
        if (entry.entity == entity) {
            return &entry;
        }
    }
    return nullptr;
}

const WorldPhysicsBridge::BodyBinding* WorldPhysicsBridge::FindBinding(::eyelib::world::EntityId entity) const {
    for (const auto& binding : m_bindings) {
        if (binding.entity == entity) {
            return &binding;
        }
    }
    return nullptr;
}

WorldPhysicsBridge::BodyBinding* WorldPhysicsBridge::FindBinding(::eyelib::world::EntityId entity) {
    for (auto& binding : m_bindings) {
        if (binding.entity == entity) {
            return &binding;
        }
    }
    return nullptr;
}

bool WorldPhysicsBridge::BuildBodyForCollider(
    eyelib::api::world::World& world,
    const eyelib::api::world::World::ColliderEntry& collider,
    std::span<const eyelib::api::world::World::RigidBodyEntry> rigidBodies) {
    const auto transform = world.GetObservation().GetTransform(collider.entity);
    const auto* rigidBody = FindRigidBody(rigidBodies, collider.entity);

    eyelib::api::physics::CollisionShapeHandle shape = 0;
    switch (collider.component.type) {
        case eyelib::world::components::Collider::Type::Box:
            shape = eyelib::api::physics::CollisionShape::CreateBox(collider.component.size * 0.5f);
            break;
        case eyelib::world::components::Collider::Type::Sphere:
            shape = eyelib::api::physics::CollisionShape::CreateSphere(collider.component.radius);
            break;
        case eyelib::world::components::Collider::Type::Capsule:
            shape = eyelib::api::physics::CollisionShape::CreateCapsule(collider.component.radius, collider.component.height);
            break;
    }

    if (shape == 0) {
        return false;
    }

    const float mass = rigidBody ? 1.0f : 0.0f;
    const auto position = transform ? transform->position : eyelib::math::Vec3(0.0f);
    const auto rotation = transform ? transform->rotation : eyelib::math::Quat(1.0f, 0.0f, 0.0f, 0.0f);

    const auto body = eyelib::api::physics::RigidBody::Create(m_physics, position, rotation, shape, mass);
    if (body == 0) {
        eyelib::api::physics::CollisionShape::Destroy(shape);
        return false;
    }

    bool syncTransform = true;
    if (rigidBody) {
        syncTransform = rigidBody->component.syncTransform;
        auto updated = rigidBody->component;
        updated.bodyHandle = body;
        updated.shapeHandle = shape;
        world.SetRigidBody(rigidBody->entity, updated);
    }

    m_bindings.push_back({ collider.entity, body, shape, syncTransform });
    return true;
}

bool WorldPhysicsBridge::BuildFromWorld(eyelib::api::world::World& world) {
    if (!m_ready) {
        return false;
    }
    m_bindings.clear();
    const auto obs = world.GetObservation();
    const auto colliders = obs.GetCollidersView();
    const auto rigidBodies = obs.GetRigidBodiesView();

    bool ok = true;
    for (const auto& collider : colliders) {
        if (!BuildBodyForCollider(world, collider, rigidBodies)) {
            ok = false;
        }
    }
    return ok;
}

void WorldPhysicsBridge::Step(eyelib::api::world::World& world, float deltaSeconds,
                              int maxSubSteps, float fixedTimeStep) {
    if (!m_ready) {
        return;
    }
    const float step = fixedTimeStep > 0.0f ? fixedTimeStep : (1.0f / 60.0f);
    m_physics.Step(deltaSeconds, maxSubSteps, step);

    if (m_bindings.empty()) {
        return;
    }

    const auto obs = world.GetObservation();
    for (const auto& binding : m_bindings) {
        if (!binding.syncTransform || binding.body == 0) {
            continue;
        }
        const auto transform = eyelib::api::physics::RigidBody::GetTransform(m_physics, binding.body);
        if (!transform.has_value()) {
            continue;
        }
        auto updated = obs.GetTransform(binding.entity).value_or(eyelib::math::Transform{});
        updated.position = transform->position;
        updated.rotation = transform->rotation;
        world.SetTransform(binding.entity, updated);
    }
}

bool WorldPhysicsBridge::GetVelocity(::eyelib::world::EntityId entity, eyelib::math::Vec3& out) {
    if (!m_ready) {
        return false;
    }
    const auto* binding = FindBinding(entity);
    if (!binding || binding->body == 0) {
        return false;
    }
    out = eyelib::api::physics::RigidBody::GetVelocity(m_physics, binding->body);
    return true;
}

bool WorldPhysicsBridge::SetVelocity(::eyelib::world::EntityId entity, const eyelib::math::Vec3& value) {
    if (!m_ready) {
        return false;
    }
    auto* binding = FindBinding(entity);
    if (!binding || binding->body == 0) {
        return false;
    }
    eyelib::api::physics::RigidBody::SetVelocity(m_physics, binding->body, value);
    return true;
}

bool WorldPhysicsBridge::GetAngularVelocity(::eyelib::world::EntityId entity, eyelib::math::Vec3& out) {
    if (!m_ready) {
        return false;
    }
    const auto* binding = FindBinding(entity);
    if (!binding || binding->body == 0) {
        return false;
    }
    out = eyelib::api::physics::RigidBody::GetAngularVelocity(m_physics, binding->body);
    return true;
}

bool WorldPhysicsBridge::SetAngularVelocity(::eyelib::world::EntityId entity, const eyelib::math::Vec3& value) {
    if (!m_ready) {
        return false;
    }
    auto* binding = FindBinding(entity);
    if (!binding || binding->body == 0) {
        return false;
    }
    eyelib::api::physics::RigidBody::SetAngularVelocity(m_physics, binding->body, value);
    return true;
}

} // namespace openeye::player
