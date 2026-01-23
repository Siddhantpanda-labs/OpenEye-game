#include "World.h"

#include "entt/entt.hpp"

#include "eyelib/core/Log.h"

namespace eyelib {
namespace ecs {

namespace {
struct NameComponent {
    std::string value;
};

struct TransformComponent {
    math::Transform value{};
};

static Entity ToPublic(entt::entity e) {
    return static_cast<Entity>(static_cast<entt::id_type>(e));
}

static entt::entity ToEnTT(Entity e) {
    return static_cast<entt::entity>(static_cast<entt::id_type>(e));
}
} // namespace

class World::Impl {
public:
    entt::registry reg{};
};

World::World() : m_impl(new Impl()) {}

World::~World() {
    delete m_impl;
    m_impl = nullptr;
}

Entity World::CreateEntity() {
    const auto e = m_impl->reg.create();
    const Entity out = ToPublic(e);
    // Default transform
    m_impl->reg.emplace<TransformComponent>(e, TransformComponent{});
    return out;
}

void World::DestroyEntity(Entity e) {
    if (!IsAlive(e)) return;
    m_impl->reg.destroy(ToEnTT(e));
}

bool World::IsAlive(Entity e) const {
    if (e == NullEntity) return false;
    return m_impl->reg.valid(ToEnTT(e));
}

void World::SetName(Entity e, const std::string& name) {
    if (!IsAlive(e)) return;
    auto ent = ToEnTT(e);
    if (auto* c = m_impl->reg.try_get<NameComponent>(ent)) {
        c->value = name;
    } else {
        m_impl->reg.emplace<NameComponent>(ent, NameComponent{ name });
    }
}

std::optional<std::string> World::GetName(Entity e) const {
    if (!IsAlive(e)) return std::nullopt;
    auto ent = ToEnTT(e);
    if (auto* c = m_impl->reg.try_get<NameComponent>(ent)) {
        return c->value;
    }
    return std::nullopt;
}

void World::SetTransform(Entity e, const math::Transform& t) {
    if (!IsAlive(e)) return;
    auto ent = ToEnTT(e);
    if (auto* c = m_impl->reg.try_get<TransformComponent>(ent)) {
        c->value = t;
    } else {
        m_impl->reg.emplace<TransformComponent>(ent, TransformComponent{ t });
    }
}

std::optional<math::Transform> World::GetTransform(Entity e) const {
    if (!IsAlive(e)) return std::nullopt;
    auto ent = ToEnTT(e);
    if (auto* c = m_impl->reg.try_get<TransformComponent>(ent)) {
        return c->value;
    }
    return std::nullopt;
}

} // namespace ecs
} // namespace eyelib

