#pragma once

#include "Entity.h"
#include "eyelib/math/Transform.h"

#include <optional>
#include <string>

namespace eyelib {
namespace ecs { class World; }
namespace api {

class World {
public:
    World();
    ~World();

    World(const World&) = delete;
    World& operator=(const World&) = delete;

    Entity CreateEntity();
    Entity CreateEntity(const std::string& name);
    void DestroyEntity(Entity e);
    bool IsAlive(Entity e) const;

    void SetName(Entity e, const std::string& name);
    std::optional<std::string> GetName(Entity e) const;

    void SetTransform(Entity e, const eyelib::math::Transform& t);
    std::optional<eyelib::math::Transform> GetTransform(Entity e) const;

    eyelib::ecs::World* GetInternal() const;

private:
    class Impl;
    Impl* m_impl = nullptr;
};

} // namespace api
} // namespace eyelib

