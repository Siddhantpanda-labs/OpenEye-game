#include "EntityBindings.h"
#include "BindingHelpers.h"
#include "BindingRegistry.h"
#include "eyelib/ecs/World.h"
#include "eyelib/ecs/components/Transform.h"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

namespace eyelib {
namespace scripting {
namespace bindings {

static int lua_Entity_Create(lua_State* L) {
    ecs::World* world = BindingRegistry::GetWorld(L);
    if (!world) {
        lua_pushnil(L);
        return 1;
    }
    
    const char* name = luaL_optstring(L, 1, nullptr);
    ecs::Entity e = name ? world->CreateEntity(name) : world->CreateEntity();
    PushEntity(L, e, world);
    return 1;
}

static int lua_Entity_IsValid(lua_State* L) {
    ecs::World* world = BindingRegistry::GetWorld(L);
    EntityUserdata* ud = GetEntityUserdata(L, 1);
    
    if (!world || !ud) {
        lua_pushboolean(L, 0);
        return 1;
    }
    
    lua_pushboolean(L, world->IsAlive(ud->entity) ? 1 : 0);
    return 1;
}

static int lua_Entity_Destroy(lua_State* L) {
    ecs::World* world = BindingRegistry::GetWorld(L);
    EntityUserdata* ud = GetEntityUserdata(L, 1);
    
    if (world && ud) {
        world->DestroyEntity(ud->entity);
    }
    return 0;
}

void RegisterEntityBindings(lua_State* L, ecs::World* world) {
    // Store world in registry
    BindingRegistry::SetWorld(L, world);
    
    // Register Entity metatable first
    RegisterEntityMetatable(L);
    
    // Create Entity table using macros
    LUA_BIND_TABLE_BEGIN(L, "Entity");
    LUA_BIND_METHOD(L, "Create", lua_Entity_Create);
    LUA_BIND_METHOD(L, "IsValid", lua_Entity_IsValid);
    LUA_BIND_METHOD(L, "Destroy", lua_Entity_Destroy);
    LUA_BIND_TABLE_END(L, "Entity");
}

} // namespace bindings
} // namespace scripting
} // namespace eyelib
