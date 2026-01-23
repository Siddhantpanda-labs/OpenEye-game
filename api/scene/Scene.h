#pragma once

#include "eyelib/math/Transform.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace eyelib {
namespace api {

using Entity = uint32_t;
static constexpr Entity NullEntity = 0;

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

private:
    class Impl;
    Impl* m_impl = nullptr;
};

} // namespace api
} // namespace eyelib

