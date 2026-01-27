#include "TimeBindings.h"
#include "BindingHelpers.h"
#include "eyelib/core/Time.h"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

namespace eyelib {
namespace scripting {
namespace bindings {

static int lua_Time_DeltaTime(lua_State* L) {
    lua_pushnumber(L, core::Time::DeltaTime());
    return 1;
}

static int lua_Time_Time(lua_State* L) {
    lua_pushnumber(L, core::Time::TotalTime());
    return 1;
}

void RegisterTimeBindings(lua_State* L) {
    // Time bindings don't need World, so no registry access needed
    // Create Time table using macros
    LUA_BIND_TABLE_BEGIN(L, "Time");
    LUA_BIND_METHOD(L, "DeltaTime", lua_Time_DeltaTime);
    LUA_BIND_METHOD(L, "Time", lua_Time_Time);
    LUA_BIND_TABLE_END(L, "Time");
}

} // namespace bindings
} // namespace scripting
} // namespace eyelib
