#pragma once

#include "api/ecs/Entity.h"
#include "eyelib/math/Transform.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace eyelib {
namespace scene { class Scene; }
namespace api {

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

    void SetTransform(Entity e, const eyelib::math::Transform& t);
    std::optional<eyelib::math::Transform> GetTransform(Entity e) const;

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

    eyelib::scene::Scene* GetInternal() const;
    static std::shared_ptr<Scene> WrapInternal(eyelib::scene::Scene* scene);

private:
    class Impl;
    Impl* m_impl = nullptr;
};

} // namespace api
} // namespace eyelib

