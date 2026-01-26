#include "TimeBindings.h"
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
    // Create Time table
    lua_newtable(L);

    lua_pushcfunction(L, lua_Time_DeltaTime);
    lua_setfield(L, -2, "DeltaTime");

    lua_pushcfunction(L, lua_Time_Time);
    lua_setfield(L, -2, "Time");

    // Set as global
    lua_setglobal(L, "Time");
}

} // namespace bindings
} // namespace scripting
} // namespace eyelib
