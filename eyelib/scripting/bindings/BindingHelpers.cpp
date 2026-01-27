#include "BindingHelpers.h"
#include "BindingRegistry.h"
#include "eyelib/core/Log.h"
#include "eyelib/scripting/ScriptValue.h"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

namespace eyelib {
namespace scripting {
namespace bindings {

// Metamethods for Entity userdata
static int Entity__index(lua_State* L) {
    EntityUserdata* ud = GetEntityUserdata(L, 1);
    const char* key = luaL_checkstring(L, 2);
    
    if (strcmp(key, "id") == 0) {
        lua_pushinteger(L, static_cast<lua_Integer>(ud->entity));
        return 1;
    }
    
    if (strcmp(key, "isValid") == 0) {
        bool valid = (ud->world && ud->world->IsAlive(ud->entity));
        lua_pushboolean(L, valid ? 1 : 0);
        return 1;
    }
    
    lua_pushnil(L);
    return 1;
}

static int Entity__eq(lua_State* L) {
    EntityUserdata* ud1 = GetEntityUserdata(L, 1);
    EntityUserdata* ud2 = GetEntityUserdata(L, 2);
    lua_pushboolean(L, (ud1->entity == ud2->entity) ? 1 : 0);
    return 1;
}

static int Entity__tostring(lua_State* L) {
    EntityUserdata* ud = GetEntityUserdata(L, 1);
    bool valid = (ud->world && ud->world->IsAlive(ud->entity));
    if (valid) {
        lua_pushfstring(L, "Entity(%zu)", ud->entity);
    } else {
        lua_pushfstring(L, "Entity(%zu, invalid)", ud->entity);
    }
    return 1;
}

static int Entity__gc(lua_State* L) {
    EntityUserdata* ud = GetEntityUserdata(L, 1);
    ud->~EntityUserdata();
    return 0;
}

void RegisterEntityMetatable(lua_State* L) {
    // Create metatable
    luaL_newmetatable(L, "Entity");
    
    // Register metamethods
    lua_pushcfunction(L, Entity__index);
    lua_setfield(L, -2, "__index");
    
    lua_pushcfunction(L, Entity__eq);
    lua_setfield(L, -2, "__eq");
    
    lua_pushcfunction(L, Entity__tostring);
    lua_setfield(L, -2, "__tostring");
    
    lua_pushcfunction(L, Entity__gc);
    lua_setfield(L, -2, "__gc");
    
    lua_pop(L, 1); // Pop metatable
}

void PushScriptValue(lua_State* L, const scripting::ScriptValue& value, ecs::World* world) {
    switch (value.type) {
        case scripting::ScriptValue::Type::Bool:
            lua_pushboolean(L, value.GetBool() ? 1 : 0);
            break;
            
        case scripting::ScriptValue::Type::Number:
            lua_pushnumber(L, value.GetNumber());
            break;
            
        case scripting::ScriptValue::Type::String:
            lua_pushstring(L, value.GetString().c_str());
            break;
            
        case scripting::ScriptValue::Type::Vec3:
            PushVec3(L, value.GetVec3());
            break;
            
        case scripting::ScriptValue::Type::Entity:
            PushEntity(L, value.GetEntity(), world);
            break;
            
        case scripting::ScriptValue::Type::Nil:
        default:
            lua_pushnil(L);
            break;
    }
}

} // namespace bindings
} // namespace scripting
} // namespace eyelib
