#include "World.h"

#include "eyelib/ecs/World.h"

namespace eyelib {
namespace api {

class World::Impl {
public:
    eyelib::ecs::World world{};
};

World::World() : m_impl(new Impl()) {}

World::~World() {
    delete m_impl;
    m_impl = nullptr;
}

Entity World::CreateEntity() {
    return m_impl->world.CreateEntity();
}

void World::DestroyEntity(Entity e) {
    m_impl->world.DestroyEntity(e);
}

bool World::IsAlive(Entity e) const {
    return m_impl->world.IsAlive(e);
}

void World::SetName(Entity e, const std::string& name) {
    m_impl->world.SetName(e, name);
}

std::optional<std::string> World::GetName(Entity e) const {
    return m_impl->world.GetName(e);
}

void World::SetTransform(Entity e, const eyelib::math::Transform& t) {
    m_impl->world.SetTransform(e, t);
}

std::optional<eyelib::math::Transform> World::GetTransform(Entity e) const {
    return m_impl->world.GetTransform(e);
}

} // namespace api
} // namespace eyelib

