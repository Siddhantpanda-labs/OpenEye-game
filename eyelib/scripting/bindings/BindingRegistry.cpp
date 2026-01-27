#include "BindingRegistry.h"
#include "eyelib/ecs/World.h"

extern "C" {
#include <lua.h>
}

namespace eyelib {
namespace scripting {
namespace bindings {

const char* BindingRegistry::WORLD_REGISTRY_KEY = "eyelib_world_ptr";

void BindingRegistry::SetWorld(lua_State* L, ecs::World* world) {
    // Store world pointer in Lua registry using a unique key
    lua_pushlightuserdata(L, (void*)WORLD_REGISTRY_KEY);
    lua_pushlightuserdata(L, world);
    lua_settable(L, LUA_REGISTRYINDEX);
}

ecs::World* BindingRegistry::GetWorld(lua_State* L) {
    // Retrieve world pointer from Lua registry
    lua_pushlightuserdata(L, (void*)WORLD_REGISTRY_KEY);
    lua_gettable(L, LUA_REGISTRYINDEX);
    
    if (lua_islightuserdata(L, -1)) {
        ecs::World* world = static_cast<ecs::World*>(lua_touserdata(L, -1));
        lua_pop(L, 1);
        return world;
    }
    
    lua_pop(L, 1);
    return nullptr;
}

void BindingRegistry::ClearWorld(lua_State* L) {
    // Remove world pointer from registry
    lua_pushlightuserdata(L, (void*)WORLD_REGISTRY_KEY);
    lua_pushnil(L);
    lua_settable(L, LUA_REGISTRYINDEX);
}

} // namespace bindings
} // namespace scripting
} // namespace eyelib
