#pragma once

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}
#include "eyelib/math/Types.h"
#include "eyelib/ecs/Entity.h"

namespace eyelib {
namespace scripting {
namespace bindings {

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

inline void PushEntity(lua_State* L, ecs::Entity e) {
    lua_pushinteger(L, static_cast<lua_Integer>(e));
}

inline ecs::Entity GetEntity(lua_State* L, int index) {
    if (lua_isnumber(L, index)) {
        return static_cast<ecs::Entity>(lua_tointeger(L, index));
    }
    return ecs::NullEntity;
}

} // namespace bindings
} // namespace scripting
} // namespace eyelib
