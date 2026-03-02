#pragma once

struct lua_State;

namespace eyelib {
namespace world { class World; }
namespace scripting {
namespace bindings {

void RegisterEntityBindings(lua_State* L, world::World* world);

} // namespace bindings
} // namespace scripting
} // namespace eyelib
