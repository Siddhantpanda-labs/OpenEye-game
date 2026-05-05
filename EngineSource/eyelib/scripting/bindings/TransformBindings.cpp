#include "TransformBindings.h"
#include "BindingHelpers.h"
#include "BindingRegistry.h"
#include "eyelib/world/World.h"
#include "eyelib/math/Transform.h"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

namespace eyelib {
namespace scripting {
namespace bindings {

static int lua_Transform_GetPosition(lua_State* L) {
    world::World* world = BindingRegistry::GetWorld(L);
    EntityUserdata* ud = GetEntityUserdata(L, 1);
    
    if (!world || !ud || !world->IsAlive(ud->entity)) {
        PushVec3(L, math::Vec3(0.0f));
        return 1;
    }
    
    auto opt = world->GetTransform(ud->entity);
    if (opt) {
        PushVec3(L, opt->position);
    } else {
        PushVec3(L, math::Vec3(0.0f));
    }
    return 1;
}

static int lua_Transform_SetPosition(lua_State* L) {
    world::World* world = BindingRegistry::GetWorld(L);
    EntityUserdata* ud = GetEntityUserdata(L, 1);
    
    if (!world || !ud || !world->IsAlive(ud->entity)) {
        return 0;
    }
    
    float x = static_cast<float>(luaL_checknumber(L, 2));
    float y = static_cast<float>(luaL_checknumber(L, 3));
    float z = static_cast<float>(luaL_checknumber(L, 4));
    
    auto opt = world->GetTransform(ud->entity);
    math::Transform t = opt.value_or(math::Transform{});
    t.position = math::Vec3(x, y, z);
    world->SetTransform(ud->entity, t);
    return 0;
}

void RegisterTransformBindings(lua_State* L, world::World* world) {
    // World is already stored in registry by RegisterEntityBindings
    // But we ensure it's set here too for standalone use
    BindingRegistry::SetWorld(L, world);
    
    // Create Transform table using macros
    LUA_BIND_TABLE_BEGIN(L, "Transform");
    LUA_BIND_METHOD(L, "GetPosition", lua_Transform_GetPosition);
    LUA_BIND_METHOD(L, "SetPosition", lua_Transform_SetPosition);
    LUA_BIND_TABLE_END(L, "Transform");
}

} // namespace bindings
} // namespace scripting
} // namespace eyelib
