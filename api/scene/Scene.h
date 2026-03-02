#pragma once

#include "eyelib/world/EntityId.h"
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
    using Entity = ::eyelib::world::EntityId;
    static constexpr Entity NullEntity = ::eyelib::world::InvalidEntity;

    Scene();
    ~Scene();

    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;

    Scene::Entity CreateEntity(const std::string& name = "Entity");
    void DestroyEntity(Scene::Entity e);
    bool IsAlive(Scene::Entity e) const;
    void Clear();

    void SetName(Scene::Entity e, const std::string& name);
    std::optional<std::string> GetName(Scene::Entity e) const;

    void SetTransform(Scene::Entity e, const eyelib::math::Transform& t);
    std::optional<eyelib::math::Transform> GetTransform(Scene::Entity e) const;

    bool SetParent(Scene::Entity child, Scene::Entity parent);
    Scene::Entity GetParent(Scene::Entity child) const;
    std::vector<Scene::Entity> GetChildren(Scene::Entity parent) const;
    std::vector<Scene::Entity> GetEntities() const;
    std::vector<Scene::Entity> GetRootEntities() const;
    std::string GetEntityPath(Scene::Entity entity) const;
    Scene::Entity FindEntity(const std::string& path) const;
    size_t GetSiblingCount(Scene::Entity entity) const;
    size_t GetChildCount(Scene::Entity entity) const;
    bool MoveEntity(Scene::Entity entity, Scene::Entity newParent);
    Scene::Entity DuplicateEntity(Scene::Entity entity);

    eyelib::scene::Scene* GetInternal() const;
    static std::shared_ptr<Scene> WrapInternal(eyelib::scene::Scene* scene);

private:
    class Impl;
    Impl* m_impl = nullptr;
};

} // namespace api
} // namespace eyelib

