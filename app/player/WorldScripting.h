#pragma once

#include "api/scripting/All.h"
#include "api/world/All.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace openeye::player {

class WorldScriptingBridge {
public:
    void BuildFromWorld(eyelib::api::world::World& world, const std::vector<std::string>& assetRoots);
    void Update(eyelib::api::world::World& world, const std::vector<std::string>& assetRoots);

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

    std::unordered_map<::eyelib::world::EntityId, ScriptBinding> m_bindings;
};

} // namespace openeye::player
