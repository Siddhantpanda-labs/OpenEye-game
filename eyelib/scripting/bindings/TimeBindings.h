#pragma once

struct lua_State;

namespace eyelib {
namespace scripting {
namespace bindings {

void RegisterTimeBindings(lua_State* L);

} // namespace bindings
} // namespace scripting
} // namespace eyelib
