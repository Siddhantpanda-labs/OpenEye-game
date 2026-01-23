#pragma once

#include "eyelib/math/Transform.h"

#include <cstdint>
#include <optional>
#include <string>

namespace eyelib {
namespace api {

using Entity = uint32_t;
static constexpr Entity NullEntity = 0;

class World {
public:
    World();
    ~World();

    World(const World&) = delete;
    World& operator=(const World&) = delete;

    Entity CreateEntity();
    void DestroyEntity(Entity e);
    bool IsAlive(Entity e) const;

    void SetName(Entity e, const std::string& name);
    std::optional<std::string> GetName(Entity e) const;

    void SetTransform(Entity e, const eyelib::math::Transform& t);
    std::optional<eyelib::math::Transform> GetTransform(Entity e) const;

private:
    class Impl;
    Impl* m_impl = nullptr;
};

} // namespace api
} // namespace eyelib

