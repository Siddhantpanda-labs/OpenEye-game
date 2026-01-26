#include "TransformBindings.h"
#include "BindingHelpers.h"
#include "eyelib/ecs/World.h"
#include "eyelib/ecs/components/Transform.h"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

namespace eyelib {
namespace scripting {
namespace bindings {

static ecs::World* s_world = nullptr;

static int lua_Transform_GetPosition(lua_State* L) {
    ecs::Entity e = GetEntity(L, 1);
    if (!s_world || !s_world->IsAlive(e)) {
        PushVec3(L, math::Vec3(0.0f));
        return 1;
    }
    
    if (auto* transform = s_world->TryGetComponent<ecs::components::Transform>(e)) {
        PushVec3(L, transform->position);
    } else {
        PushVec3(L, math::Vec3(0.0f));
    }
    return 1;
}

static int lua_Transform_SetPosition(lua_State* L) {
    ecs::Entity e = GetEntity(L, 1);
    if (!s_world || !s_world->IsAlive(e)) {
        return 0;
    }
    
    float x = static_cast<float>(luaL_checknumber(L, 2));
    float y = static_cast<float>(luaL_checknumber(L, 3));
    float z = static_cast<float>(luaL_checknumber(L, 4));
    
    if (auto* transform = s_world->TryGetComponent<ecs::components::Transform>(e)) {
        transform->position = math::Vec3(x, y, z);
        transform->MarkDirty();
    } else {
        ecs::components::Transform t;
        t.position = math::Vec3(x, y, z);
        s_world->AddComponent<ecs::components::Transform>(e, t);
    }
    return 0;
}

void RegisterTransformBindings(lua_State* L, ecs::World* world) {
    s_world = world;
    
    // Create Transform table
    lua_newtable(L);

    lua_pushcfunction(L, lua_Transform_GetPosition);
    lua_setfield(L, -2, "GetPosition");

    lua_pushcfunction(L, lua_Transform_SetPosition);
    lua_setfield(L, -2, "SetPosition");

    // Set as global
    lua_setglobal(L, "Transform");
}

} // namespace bindings
} // namespace scripting
} // namespace eyelib
