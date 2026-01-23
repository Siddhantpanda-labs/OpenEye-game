#pragma once

#include "eyelib/math/Transform.h"

#include <cstdint>
#include <optional>
#include <string>

namespace eyelib {
namespace ecs {

using Entity = uint32_t;
static constexpr Entity NullEntity = 0;

/**
 * @brief Minimal ECS world (EnTT-backed, hidden in .cpp).
 *
 * For bring-up we expose only Name + Transform. We’ll expand to generic components,
 * queries, and systems once the boundary is stable.
 */
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

    void SetTransform(Entity e, const math::Transform& t);
    std::optional<math::Transform> GetTransform(Entity e) const;

private:
    class Impl;
    Impl* m_impl = nullptr;
};

} // namespace ecs
} // namespace eyelib

