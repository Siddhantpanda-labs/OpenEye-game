#pragma once

#include "api/world/All.h"

#include <string>
#include <unordered_map>

struct lua_State;

namespace openeye::player {
class WorldPhysicsBridge;
class WorldAnimationBridge;

namespace scripting {

void RegisterWorldBindings(lua_State* L,
                           eyelib::api::world::World* world,
                           WorldPhysicsBridge* physics,
                           WorldAnimationBridge* animation);

bool BindScriptInstance(lua_State* L,
                        const std::string& scriptName,
                        const ::eyelib::world::EntityId& entity,
                        const ::eyelib::world::components::Script& component);

bool ReadProperties(lua_State* L,
                    const std::string& scriptName,
                    std::unordered_map<std::string, ::eyelib::world::components::ScriptValue>& out);

bool PropertiesEqual(const std::unordered_map<std::string, ::eyelib::world::components::ScriptValue>& a,
                     const std::unordered_map<std::string, ::eyelib::world::components::ScriptValue>& b);

} // namespace scripting
} // namespace openeye::player
