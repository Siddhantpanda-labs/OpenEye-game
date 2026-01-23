#pragma once

#include "eyelib/ecs/World.h"
#include "eyelib/math/Transform.h"

#include <optional>
#include <string>
#include <vector>

namespace eyelib {
namespace scene {

using Entity = eyelib::ecs::Entity;
static constexpr Entity NullEntity = eyelib::ecs::NullEntity;

/**
 * @brief Scene: world ownership + hierarchy utilities.
 *
 * This is intentionally engine-only (not rendering-dependent). Rendering/physics
 * will consume the Scene/World data later via dedicated systems.
 */
class Scene {
public:
    Scene();
    ~Scene();

    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;

    Entity CreateEntity(const std::string& name = "Entity");
    void DestroyEntity(Entity e);
    bool IsAlive(Entity e) const;
    void Clear();

    void SetName(Entity e, const std::string& name);
    std::optional<std::string> GetName(Entity e) const;

    void SetTransform(Entity e, const math::Transform& t);
    std::optional<math::Transform> GetTransform(Entity e) const;

    // Hierarchy (NullEntity = no parent)
    bool SetParent(Entity child, Entity parent);
    Entity GetParent(Entity child) const;
    std::vector<Entity> GetChildren(Entity parent) const;
    std::vector<Entity> GetEntities() const;

    ecs::World& World() { return m_world; }
    const ecs::World& World() const { return m_world; }

private:
    class Impl;
    Impl* m_impl = nullptr;

    ecs::World m_world{};
};

} // namespace scene
} // namespace eyelib

