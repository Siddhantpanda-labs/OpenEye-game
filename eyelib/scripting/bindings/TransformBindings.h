#pragma once

struct lua_State;

namespace eyelib {
namespace ecs { class World; }
namespace scripting {
namespace bindings {

void RegisterTransformBindings(lua_State* L, ecs::World* world);

} // namespace bindings
} // namespace scripting
} // namespace eyelib
