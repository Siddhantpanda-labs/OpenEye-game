#include "WorldPhysics.h"

#include "api/core/All.h"

namespace openeye::player {

WorldPhysicsBridge::~WorldPhysicsBridge() {
    Shutdown();
}

bool WorldPhysicsBridge::Initialize(eyelib::api::physics::PhysicsWorld::PhysicsBackendType backendType) {
    if (m_ready) {
        return true;
    }
    m_physics = std::make_unique<eyelib::api::physics::PhysicsWorld>(backendType);
    if (!m_physics->Initialize()) {
        m_physics.reset();
        return false;
    }
    m_ready = true;
    return true;
}

void WorldPhysicsBridge::Shutdown() {
    if (!m_ready) {
        return;
    }
    eyelib::api::core::Log::InfoF("[WorldPhysicsBridge] Shutdown (per Play session)");
    for (auto& binding : m_bindings) {
        if (binding.body) {
            eyelib::api::physics::RigidBody::Destroy(*m_physics, binding.body);
        }
        if (binding.shape) {
            m_physics->DestroyShape(binding.shape);
        }
    }
    m_bindings.clear();
    m_physics->Shutdown();
    m_physics.reset();
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

bool WorldPhysicsBridge::BuildBodyForEntity(
    eyelib::api::world::World& world,
    const eyelib::api::world::World::ColliderEntry& collider,
    const eyelib::api::world::World::RigidBodyEntry& rigidBody) {
    const auto transform = world.GetObservation().GetTransform(collider.entity);

    eyelib::api::physics::CollisionShapeHandle shape;
    switch (collider.component.type) {
        case eyelib::world::components::Collider::Type::Box:
            shape = m_physics->CreateBoxShape(collider.component.size * 0.5f);
            break;
        case eyelib::world::components::Collider::Type::Sphere:
            shape = m_physics->CreateSphereShape(collider.component.radius);
            break;
        case eyelib::world::components::Collider::Type::Capsule:
            shape = m_physics->CreateCapsuleShape(collider.component.radius, collider.component.height);
            break;
        default:
            return false;
    }

    if (!shape) {
        return false;
    }

    const float mass = rigidBody.component.mass;
    const auto basePos = transform ? transform->position : eyelib::math::Vec3(0.0f);
    const auto position = basePos + collider.component.center;
    const auto rotation = transform ? transform->rotation : eyelib::math::Quat(1.0f, 0.0f, 0.0f, 0.0f);

    const auto body = eyelib::api::physics::RigidBody::Create(*m_physics, position, rotation, shape, mass);
    if (!body) {
        m_physics->DestroyShape(shape);
        return false;
    }

    eyelib::api::physics::RigidBody::SetKinematic(*m_physics, body, rigidBody.component.isKinematic);
    if (!rigidBody.component.useGravity) {
        eyelib::api::physics::RigidBody::SetGravity(*m_physics, body, eyelib::math::Vec3(0.0f, 0.0f, 0.0f));
    }

    m_bindings.push_back({ collider.entity, body, shape, rigidBody.component.syncTransform });
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
        const auto* rb = FindRigidBody(rigidBodies, collider.entity);
        if (!rb) {
            continue;
        }
        if (!BuildBodyForEntity(world, collider, *rb)) {
            ok = false;
        }
    }
    return ok;
}

void WorldPhysicsBridge::StepPhysicsOnly(float deltaSeconds, int maxSubSteps, float fixedTimeStep) {
    if (!m_ready) {
        return;
    }
    const float step = fixedTimeStep > 0.0f ? fixedTimeStep : (1.0f / 60.0f);
    m_physics->Step(deltaSeconds, maxSubSteps, step);
}

void WorldPhysicsBridge::SyncTransformsToWorld(eyelib::api::world::World& world) {
    if (!m_ready || m_bindings.empty()) {
        return;
    }
    const auto obs = world.GetObservation();
    for (const auto& binding : m_bindings) {
        if (!binding.syncTransform || !binding.body) {
            continue;
        }
        const auto transform = eyelib::api::physics::RigidBody::GetTransform(*m_physics, binding.body);
        if (!transform.has_value()) {
            continue;
        }
        auto updated = obs.GetTransform(binding.entity).value_or(eyelib::math::Transform{});
        updated.position = transform->position;
        updated.rotation = transform->rotation;
        world.SetTransform(binding.entity, updated);
    }
}

void WorldPhysicsBridge::Step(eyelib::api::world::World& world, float deltaSeconds,
                              int maxSubSteps, float fixedTimeStep) {
    StepPhysicsOnly(deltaSeconds, maxSubSteps, fixedTimeStep);
    SyncTransformsToWorld(world);
}

bool WorldPhysicsBridge::GetVelocity(::eyelib::world::EntityId entity, eyelib::math::Vec3& out) {
    if (!m_ready) {
        return false;
    }
    const auto* binding = FindBinding(entity);
    if (!binding || !binding->body) {
        return false;
    }
    out = eyelib::api::physics::RigidBody::GetVelocity(*m_physics, binding->body);
    return true;
}

bool WorldPhysicsBridge::SetVelocity(::eyelib::world::EntityId entity, const eyelib::math::Vec3& value) {
    if (!m_ready) {
        return false;
    }
    auto* binding = FindBinding(entity);
    if (!binding || !binding->body) {
        return false;
    }
    eyelib::api::physics::RigidBody::SetVelocity(*m_physics, binding->body, value);
    return true;
}

bool WorldPhysicsBridge::GetAngularVelocity(::eyelib::world::EntityId entity, eyelib::math::Vec3& out) {
    if (!m_ready) {
        return false;
    }
    const auto* binding = FindBinding(entity);
    if (!binding || !binding->body) {
        return false;
    }
    out = eyelib::api::physics::RigidBody::GetAngularVelocity(*m_physics, binding->body);
    return true;
}

bool WorldPhysicsBridge::SetAngularVelocity(::eyelib::world::EntityId entity, const eyelib::math::Vec3& value) {
    if (!m_ready) {
        return false;
    }
    auto* binding = FindBinding(entity);
    if (!binding || !binding->body) {
        return false;
    }
    eyelib::api::physics::RigidBody::SetAngularVelocity(*m_physics, binding->body, value);
    return true;
}

} // namespace openeye::player
