#pragma once

#include "eyelib/ecs/System.h"

namespace eyelib {
namespace ecs {

class ScriptSystem : public ISystem {
public:
    ScriptSystem() = default;
    
    void Update(World& world, float deltaTime) override;
    std::string GetName() const override { return "ScriptSystem"; }
    int GetUpdateOrder() const override { return 100; } // Run after most systems

    // Script lifecycle
    void InitializeScript(Entity entity, components::ScriptComponent& script);
    void UpdateScript(Entity entity, components::ScriptComponent& script, float deltaTime);
    void ShutdownScript(Entity entity, components::ScriptComponent& script);

    // Event handling
    void OnCollision(Entity entity, Entity other);
    void OnTriggerEnter(Entity entity, Entity other);
    void OnTriggerExit(Entity entity, Entity other);

    // Hot-reload
    void ReloadScript(Entity entity);

private:
    World* m_world = nullptr;
};

} // namespace ecs
} // namespace eyelib
