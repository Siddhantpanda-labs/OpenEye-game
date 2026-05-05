#pragma once

#include "eyelib/world/World.h"
#include "eyelib/world/EntityId.h"
#include "eyelib/math/Transform.h"

#include <optional>
#include <string>
#include <vector>

namespace eyelib {
namespace scene {

using Entity = eyelib::world::EntityId;
static constexpr Entity NullEntity = eyelib::world::InvalidEntity;

/**
 * @brief Scene: world ownership + hierarchy utilities.
 *
 * Backed by eyelib::world::World. Hierarchy and entity list come from the world's
 * GraphHierarchy and EntityRegistry.
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

    std::vector<Entity> GetRootEntities() const;
    std::string GetEntityPath(Entity entity) const;
    Entity FindEntity(const std::string& path) const;
    size_t GetSiblingCount(Entity entity) const;
    size_t GetChildCount(Entity entity) const;
    bool MoveEntity(Entity entity, Entity newParent);
    Entity DuplicateEntity(Entity entity);

    world::World& World() { return m_world; }
    const world::World& World() const { return m_world; }

private:
    world::World m_world;
};

} // namespace scene
} // namespace eyelib
