#include "World.h"

#include "ComponentRegistry.h"
#include "ComponentStorage.h"
#include "components/All.h"

#include "entt/entt.hpp"

namespace eyelib {
namespace ecs {

namespace {
static Entity ToPublic(entt::entity e) {
    return static_cast<Entity>(static_cast<entt::id_type>(e));
}
} // namespace

class World::Impl {
public:
    ComponentRegistry registry;
    ComponentStorage storage{ registry };
    ::eyelib::physics::PhysicsWorld* physicsWorld = nullptr;
    ::eyelib::render::RenderPipeline* renderPipeline = nullptr;
};

World::World() : m_impl(new Impl()) {
    m_impl->registry.RegisterComponent<components::Name>();
    m_impl->registry.RegisterComponent<components::Transform>();
    m_impl->registry.RegisterComponent<components::Renderable>();
    m_impl->registry.RegisterComponent<components::Camera>();
    m_impl->registry.RegisterComponent<components::Light>();
    m_impl->registry.RegisterComponent<components::RigidBody>();
    m_impl->registry.RegisterComponent<components::Animator>();
    m_impl->registry.RegisterComponent<components::AABB>();
}

World::~World() {
    delete m_impl;
    m_impl = nullptr;
}

Entity World::CreateEntity() {
    const auto e = m_impl->storage.Registry().create();
    const Entity out = ToPublic(e);
    m_impl->storage.AddComponent<components::Transform>(out, components::Transform{});
    return out;
}

Entity World::CreateEntity(const std::string& name) {
    const auto e = m_impl->storage.Registry().create();
    const Entity out = ToPublic(e);
    m_impl->storage.AddComponent<components::Transform>(out, components::Transform{});
    if (!name.empty()) {
        m_impl->storage.AddComponent<components::Name>(out, components::Name{name});
    }
    return out;
}

void World::DestroyEntity(Entity e) {
    if (!IsAlive(e)) return;
    m_impl->storage.Registry().destroy(detail::ToEnTT(e));
}

bool World::IsAlive(Entity e) const {
    if (e == NullEntity) return false;
    return m_impl->storage.Registry().valid(detail::ToEnTT(e));
}

void World::SetName(Entity e, const std::string& name) {
    if (!IsAlive(e)) return;
    if (auto* c = m_impl->storage.Registry().try_get<components::Name>(detail::ToEnTT(e))) {
        c->name = name;
    } else {
        m_impl->storage.AddComponent<components::Name>(e, components::Name{name});
    }
}

std::optional<std::string> World::GetName(Entity e) const {
    if (!IsAlive(e)) return std::nullopt;
    if (auto* c = m_impl->storage.Registry().try_get<components::Name>(detail::ToEnTT(e))) {
        return c->name;
    }
    return std::nullopt;
}

void World::SetTransform(Entity e, const math::Transform& t) {
    if (!IsAlive(e)) return;
    if (auto* c = m_impl->storage.Registry().try_get<components::Transform>(detail::ToEnTT(e))) {
        c->SetFromTransform(t);
    } else {
        components::Transform tr;
        tr.SetFromTransform(t);
        m_impl->storage.AddComponent<components::Transform>(e, tr);
    }
}

std::optional<math::Transform> World::GetTransform(Entity e) const {
    if (!IsAlive(e)) return std::nullopt;
    if (auto* c = m_impl->storage.Registry().try_get<components::Transform>(detail::ToEnTT(e))) {
        return c->ToTransform();
    }
    return std::nullopt;
}

ComponentRegistry& World::Components() {
    return m_impl->registry;
}

const ComponentRegistry& World::Components() const {
    return m_impl->registry;
}

ComponentStorage& World::Storage() {
    return m_impl->storage;
}

const ComponentStorage& World::Storage() const {
    return m_impl->storage;
}

entt::registry& World::Registry() {
    return m_impl->storage.Registry();
}

const entt::registry& World::Registry() const {
    return m_impl->storage.Registry();
}

void World::SetPhysicsWorld(::eyelib::physics::PhysicsWorld* world) {
    m_impl->physicsWorld = world;
}

::eyelib::physics::PhysicsWorld* World::GetPhysicsWorld() const {
    return m_impl->physicsWorld;
}

void World::SetRenderPipeline(::eyelib::render::RenderPipeline* pipeline) {
    m_impl->renderPipeline = pipeline;
}

::eyelib::render::RenderPipeline* World::GetRenderPipeline() const {
    return m_impl->renderPipeline;
}

} // namespace ecs
} // namespace eyelib
