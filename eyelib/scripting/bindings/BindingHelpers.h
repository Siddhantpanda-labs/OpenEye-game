#pragma once

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}
#include "eyelib/math/Types.h"
#include "eyelib/ecs/Entity.h"
#include "eyelib/ecs/World.h"
#include "eyelib/scripting/ScriptValue.h"

// Forward declare BindingRegistry for macros
namespace eyelib {
namespace scripting {
namespace bindings {
    class BindingRegistry;
} // namespace bindings
} // namespace scripting
} // namespace eyelib

namespace eyelib {
namespace scripting {
namespace bindings {

// Forward declarations
struct EntityUserdata;

// Entity userdata structure
struct EntityUserdata {
    ecs::Entity entity;
    ecs::World* world;  // For validation
    
    EntityUserdata(ecs::Entity e, ecs::World* w) : entity(e), world(w) {}
};

// Helper functions for pushing/pulling Lua values

inline void PushVec3(lua_State* L, const math::Vec3& v) {
    lua_newtable(L);
    lua_pushnumber(L, v.x);
    lua_setfield(L, -2, "x");
    lua_pushnumber(L, v.y);
    lua_setfield(L, -2, "y");
    lua_pushnumber(L, v.z);
    lua_setfield(L, -2, "z");
}

inline math::Vec3 GetVec3(lua_State* L, int index) {
    math::Vec3 v{0.0f};
    if (lua_istable(L, index)) {
        lua_getfield(L, index, "x");
        if (lua_isnumber(L, -1)) v.x = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);
        
        lua_getfield(L, index, "y");
        if (lua_isnumber(L, -1)) v.y = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);
        
        lua_getfield(L, index, "z");
        if (lua_isnumber(L, -1)) v.z = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);
    }
    return v;
}

// Entity userdata management
inline void PushEntity(lua_State* L, ecs::Entity e, ecs::World* world) {
    // Allocate userdata
    EntityUserdata* ud = static_cast<EntityUserdata*>(
        lua_newuserdata(L, sizeof(EntityUserdata))
    );
    new(ud) EntityUserdata(e, world);
    
    // Set metatable (must be registered first via RegisterEntityMetatable)
    luaL_getmetatable(L, "Entity");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        // Fallback: push as integer if metatable not registered
        lua_pop(L, 1);
        lua_pushinteger(L, static_cast<lua_Integer>(e));
        return;
    }
    lua_setmetatable(L, -2);
}

inline EntityUserdata* GetEntityUserdata(lua_State* L, int index) {
    EntityUserdata* ud = static_cast<EntityUserdata*>(luaL_checkudata(L, index, "Entity"));
    return ud;
}

inline ecs::Entity GetEntity(lua_State* L, int index) {
    // Try userdata first
    if (lua_isuserdata(L, index)) {
        EntityUserdata* ud = static_cast<EntityUserdata*>(luaL_testudata(L, index, "Entity"));
        if (ud) {
            // Validate entity
            if (ud->world && ud->world->IsAlive(ud->entity)) {
                return ud->entity;
            }
            return ecs::NullEntity;
        }
    }
    
    // Fallback to integer (for backward compatibility during migration)
    if (lua_isnumber(L, index)) {
        return static_cast<ecs::Entity>(lua_tointeger(L, index));
    }
    
    return ecs::NullEntity;
}

// Register Entity metatable (call once per VM)
void RegisterEntityMetatable(lua_State* L);

// Push ScriptValue to Lua stack
void PushScriptValue(lua_State* L, const scripting::ScriptValue& value, ecs::World* world);

// ============================================================================
// Binding Macros - Industry Standard Pattern
// ============================================================================

/**
 * @brief Register a function in a Lua table (creates table if needed)
 * 
 * Usage:
 *   LUA_BIND_FUNCTION(L, "Entity", "Create", lua_Entity_Create);
 */
#define LUA_BIND_FUNCTION(L, tableName, funcName, func) \
    do { \
        lua_getglobal(L, tableName); \
        if (lua_isnil(L, -1)) { \
            lua_pop(L, 1); \
            lua_newtable(L); \
        } \
        lua_pushcfunction(L, func); \
        lua_setfield(L, -2, funcName); \
        lua_setglobal(L, tableName); \
    } while(0)

/**
 * @brief Create a new Lua table and register functions
 * 
 * Usage:
 *   LUA_BIND_TABLE_BEGIN(L, "Entity");
 *   LUA_BIND_METHOD(L, "Create", lua_Entity_Create);
 *   LUA_BIND_METHOD(L, "Destroy", lua_Entity_Destroy);
 *   LUA_BIND_TABLE_END(L);
 */
#define LUA_BIND_TABLE_BEGIN(L, tableName) \
    lua_newtable(L)

#define LUA_BIND_METHOD(L, funcName, func) \
    do { \
        lua_pushcfunction(L, func); \
        lua_setfield(L, -2, funcName); \
    } while(0)

#define LUA_BIND_TABLE_END(L, tableName) \
    lua_setglobal(L, tableName)

/**
 * @brief Check if a required argument is present and of correct type
 * 
 * Usage:
 *   LUA_CHECK_ARG(L, 1, "number", "x coordinate");
 */
#define LUA_CHECK_ARG(L, index, type, name) \
    do { \
        if (lua_gettop(L) < (index)) { \
            return luaL_error(L, "Missing argument %d: %s (%s required)", index, name, type); \
        } \
    } while(0)

/**
 * @brief Get World pointer from registry with error checking
 * 
 * Usage:
 *   ecs::World* world = LUA_GET_WORLD(L);
 *   if (!world) return luaL_error(L, "World not available");
 */
#define LUA_GET_WORLD(L) \
    (eyelib::scripting::bindings::BindingRegistry::GetWorld(L))

} // namespace bindings
} // namespace scripting
} // namespace eyelib
