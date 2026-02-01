#pragma once

#include "api/scripting/All.h"
#include "api/world/All.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace openeye::player {

class WorldPhysicsBridge;
class WorldAnimationBridge;

class WorldScriptingBridge {
public:
    void BuildFromWorld(eyelib::api::world::World& world, const std::vector<std::string>& assetRoots);
    void Update(eyelib::api::world::World& world, const std::vector<std::string>& assetRoots, float deltaTime);
    void SetPhysicsBridge(WorldPhysicsBridge* physics);
    void SetAnimationBridge(WorldAnimationBridge* animation);

private:
    struct ScriptBinding {
        ::eyelib::world::EntityId entity;
        ::eyelib::scripting::ScriptHandle handle = eyelib::scripting::InvalidScriptHandle;
        std::string resolvedPath;
    };

    std::string ResolveScriptPath(const std::string& name, const std::vector<std::string>& assetRoots) const;
    void LoadOrReload(eyelib::api::world::World& world,
                      const eyelib::api::world::World::ScriptEntry& script,
                      const std::vector<std::string>& assetRoots,
                      bool forceReload);
    void UnloadBinding(const ::eyelib::world::EntityId& entity);

    std::unordered_map<::eyelib::world::EntityId, ScriptBinding, ::eyelib::world::EntityIdHash> m_bindings;
    WorldPhysicsBridge* m_physics = nullptr;
    WorldAnimationBridge* m_animation = nullptr;
};

} // namespace openeye::player
