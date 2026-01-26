#include "EntityBindings.h"
#include "BindingHelpers.h"
#include "eyelib/ecs/World.h"
#include "eyelib/ecs/components/Transform.h"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

namespace eyelib {
namespace scripting {
namespace bindings {

static ecs::World* s_world = nullptr;

static int lua_Entity_Create(lua_State* L) {
    if (!s_world) {
        lua_pushnil(L);
        return 1;
    }
    
    const char* name = luaL_optstring(L, 1, nullptr);
    ecs::Entity e = name ? s_world->CreateEntity(name) : s_world->CreateEntity();
    PushEntity(L, e);
    return 1;
}

static int lua_Entity_IsValid(lua_State* L) {
    ecs::Entity e = GetEntity(L, 1);
    if (!s_world) {
        lua_pushboolean(L, 0);
        return 1;
    }
    lua_pushboolean(L, s_world->IsAlive(e) ? 1 : 0);
    return 1;
}

static int lua_Entity_Destroy(lua_State* L) {
    ecs::Entity e = GetEntity(L, 1);
    if (s_world) {
        s_world->DestroyEntity(e);
    }
    return 0;
}

void RegisterEntityBindings(lua_State* L, ecs::World* world) {
    s_world = world;
    
    // Create Entity table
    lua_newtable(L);

    lua_pushcfunction(L, lua_Entity_Create);
    lua_setfield(L, -2, "Create");

    lua_pushcfunction(L, lua_Entity_IsValid);
    lua_setfield(L, -2, "IsValid");

    lua_pushcfunction(L, lua_Entity_Destroy);
    lua_setfield(L, -2, "Destroy");

    // Set as global
    lua_setglobal(L, "Entity");
}

} // namespace bindings
} // namespace scripting
} // namespace eyelib
