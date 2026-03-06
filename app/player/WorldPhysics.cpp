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
    m_physicsAccumulator = 0.0f;
    m_physicsFirstFrame = true;
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

const eyelib::api::world::World::ColliderEntry* WorldPhysicsBridge::FindCollider(
    std::span<const eyelib::api::world::World::ColliderEntry> colliders,
    ::eyelib::world::EntityId entity) const {
    for (const auto& entry : colliders) {
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
    const eyelib::math::Vec3 scale = transform ? transform->scale : eyelib::math::Vec3(1.0f);
    const eyelib::math::Quat rotation = transform ? transform->rotation : eyelib::math::Quat(1.0f, 0.0f, 0.0f, 0.0f);

    eyelib::api::physics::CollisionShapeHandle shape;
    switch (collider.component.type) {
        case eyelib::world::components::Collider::Type::Box: {
            eyelib::math::Vec3 scaledSize(
                collider.component.size.x * scale.x,
                collider.component.size.y * scale.y,
                collider.component.size.z * scale.z);
            shape = m_physics->CreateBoxShape(scaledSize * 0.5f);
            break;
        }
        case eyelib::world::components::Collider::Type::Sphere: {
            float scaleFactor = (scale.x + scale.y + scale.z) / 3.0f;
            shape = m_physics->CreateSphereShape(collider.component.radius * scaleFactor);
            break;
        }
        case eyelib::world::components::Collider::Type::Capsule: {
            float scaleFactor = (scale.x + scale.y + scale.z) / 3.0f;
            shape = m_physics->CreateCapsuleShape(
                collider.component.radius * scaleFactor,
                collider.component.height * scaleFactor);
            break;
        }
        default:
            return false;
    }

    if (!shape) {
        return false;
    }

    const float mass = rigidBody.component.mass;
    const auto basePos = transform ? transform->position : eyelib::math::Vec3(0.0f);
    const eyelib::math::Vec3 scaledCenter(
        collider.component.center.x * scale.x,
        collider.component.center.y * scale.y,
        collider.component.center.z * scale.z);
    const auto position = basePos + (rotation * scaledCenter);

    const auto body = eyelib::api::physics::RigidBody::Create(*m_physics, position, rotation, shape, mass);
    if (!body) {
        m_physics->DestroyShape(shape);
        return false;
    }

    if (mass > 0.0f) {
        eyelib::api::physics::RigidBody::SetKinematic(*m_physics, body, rigidBody.component.isKinematic);
        if (!rigidBody.component.useGravity) {
            eyelib::api::physics::RigidBody::SetGravity(*m_physics, body, eyelib::math::Vec3(0.0f, 0.0f, 0.0f));
        }
    }

    m_bindings.push_back({ collider.entity, body, shape, rigidBody.component.syncTransform });
    return true;
}

bool WorldPhysicsBridge::BuildBodyForRigidBodyOnly(
    eyelib::api::world::World& world,
    const eyelib::api::world::World::RigidBodyEntry& rigidBody) {
    const auto transform = world.GetObservation().GetTransform(rigidBody.entity);
    const auto position = transform ? transform->position : eyelib::math::Vec3(0.0f);
    const auto rotation = transform ? transform->rotation : eyelib::math::Quat(1.0f, 0.0f, 0.0f, 0.0f);

    eyelib::api::physics::CollisionShapeHandle shape =
        m_physics->CreateBoxShape(eyelib::math::Vec3(0.5f, 0.5f, 0.5f));
    if (!shape) {
        return false;
    }

    const float mass = rigidBody.component.mass;
    const auto body = eyelib::api::physics::RigidBody::Create(
        *m_physics, position, rotation, shape, mass);
    if (!body) {
        m_physics->DestroyShape(shape);
        return false;
    }

    if (mass > 0.0f) {
        eyelib::api::physics::RigidBody::SetKinematic(*m_physics, body, rigidBody.component.isKinematic);
        if (!rigidBody.component.useGravity) {
            eyelib::api::physics::RigidBody::SetGravity(*m_physics, body, eyelib::math::Vec3(0.0f, 0.0f, 0.0f));
        }
    }

    m_bindings.push_back({ rigidBody.entity, body, shape, rigidBody.component.syncTransform });
    return true;
}

::eyelib::world::EntityId WorldPhysicsBridge::EntityFromBody(eyelib::api::physics::RigidBodyHandle body) const {
    for (const auto& binding : m_bindings) {
        if (binding.body.Raw() == body.Raw()) {
            return binding.entity;
        }
    }
    return eyelib::world::InvalidEntity;
}

void WorldPhysicsBridge::InstallContactCallback() {
    m_physics->SetContactCallback([this](eyelib::api::physics::RigidBodyHandle a,
                                         eyelib::api::physics::RigidBodyHandle b,
                                         const eyelib::math::Vec3& /*pos*/,
                                         const eyelib::math::Vec3& /*n*/) {
        const auto ea = EntityFromBody(a);
        const auto eb = EntityFromBody(b);
        if (ea != eyelib::world::InvalidEntity && eb != eyelib::world::InvalidEntity) {
            m_contactPairs.emplace_back(ea, eb);
        }
    });
}

bool WorldPhysicsBridge::BuildFromWorld(eyelib::api::world::World& world) {
    if (!m_ready) {
        return false;
    }
    m_bindings.clear();
    m_contactPairs.clear();
    const auto obs = world.GetObservation();
    const auto colliders = obs.GetCollidersView();
    const auto rigidBodies = obs.GetRigidBodiesView();

    bool ok = true;

    for (const auto& collider : colliders) {
        const auto* rb = FindRigidBody(rigidBodies, collider.entity);
        if (rb) {
            if (!BuildBodyForEntity(world, collider, *rb)) {
                ok = false;
            }
        } else {
            eyelib::api::world::World::RigidBodyEntry staticDefaults{};
            staticDefaults.entity = collider.entity;
            staticDefaults.component.mass = 0.0f;
            staticDefaults.component.isKinematic = false;
            staticDefaults.component.syncTransform = false;
            if (!BuildBodyForEntity(world, collider, staticDefaults)) {
                ok = false;
            }
        }
    }

    for (const auto& rb : rigidBodies) {
        if (FindCollider(colliders, rb.entity) != nullptr) {
            continue;
        }
        if (!BuildBodyForRigidBodyOnly(world, rb)) {
            ok = false;
        }
    }

    InstallContactCallback();
    return ok;
}

void WorldPhysicsBridge::StepPhysicsOnly(float deltaSeconds, int maxSubSteps, float fixedTimeStep) {
    if (!m_ready) {
        return;
    }
    m_contactPairs.clear();
    const float fixedStep = fixedTimeStep > 0.0f ? fixedTimeStep : (1.0f / 60.0f);

    if (m_physicsFirstFrame) {
        m_physicsFirstFrame = false;
    } else {
        m_physicsAccumulator += deltaSeconds;
    }
    m_physicsAccumulator = (m_physicsAccumulator > 0.25f) ? 0.25f : m_physicsAccumulator;

    const int steps = static_cast<int>(m_physicsAccumulator / fixedStep);
    const int stepsToRun = (steps > maxSubSteps) ? maxSubSteps : steps;
    if (stepsToRun > 0) {
        m_physics->Step(stepsToRun * fixedStep, stepsToRun, fixedStep);
        m_physicsAccumulator -= stepsToRun * fixedStep;
        if (m_physicsAccumulator < 0.0f) m_physicsAccumulator = 0.0f;
    }
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

std::vector<::eyelib::world::EntityId> WorldPhysicsBridge::GetCollisions(::eyelib::world::EntityId entity) const {
    std::vector<::eyelib::world::EntityId> out;
    for (const auto& p : m_contactPairs) {
        if (p.first == entity) {
            out.push_back(p.second);
        } else if (p.second == entity) {
            out.push_back(p.first);
        }
    }
    return out;
}

bool WorldPhysicsBridge::SetTransform(eyelib::api::world::World& world, ::eyelib::world::EntityId entity,
                                      const eyelib::math::Vec3& position, const eyelib::math::Quat& rotation) {
    auto* binding = FindBinding(entity);
    if (!binding || !binding->body) {
        return false;
    }
    if (!eyelib::api::physics::RigidBody::SetTransform(*m_physics, binding->body, position, rotation)) {
        return false;
    }
    auto transform = world.GetObservation().GetTransform(entity).value_or(eyelib::math::Transform{});
    transform.position = position;
    transform.rotation = rotation;
    world.SetTransform(entity, transform);
    return true;
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
