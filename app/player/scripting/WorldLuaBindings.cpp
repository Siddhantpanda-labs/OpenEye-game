#include "scripting/WorldLuaBindings.h"

#include "api/core/All.h"
#include "api/platform/All.h"

#include "WorldAnimation.h"
#include "WorldPhysics.h"

#include "scripting/WorldLuaHelpers.h"
#include "scripting/WorldLuaJson.h"

#include <cctype>
#include <cstdint>
#include <json.hpp>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

namespace openeye::player {

namespace {

constexpr const char* kWorldRegistryKey = "openeye_world_ptr";
constexpr const char* kPhysicsRegistryKey = "openeye_physics_ptr";
constexpr const char* kAnimationRegistryKey = "openeye_animation_ptr";

void SetWorld(lua_State* L, eyelib::api::world::World* world) {
    lua_pushlightuserdata(L, (void*)kWorldRegistryKey);
    lua_pushlightuserdata(L, world);
    lua_settable(L, LUA_REGISTRYINDEX);
}

void SetPhysics(lua_State* L, WorldPhysicsBridge* physics) {
    lua_pushlightuserdata(L, (void*)kPhysicsRegistryKey);
    lua_pushlightuserdata(L, physics);
    lua_settable(L, LUA_REGISTRYINDEX);
}

void SetAnimation(lua_State* L, WorldAnimationBridge* animation) {
    lua_pushlightuserdata(L, (void*)kAnimationRegistryKey);
    lua_pushlightuserdata(L, animation);
    lua_settable(L, LUA_REGISTRYINDEX);
}

eyelib::api::world::World* GetWorld(lua_State* L) {
    lua_pushlightuserdata(L, (void*)kWorldRegistryKey);
    lua_gettable(L, LUA_REGISTRYINDEX);
    if (lua_islightuserdata(L, -1)) {
        auto* world = static_cast<eyelib::api::world::World*>(lua_touserdata(L, -1));
        lua_pop(L, 1);
        return world;
    }
    lua_pop(L, 1);
    return nullptr;
}

WorldPhysicsBridge* GetPhysics(lua_State* L) {
    lua_pushlightuserdata(L, (void*)kPhysicsRegistryKey);
    lua_gettable(L, LUA_REGISTRYINDEX);
    if (lua_islightuserdata(L, -1)) {
        auto* physics = static_cast<WorldPhysicsBridge*>(lua_touserdata(L, -1));
        lua_pop(L, 1);
        return physics;
    }
    lua_pop(L, 1);
    return nullptr;
}

WorldAnimationBridge* GetAnimation(lua_State* L) {
    lua_pushlightuserdata(L, (void*)kAnimationRegistryKey);
    lua_gettable(L, LUA_REGISTRYINDEX);
    if (lua_islightuserdata(L, -1)) {
        auto* animation = static_cast<WorldAnimationBridge*>(lua_touserdata(L, -1));
        lua_pop(L, 1);
        return animation;
    }
    lua_pop(L, 1);
    return nullptr;
}

void PushProperties(lua_State* L,
                    const std::unordered_map<std::string, ::eyelib::world::components::ScriptValue>& props) {
    lua_newtable(L);
    for (const auto& [key, value] : props) {
        PushScriptValue(L, value);
        lua_setfield(L, -2, key.c_str());
    }
}

void PushScriptComponent(lua_State* L, const ::eyelib::world::components::Script& script) {
    lua_newtable(L);
    lua_pushstring(L, script.scriptName.c_str());
    lua_setfield(L, -2, "scriptName");
    lua_pushboolean(L, script.isEnabled ? 1 : 0);
    lua_setfield(L, -2, "isEnabled");
    lua_pushboolean(L, script.needsReload ? 1 : 0);
    lua_setfield(L, -2, "needsReload");
    PushProperties(L, script.properties);
    lua_setfield(L, -2, "properties");
}

bool ReadScriptComponent(lua_State* L, int index, ::eyelib::world::components::Script& out) {
    if (!lua_istable(L, index)) {
        return false;
    }
    ReadStringField(L, index, "scriptName", out.scriptName);
    ReadBoolField(L, index, "isEnabled", out.isEnabled);
    ReadBoolField(L, index, "needsReload", out.needsReload);
    lua_getfield(L, index, "properties");
    if (lua_istable(L, -1)) {
        ReadPropertiesTable(L, lua_gettop(L), out.properties);
    }
    lua_pop(L, 1);
    return true;
}

const char* TimeModeToString(::eyelib::world::WorldScheduler::TimeMode mode) {
    switch (mode) {
        case ::eyelib::world::WorldScheduler::TimeMode::Realtime:
            return "realtime";
        case ::eyelib::world::WorldScheduler::TimeMode::Fixed:
            return "fixed";
        case ::eyelib::world::WorldScheduler::TimeMode::Replay:
            return "replay";
        default:
            return "realtime";
    }
}

bool ParseTimeMode(const char* value, ::eyelib::world::WorldScheduler::TimeMode& out) {
    if (!value) {
        return false;
    }
    std::string mode(value);
    for (auto& ch : mode) {
        ch = static_cast<char>(std::tolower(ch));
    }
    if (mode == "realtime" || mode == "real") {
        out = ::eyelib::world::WorldScheduler::TimeMode::Realtime;
        return true;
    }
    if (mode == "fixed") {
        out = ::eyelib::world::WorldScheduler::TimeMode::Fixed;
        return true;
    }
    if (mode == "replay") {
        out = ::eyelib::world::WorldScheduler::TimeMode::Replay;
        return true;
    }
    return false;
}

int lua_World_GetPosition(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        lua_pushnil(L);
        return 1;
    }

    const auto entity = ReadEntity(L, 1);
    if (!world->IsAlive(entity)) {
        lua_pushnil(L);
        return 1;
    }

    ::eyelib::world::components::Transform transform;
    if (!world->GetTransform(entity, transform)) {
        lua_pushnil(L);
        return 1;
    }

    PushVec3(L, transform.position);
    return 1;
}

int lua_World_SetPosition(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        return 0;
    }

    const auto entity = ReadEntity(L, 1);
    if (!world->IsAlive(entity)) {
        return 0;
    }

    ::eyelib::world::components::Transform transform;
    if (!world->GetTransform(entity, transform)) {
        transform = ::eyelib::world::components::Transform{};
    }

    ::eyelib::math::Vec3 pos = transform.position;
    if (lua_istable(L, 2)) {
        if (!ReadVec3(L, 2, pos)) {
            return 0;
        }
    } else {
        pos.x = static_cast<float>(luaL_checknumber(L, 2));
        pos.y = static_cast<float>(luaL_checknumber(L, 3));
        pos.z = static_cast<float>(luaL_checknumber(L, 4));
    }
    transform.position = pos;
    world->SetTransform(entity, transform);
    return 0;
}

int lua_World_GetRotation(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        lua_pushnil(L);
        return 1;
    }

    const auto entity = ReadEntity(L, 1);
    if (!world->IsAlive(entity)) {
        lua_pushnil(L);
        return 1;
    }

    ::eyelib::world::components::Transform transform;
    if (!world->GetTransform(entity, transform)) {
        lua_pushnil(L);
        return 1;
    }

    PushQuat(L, transform.rotation);
    return 1;
}

int lua_World_SetRotation(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        return 0;
    }

    const auto entity = ReadEntity(L, 1);
    if (!world->IsAlive(entity)) {
        return 0;
    }

    ::eyelib::world::components::Transform transform;
    if (!world->GetTransform(entity, transform)) {
        transform = ::eyelib::world::components::Transform{};
    }

    ::eyelib::math::Quat rot = transform.rotation;
    if (lua_istable(L, 2)) {
        if (!ReadQuat(L, 2, rot)) {
            return 0;
        }
    } else {
        rot.w = static_cast<float>(luaL_checknumber(L, 2));
        rot.x = static_cast<float>(luaL_checknumber(L, 3));
        rot.y = static_cast<float>(luaL_checknumber(L, 4));
        rot.z = static_cast<float>(luaL_checknumber(L, 5));
    }
    transform.rotation = rot;
    world->SetTransform(entity, transform);
    return 0;
}

int lua_World_GetScale(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        lua_pushnil(L);
        return 1;
    }

    const auto entity = ReadEntity(L, 1);
    if (!world->IsAlive(entity)) {
        lua_pushnil(L);
        return 1;
    }

    ::eyelib::world::components::Transform transform;
    if (!world->GetTransform(entity, transform)) {
        lua_pushnil(L);
        return 1;
    }

    PushVec3(L, transform.scale);
    return 1;
}

int lua_World_SetScale(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        return 0;
    }

    const auto entity = ReadEntity(L, 1);
    if (!world->IsAlive(entity)) {
        return 0;
    }

    ::eyelib::world::components::Transform transform;
    if (!world->GetTransform(entity, transform)) {
        transform = ::eyelib::world::components::Transform{};
    }

    ::eyelib::math::Vec3 scale = transform.scale;
    if (lua_istable(L, 2)) {
        if (!ReadVec3(L, 2, scale)) {
            return 0;
        }
    } else {
        scale.x = static_cast<float>(luaL_checknumber(L, 2));
        scale.y = static_cast<float>(luaL_checknumber(L, 3));
        scale.z = static_cast<float>(luaL_checknumber(L, 4));
    }
    transform.scale = scale;
    world->SetTransform(entity, transform);
    return 0;
}

int lua_World_GetCamera(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        lua_pushnil(L);
        return 1;
    }
    const auto entity = ReadEntity(L, 1);
    if (!world->IsAlive(entity)) {
        lua_pushnil(L);
        return 1;
    }
    ::eyelib::world::components::Camera camera;
    if (!world->GetCamera(entity, camera)) {
        lua_pushnil(L);
        return 1;
    }
    PushCamera(L, camera);
    return 1;
}

int lua_World_SetCamera(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        return 0;
    }
    const auto entity = ReadEntity(L, 1);
    if (!world->IsAlive(entity)) {
        return 0;
    }
    ::eyelib::world::components::Camera camera;
    if (!world->GetCamera(entity, camera)) {
        camera = ::eyelib::world::components::Camera{};
    }
    if (!ReadCamera(L, 2, camera)) {
        return 0;
    }
    world->SetCamera(entity, camera);
    return 0;
}

int lua_World_GetLight(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        lua_pushnil(L);
        return 1;
    }
    const auto entity = ReadEntity(L, 1);
    if (!world->IsAlive(entity)) {
        lua_pushnil(L);
        return 1;
    }
    ::eyelib::world::components::Light light;
    if (!world->GetLight(entity, light)) {
        lua_pushnil(L);
        return 1;
    }
    PushLight(L, light);
    return 1;
}

int lua_World_SetLight(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        return 0;
    }
    const auto entity = ReadEntity(L, 1);
    if (!world->IsAlive(entity)) {
        return 0;
    }
    ::eyelib::world::components::Light light;
    if (!world->GetLight(entity, light)) {
        light = ::eyelib::world::components::Light{};
    }
    if (!ReadLight(L, 2, light)) {
        return 0;
    }
    world->SetLight(entity, light);
    return 0;
}

int lua_World_GetRenderable(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        lua_pushnil(L);
        return 1;
    }
    const auto entity = ReadEntity(L, 1);
    if (!world->IsAlive(entity)) {
        lua_pushnil(L);
        return 1;
    }
    ::eyelib::world::components::Renderable renderable;
    if (!world->GetRenderable(entity, renderable)) {
        lua_pushnil(L);
        return 1;
    }
    PushRenderable(L, renderable);
    return 1;
}

int lua_World_SetRenderable(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        return 0;
    }
    const auto entity = ReadEntity(L, 1);
    if (!world->IsAlive(entity)) {
        return 0;
    }
    ::eyelib::world::components::Renderable renderable;
    if (!world->GetRenderable(entity, renderable)) {
        renderable = ::eyelib::world::components::Renderable{};
    }
    if (!ReadRenderable(L, 2, renderable)) {
        return 0;
    }
    world->SetRenderable(entity, renderable);
    return 0;
}

int lua_World_GetMeshRenderer(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        lua_pushnil(L);
        return 1;
    }
    const auto entity = ReadEntity(L, 1);
    if (!world->IsAlive(entity)) {
        lua_pushnil(L);
        return 1;
    }
    ::eyelib::world::components::MeshRenderer renderer;
    if (!world->GetMeshRenderer(entity, renderer)) {
        lua_pushnil(L);
        return 1;
    }
    PushMeshRenderer(L, renderer);
    return 1;
}

int lua_World_SetMeshRenderer(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        return 0;
    }
    const auto entity = ReadEntity(L, 1);
    if (!world->IsAlive(entity)) {
        return 0;
    }
    ::eyelib::world::components::MeshRenderer renderer;
    if (!world->GetMeshRenderer(entity, renderer)) {
        renderer = ::eyelib::world::components::MeshRenderer{};
    }
    if (!ReadMeshRenderer(L, 2, renderer)) {
        return 0;
    }
    world->SetMeshRenderer(entity, renderer);
    return 0;
}

int lua_World_GetCollider(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        lua_pushnil(L);
        return 1;
    }
    const auto entity = ReadEntity(L, 1);
    if (!world->IsAlive(entity)) {
        lua_pushnil(L);
        return 1;
    }
    ::eyelib::world::components::Collider collider;
    if (!world->GetCollider(entity, collider)) {
        lua_pushnil(L);
        return 1;
    }
    PushCollider(L, collider);
    return 1;
}

int lua_World_SetCollider(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        return 0;
    }
    const auto entity = ReadEntity(L, 1);
    if (!world->IsAlive(entity)) {
        return 0;
    }
    ::eyelib::world::components::Collider collider;
    if (!world->GetCollider(entity, collider)) {
        collider = ::eyelib::world::components::Collider{};
    }
    if (!ReadCollider(L, 2, collider)) {
        return 0;
    }
    world->SetCollider(entity, collider);
    return 0;
}

int lua_World_GetScript(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        lua_pushnil(L);
        return 1;
    }
    const auto entity = ReadEntity(L, 1);
    if (!world->IsAlive(entity)) {
        lua_pushnil(L);
        return 1;
    }
    ::eyelib::world::components::Script script;
    if (!world->GetScript(entity, script)) {
        lua_pushnil(L);
        return 1;
    }
    PushScriptComponent(L, script);
    return 1;
}

int lua_World_SetScript(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        return 0;
    }
    const auto entity = ReadEntity(L, 1);
    if (!world->IsAlive(entity)) {
        return 0;
    }
    ::eyelib::world::components::Script script;
    if (!world->GetScript(entity, script)) {
        script = ::eyelib::world::components::Script{};
    }
    if (!ReadScriptComponent(L, 2, script)) {
        return 0;
    }
    world->SetScript(entity, script);
    return 0;
}

int lua_World_GetRigidBody(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        lua_pushnil(L);
        return 1;
    }
    const auto entity = ReadEntity(L, 1);
    if (!world->IsAlive(entity)) {
        lua_pushnil(L);
        return 1;
    }
    ::eyelib::world::components::RigidBody rigidBody;
    if (!world->GetRigidBody(entity, rigidBody)) {
        lua_pushnil(L);
        return 1;
    }
    PushRigidBody(L, rigidBody);
    return 1;
}

int lua_World_SetRigidBody(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        return 0;
    }
    const auto entity = ReadEntity(L, 1);
    if (!world->IsAlive(entity)) {
        return 0;
    }
    ::eyelib::world::components::RigidBody rigidBody;
    if (!world->GetRigidBody(entity, rigidBody)) {
        rigidBody = ::eyelib::world::components::RigidBody{};
    }
    if (!ReadRigidBody(L, 2, rigidBody)) {
        return 0;
    }
    world->SetRigidBody(entity, rigidBody);
    return 0;
}

int lua_World_GetAnimator(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        lua_pushnil(L);
        return 1;
    }
    const auto entity = ReadEntity(L, 1);
    if (!world->IsAlive(entity)) {
        lua_pushnil(L);
        return 1;
    }
    ::eyelib::world::components::Animator animator;
    if (!world->GetAnimator(entity, animator)) {
        lua_pushnil(L);
        return 1;
    }
    PushAnimator(L, animator);
    return 1;
}

int lua_World_SetAnimator(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        return 0;
    }
    const auto entity = ReadEntity(L, 1);
    if (!world->IsAlive(entity)) {
        return 0;
    }
    ::eyelib::world::components::Animator animator;
    if (!world->GetAnimator(entity, animator)) {
        animator = ::eyelib::world::components::Animator{};
    }
    if (!ReadAnimator(L, 2, animator)) {
        return 0;
    }
    world->SetAnimator(entity, animator);
    return 0;
}

int lua_World_GetName(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        lua_pushnil(L);
        return 1;
    }
    const auto entity = ReadEntity(L, 1);
    if (!world->IsAlive(entity)) {
        lua_pushnil(L);
        return 1;
    }
    ::eyelib::world::components::Name name;
    if (!world->GetName(entity, name)) {
        lua_pushnil(L);
        return 1;
    }
    PushName(L, name);
    return 1;
}

int lua_World_SetName(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        return 0;
    }
    const auto entity = ReadEntity(L, 1);
    if (!world->IsAlive(entity)) {
        return 0;
    }
    ::eyelib::world::components::Name name;
    if (!world->GetName(entity, name)) {
        name = ::eyelib::world::components::Name{};
    }
    if (!ReadName(L, 2, name)) {
        return 0;
    }
    world->SetName(entity, name);
    return 0;
}

int lua_World_GetRegion(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        lua_pushnil(L);
        return 1;
    }
    const auto entity = ReadEntity(L, 1);
    if (!world->IsAlive(entity)) {
        lua_pushnil(L);
        return 1;
    }
    ::eyelib::world::components::Region region;
    if (!world->GetRegion(entity, region)) {
        lua_pushnil(L);
        return 1;
    }
    PushRegion(L, region);
    return 1;
}

int lua_World_SetRegion(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        return 0;
    }
    const auto entity = ReadEntity(L, 1);
    if (!world->IsAlive(entity)) {
        return 0;
    }
    ::eyelib::world::components::Region region;
    if (!world->GetRegion(entity, region)) {
        region = ::eyelib::world::components::Region{};
    }
    if (!ReadRegion(L, 2, region)) {
        return 0;
    }
    world->SetRegion(entity, region);
    return 0;
}

int lua_World_GetRegionMembership(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        lua_pushnil(L);
        return 1;
    }
    const auto entity = ReadEntity(L, 1);
    if (!world->IsAlive(entity)) {
        lua_pushnil(L);
        return 1;
    }
    ::eyelib::world::components::RegionMembership membership;
    if (!world->GetRegionMembership(entity, membership)) {
        lua_pushnil(L);
        return 1;
    }
    PushRegionMembership(L, membership);
    return 1;
}

int lua_World_SetRegionMembership(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        return 0;
    }
    const auto entity = ReadEntity(L, 1);
    if (!world->IsAlive(entity)) {
        return 0;
    }
    ::eyelib::world::components::RegionMembership membership;
    if (!world->GetRegionMembership(entity, membership)) {
        membership = ::eyelib::world::components::RegionMembership{};
    }
    if (!ReadRegionMembership(L, 2, membership)) {
        return 0;
    }
    world->SetRegionMembership(entity, membership);
    return 0;
}

int lua_World_GetAABB(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        lua_pushnil(L);
        return 1;
    }
    const auto entity = ReadEntity(L, 1);
    if (!world->IsAlive(entity)) {
        lua_pushnil(L);
        return 1;
    }
    ::eyelib::world::components::AABB aabb;
    if (!world->GetAABB(entity, aabb)) {
        lua_pushnil(L);
        return 1;
    }
    PushAABB(L, aabb);
    return 1;
}

int lua_World_SetAABB(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        return 0;
    }
    const auto entity = ReadEntity(L, 1);
    if (!world->IsAlive(entity)) {
        return 0;
    }
    ::eyelib::world::components::AABB aabb;
    if (!world->GetAABB(entity, aabb)) {
        aabb = ::eyelib::world::components::AABB{};
    }
    if (!ReadAABB(L, 2, aabb)) {
        return 0;
    }
    world->SetAABB(entity, aabb);
    return 0;
}

int lua_World_GetSystemBindings(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        lua_pushnil(L);
        return 1;
    }
    const auto entity = ReadEntity(L, 1);
    if (!world->IsAlive(entity)) {
        lua_pushnil(L);
        return 1;
    }
    ::eyelib::world::components::SystemBindings bindings;
    if (!world->GetSystemBindings(entity, bindings)) {
        lua_pushnil(L);
        return 1;
    }
    PushSystemBindings(L, bindings);
    return 1;
}

int lua_World_SetSystemBindings(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        return 0;
    }
    const auto entity = ReadEntity(L, 1);
    if (!world->IsAlive(entity)) {
        return 0;
    }
    ::eyelib::world::components::SystemBindings bindings;
    if (!world->GetSystemBindings(entity, bindings)) {
        bindings = ::eyelib::world::components::SystemBindings{};
    }
    if (!ReadSystemBindings(L, 2, bindings)) {
        return 0;
    }
    world->SetSystemBindings(entity, bindings);
    return 0;
}

int lua_World_GetActiveSystems(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        lua_pushnil(L);
        return 1;
    }
    const auto entity = ReadEntity(L, 1);
    if (!world->IsAlive(entity)) {
        lua_pushnil(L);
        return 1;
    }
    ::eyelib::world::components::ActiveSystems systems;
    if (!world->GetActiveSystems(entity, systems)) {
        lua_pushnil(L);
        return 1;
    }
    PushActiveSystems(L, systems);
    return 1;
}

int lua_World_SetActiveSystems(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        return 0;
    }
    const auto entity = ReadEntity(L, 1);
    if (!world->IsAlive(entity)) {
        return 0;
    }
    ::eyelib::world::components::ActiveSystems systems;
    if (!world->GetActiveSystems(entity, systems)) {
        systems = ::eyelib::world::components::ActiveSystems{};
    }
    if (!ReadActiveSystems(L, 2, systems)) {
        return 0;
    }
    world->SetActiveSystems(entity, systems);
    return 0;
}

int lua_World_CreateEntity(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        lua_pushnil(L);
        return 1;
    }
    const char* name = luaL_optstring(L, 1, "");
    const auto entity = world->CreateEntity(name ? name : "");
    if (entity == ::eyelib::world::InvalidEntity) {
        lua_pushnil(L);
        return 1;
    }
    PushEntity(L, entity);
    return 1;
}

int lua_World_DestroyEntity(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        return 0;
    }
    const auto entity = ReadEntity(L, 1);
    world->DestroyEntity(entity);
    return 0;
}

int lua_World_SetParent(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        lua_pushboolean(L, 0);
        return 1;
    }
    const auto child = ReadEntity(L, 1);
    if (lua_isnil(L, 2)) {
        world->ClearParent(child);
        lua_pushboolean(L, 1);
        return 1;
    }
    const auto parent = ReadEntity(L, 2);
    const bool ok = world->SetParent(child, parent);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

int lua_World_ClearParent(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        return 0;
    }
    const auto child = ReadEntity(L, 1);
    world->ClearParent(child);
    return 0;
}

int lua_World_GetParent(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        lua_pushnil(L);
        return 1;
    }
    const auto child = ReadEntity(L, 1);
    const auto parent = world->GetParent(child);
    if (parent == ::eyelib::world::InvalidEntity) {
        lua_pushnil(L);
        return 1;
    }
    PushEntity(L, parent);
    return 1;
}

int lua_World_GetChildren(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        lua_pushnil(L);
        return 1;
    }
    const auto parent = ReadEntity(L, 1);
    const auto children = world->GetChildren(parent);
    PushEntityArray(L, children);
    return 1;
}

int lua_World_GetAncestors(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        lua_pushnil(L);
        return 1;
    }
    const auto entity = ReadEntity(L, 1);
    const auto ancestors = world->GetAncestors(entity);
    PushEntityArray(L, ancestors);
    return 1;
}

int lua_World_GetDescendants(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        lua_pushnil(L);
        return 1;
    }
    const auto entity = ReadEntity(L, 1);
    const auto descendants = world->GetDescendants(entity);
    PushEntityArray(L, descendants);
    return 1;
}

int lua_World_AddTag(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        return 0;
    }
    const auto entity = ReadEntity(L, 1);
    const char* tag = luaL_checkstring(L, 2);
    if (tag) {
        world->AddTag(entity, tag);
    }
    return 0;
}

int lua_World_RemoveTag(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        return 0;
    }
    const auto entity = ReadEntity(L, 1);
    const char* tag = luaL_checkstring(L, 2);
    if (tag) {
        world->RemoveTag(entity, tag);
    }
    return 0;
}

int lua_World_HasTag(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        lua_pushboolean(L, 0);
        return 1;
    }
    const auto entity = ReadEntity(L, 1);
    const char* tag = luaL_checkstring(L, 2);
    const bool ok = tag ? world->HasTag(entity, tag) : false;
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

int lua_World_GetTags(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        lua_pushnil(L);
        return 1;
    }
    const auto entity = ReadEntity(L, 1);
    const auto tags = world->GetTags(entity);
    PushStringArray(L, tags);
    return 1;
}

int lua_World_GetEntitiesWithTag(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        lua_pushnil(L);
        return 1;
    }
    const char* tag = luaL_checkstring(L, 1);
    const auto entities = tag ? world->GetEntitiesWithTag(tag) : std::vector<::eyelib::world::EntityId>{};
    PushEntityArray(L, entities);
    return 1;
}

int lua_World_GetEntitiesWithTags(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        lua_pushnil(L);
        return 1;
    }
    std::vector<std::string> tags;
    if (!ReadStringArray(L, 1, tags)) {
        lua_pushnil(L);
        return 1;
    }
    const auto entities = world->GetEntitiesWithTags(tags);
    PushEntityArray(L, entities);
    return 1;
}

int lua_World_FindEntityByName(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        lua_pushnil(L);
        return 1;
    }
    const char* name = luaL_checkstring(L, 1);
    const auto entity = name ? world->FindEntityByName(name) : ::eyelib::world::InvalidEntity;
    if (entity == ::eyelib::world::InvalidEntity) {
        lua_pushnil(L);
        return 1;
    }
    PushEntity(L, entity);
    return 1;
}

int lua_World_FindEntitiesByName(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        lua_pushnil(L);
        return 1;
    }
    const char* name = luaL_checkstring(L, 1);
    const auto entities = name ? world->FindEntitiesByName(name) : std::vector<::eyelib::world::EntityId>{};
    PushEntityArray(L, entities);
    return 1;
}

int lua_World_FindEntitiesByNameContains(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        lua_pushnil(L);
        return 1;
    }
    const char* namePart = luaL_checkstring(L, 1);
    const auto entities = namePart ? world->FindEntitiesByNameContains(namePart)
                                   : std::vector<::eyelib::world::EntityId>{};
    PushEntityArray(L, entities);
    return 1;
}

int lua_World_FindEntitiesByFlags(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        lua_pushnil(L);
        return 1;
    }
    const int flags = static_cast<int>(luaL_checkinteger(L, 1));
    const bool exactMatch = lua_toboolean(L, 2) != 0;
    const auto entities = world->FindEntitiesByFlags(static_cast<uint32_t>(flags), exactMatch);
    PushEntityArray(L, entities);
    return 1;
}

int lua_World_GetEntities(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        lua_pushnil(L);
        return 1;
    }
    const auto entities = world->GetEntities();
    PushEntityArray(L, entities);
    return 1;
}

int lua_World_GetEntitiesWithComponent(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        lua_pushnil(L);
        return 1;
    }
    const char* name = luaL_checkstring(L, 1);
    const auto entities = name ? world->GetEntitiesWithComponent(name) : std::vector<::eyelib::world::EntityId>{};
    PushEntityArray(L, entities);
    return 1;
}

int lua_World_GetEntitiesWithComponents(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        lua_pushnil(L);
        return 1;
    }
    std::vector<std::string> names;
    if (!ReadStringArray(L, 1, names)) {
        lua_pushnil(L);
        return 1;
    }
    const auto entities = world->GetEntitiesWithComponents(names);
    PushEntityArray(L, entities);
    return 1;
}

int lua_World_GetComponentNames(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        lua_pushnil(L);
        return 1;
    }
    const auto entity = ReadEntity(L, 1);
    const auto names = world->GetComponentNames(entity);
    PushStringArray(L, names);
    return 1;
}

int lua_World_GetTransformsBatch(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        lua_pushnil(L);
        return 1;
    }
    const auto entries = world->GetObservation().GetTransformsView();
    lua_newtable(L);
    int idx = 1;
    for (const auto& entry : entries) {
        lua_newtable(L);
        PushEntity(L, entry.entity);
        lua_setfield(L, -2, "entity");
        PushTransform(L, entry.component);
        lua_setfield(L, -2, "transform");
        lua_rawseti(L, -2, idx++);
    }
    return 1;
}

int lua_World_SetTransformsBatch(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        lua_pushinteger(L, 0);
        return 1;
    }
    if (!lua_istable(L, 1)) {
        lua_pushinteger(L, 0);
        return 1;
    }
    const int absIndex = lua_absindex(L, 1);
    const size_t len = lua_rawlen(L, absIndex);
    int updated = 0;
    for (size_t i = 1; i <= len; ++i) {
        lua_rawgeti(L, absIndex, static_cast<lua_Integer>(i));
        if (!lua_istable(L, -1)) {
            lua_pop(L, 1);
            continue;
        }
        ::eyelib::world::EntityId entity = ::eyelib::world::InvalidEntity;
        lua_getfield(L, -1, "entity");
        if (lua_istable(L, -1)) {
            ReadEntityTable(L, lua_gettop(L), entity);
        }
        lua_pop(L, 1);

        ::eyelib::world::components::Transform transform;
        if (!world->GetTransform(entity, transform)) {
            transform = ::eyelib::world::components::Transform{};
        }
        lua_getfield(L, -1, "transform");
        if (lua_istable(L, -1)) {
            ReadTransform(L, lua_gettop(L), transform);
            if (world->SetTransform(entity, transform)) {
                ++updated;
            }
        }
        lua_pop(L, 1);
        lua_pop(L, 1);
    }
    lua_pushinteger(L, updated);
    return 1;
}

int lua_World_GetCamerasBatch(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        lua_pushnil(L);
        return 1;
    }
    const auto entries = world->GetObservation().GetCamerasView();
    lua_newtable(L);
    int idx = 1;
    for (const auto& entry : entries) {
        lua_newtable(L);
        PushEntity(L, entry.entity);
        lua_setfield(L, -2, "entity");
        PushCamera(L, entry.component);
        lua_setfield(L, -2, "camera");
        lua_rawseti(L, -2, idx++);
    }
    return 1;
}

int lua_World_SetCamerasBatch(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        lua_pushinteger(L, 0);
        return 1;
    }
    if (!lua_istable(L, 1)) {
        lua_pushinteger(L, 0);
        return 1;
    }
    const int absIndex = lua_absindex(L, 1);
    const size_t len = lua_rawlen(L, absIndex);
    int updated = 0;
    for (size_t i = 1; i <= len; ++i) {
        lua_rawgeti(L, absIndex, static_cast<lua_Integer>(i));
        if (!lua_istable(L, -1)) {
            lua_pop(L, 1);
            continue;
        }
        ::eyelib::world::EntityId entity = ::eyelib::world::InvalidEntity;
        lua_getfield(L, -1, "entity");
        if (lua_istable(L, -1)) {
            ReadEntityTable(L, lua_gettop(L), entity);
        }
        lua_pop(L, 1);

        ::eyelib::world::components::Camera camera;
        if (!world->GetCamera(entity, camera)) {
            camera = ::eyelib::world::components::Camera{};
        }
        lua_getfield(L, -1, "camera");
        if (lua_istable(L, -1)) {
            ReadCamera(L, lua_gettop(L), camera);
            if (world->SetCamera(entity, camera)) {
                ++updated;
            }
        }
        lua_pop(L, 1);
        lua_pop(L, 1);
    }
    lua_pushinteger(L, updated);
    return 1;
}

int lua_World_GetLightsBatch(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        lua_pushnil(L);
        return 1;
    }
    const auto entries = world->GetObservation().GetLightsView();
    lua_newtable(L);
    int idx = 1;
    for (const auto& entry : entries) {
        lua_newtable(L);
        PushEntity(L, entry.entity);
        lua_setfield(L, -2, "entity");
        PushLight(L, entry.component);
        lua_setfield(L, -2, "light");
        lua_rawseti(L, -2, idx++);
    }
    return 1;
}

int lua_World_SetLightsBatch(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        lua_pushinteger(L, 0);
        return 1;
    }
    if (!lua_istable(L, 1)) {
        lua_pushinteger(L, 0);
        return 1;
    }
    const int absIndex = lua_absindex(L, 1);
    const size_t len = lua_rawlen(L, absIndex);
    int updated = 0;
    for (size_t i = 1; i <= len; ++i) {
        lua_rawgeti(L, absIndex, static_cast<lua_Integer>(i));
        if (!lua_istable(L, -1)) {
            lua_pop(L, 1);
            continue;
        }
        ::eyelib::world::EntityId entity = ::eyelib::world::InvalidEntity;
        lua_getfield(L, -1, "entity");
        if (lua_istable(L, -1)) {
            ReadEntityTable(L, lua_gettop(L), entity);
        }
        lua_pop(L, 1);

        ::eyelib::world::components::Light light;
        if (!world->GetLight(entity, light)) {
            light = ::eyelib::world::components::Light{};
        }
        lua_getfield(L, -1, "light");
        if (lua_istable(L, -1)) {
            ReadLight(L, lua_gettop(L), light);
            if (world->SetLight(entity, light)) {
                ++updated;
            }
        }
        lua_pop(L, 1);
        lua_pop(L, 1);
    }
    lua_pushinteger(L, updated);
    return 1;
}

int lua_World_GetRenderablesBatch(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        lua_pushnil(L);
        return 1;
    }
    const auto entries = world->GetObservation().GetRenderablesView();
    lua_newtable(L);
    int idx = 1;
    for (const auto& entry : entries) {
        lua_newtable(L);
        PushEntity(L, entry.entity);
        lua_setfield(L, -2, "entity");
        PushRenderable(L, entry.component);
        lua_setfield(L, -2, "renderable");
        lua_rawseti(L, -2, idx++);
    }
    return 1;
}

int lua_World_SetRenderablesBatch(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        lua_pushinteger(L, 0);
        return 1;
    }
    if (!lua_istable(L, 1)) {
        lua_pushinteger(L, 0);
        return 1;
    }
    const int absIndex = lua_absindex(L, 1);
    const size_t len = lua_rawlen(L, absIndex);
    int updated = 0;
    for (size_t i = 1; i <= len; ++i) {
        lua_rawgeti(L, absIndex, static_cast<lua_Integer>(i));
        if (!lua_istable(L, -1)) {
            lua_pop(L, 1);
            continue;
        }
        ::eyelib::world::EntityId entity = ::eyelib::world::InvalidEntity;
        lua_getfield(L, -1, "entity");
        if (lua_istable(L, -1)) {
            ReadEntityTable(L, lua_gettop(L), entity);
        }
        lua_pop(L, 1);

        ::eyelib::world::components::Renderable renderable;
        if (!world->GetRenderable(entity, renderable)) {
            renderable = ::eyelib::world::components::Renderable{};
        }
        lua_getfield(L, -1, "renderable");
        if (lua_istable(L, -1)) {
            ReadRenderable(L, lua_gettop(L), renderable);
            if (world->SetRenderable(entity, renderable)) {
                ++updated;
            }
        }
        lua_pop(L, 1);
        lua_pop(L, 1);
    }
    lua_pushinteger(L, updated);
    return 1;
}

int lua_World_GetMeshRenderersBatch(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        lua_pushnil(L);
        return 1;
    }
    const auto entries = world->GetObservation().GetMeshRenderersView();
    lua_newtable(L);
    int idx = 1;
    for (const auto& entry : entries) {
        lua_newtable(L);
        PushEntity(L, entry.entity);
        lua_setfield(L, -2, "entity");
        PushMeshRenderer(L, entry.component);
        lua_setfield(L, -2, "meshRenderer");
        lua_rawseti(L, -2, idx++);
    }
    return 1;
}

int lua_World_SetMeshRenderersBatch(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        lua_pushinteger(L, 0);
        return 1;
    }
    if (!lua_istable(L, 1)) {
        lua_pushinteger(L, 0);
        return 1;
    }
    const int absIndex = lua_absindex(L, 1);
    const size_t len = lua_rawlen(L, absIndex);
    int updated = 0;
    for (size_t i = 1; i <= len; ++i) {
        lua_rawgeti(L, absIndex, static_cast<lua_Integer>(i));
        if (!lua_istable(L, -1)) {
            lua_pop(L, 1);
            continue;
        }
        ::eyelib::world::EntityId entity = ::eyelib::world::InvalidEntity;
        lua_getfield(L, -1, "entity");
        if (lua_istable(L, -1)) {
            ReadEntityTable(L, lua_gettop(L), entity);
        }
        lua_pop(L, 1);

        ::eyelib::world::components::MeshRenderer renderer;
        if (!world->GetMeshRenderer(entity, renderer)) {
            renderer = ::eyelib::world::components::MeshRenderer{};
        }
        lua_getfield(L, -1, "meshRenderer");
        if (lua_istable(L, -1)) {
            ReadMeshRenderer(L, lua_gettop(L), renderer);
            if (world->SetMeshRenderer(entity, renderer)) {
                ++updated;
            }
        }
        lua_pop(L, 1);
        lua_pop(L, 1);
    }
    lua_pushinteger(L, updated);
    return 1;
}

int lua_World_GetVelocitiesBatch(lua_State* L) {
    auto* world = GetWorld(L);
    auto* physics = GetPhysics(L);
    if (!world || !physics) {
        lua_pushnil(L);
        return 1;
    }
    const auto bodies = world->GetObservation().GetRigidBodiesView();
    lua_newtable(L);
    int idx = 1;
    for (const auto& entry : bodies) {
        ::eyelib::math::Vec3 linear(0.0f);
        ::eyelib::math::Vec3 angular(0.0f);
        const bool hasLinear = physics->GetVelocity(entry.entity, linear);
        const bool hasAngular = physics->GetAngularVelocity(entry.entity, angular);
        if (!hasLinear && !hasAngular) {
            continue;
        }
        lua_newtable(L);
        PushEntity(L, entry.entity);
        lua_setfield(L, -2, "entity");
        if (hasLinear) {
            PushVec3(L, linear);
            lua_setfield(L, -2, "velocity");
        }
        if (hasAngular) {
            PushVec3(L, angular);
            lua_setfield(L, -2, "angularVelocity");
        }
        lua_rawseti(L, -2, idx++);
    }
    return 1;
}

int lua_World_SetVelocitiesBatch(lua_State* L) {
    auto* world = GetWorld(L);
    auto* physics = GetPhysics(L);
    if (!world || !physics) {
        lua_pushinteger(L, 0);
        return 1;
    }
    if (!lua_istable(L, 1)) {
        lua_pushinteger(L, 0);
        return 1;
    }
    const int absIndex = lua_absindex(L, 1);
    const size_t len = lua_rawlen(L, absIndex);
    int updated = 0;
    for (size_t i = 1; i <= len; ++i) {
        lua_rawgeti(L, absIndex, static_cast<lua_Integer>(i));
        if (!lua_istable(L, -1)) {
            lua_pop(L, 1);
            continue;
        }
        ::eyelib::world::EntityId entity = ::eyelib::world::InvalidEntity;
        lua_getfield(L, -1, "entity");
        if (lua_istable(L, -1)) {
            ReadEntityTable(L, lua_gettop(L), entity);
        }
        lua_pop(L, 1);

        bool applied = false;
        ::eyelib::math::Vec3 linear(0.0f);
        lua_getfield(L, -1, "velocity");
        if (lua_istable(L, -1) && ReadVec3(L, lua_gettop(L), linear)) {
            applied |= physics->SetVelocity(entity, linear);
        }
        lua_pop(L, 1);

        ::eyelib::math::Vec3 angular(0.0f);
        lua_getfield(L, -1, "angularVelocity");
        if (lua_istable(L, -1) && ReadVec3(L, lua_gettop(L), angular)) {
            applied |= physics->SetAngularVelocity(entity, angular);
        }
        lua_pop(L, 1);

        if (applied) {
            ++updated;
        }
        lua_pop(L, 1);
    }
    lua_pushinteger(L, updated);
    return 1;
}

int lua_World_SetPaused(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        return 0;
    }
    const bool paused = lua_toboolean(L, 1) != 0;
    world->SetPaused(paused);
    return 0;
}

int lua_World_StepOnce(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        return 0;
    }
    world->StepOnce();
    return 0;
}

int lua_World_SetFixedTimestep(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        return 0;
    }
    const float value = static_cast<float>(luaL_checknumber(L, 1));
    world->SetFixedTimestep(value);
    return 0;
}

int lua_World_GetFixedTimestep(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        lua_pushnumber(L, 0.0);
        return 1;
    }
    lua_pushnumber(L, world->GetFixedTimestep());
    return 1;
}

int lua_World_SetMaxSubsteps(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        return 0;
    }
    const int value = static_cast<int>(luaL_checkinteger(L, 1));
    world->SetMaxSubsteps(value);
    return 0;
}

int lua_World_GetMaxSubsteps(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        lua_pushinteger(L, 0);
        return 1;
    }
    lua_pushinteger(L, world->GetMaxSubsteps());
    return 1;
}

int lua_World_SetTimeScale(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        return 0;
    }
    const float value = static_cast<float>(luaL_checknumber(L, 1));
    world->SetTimeScale(value);
    return 0;
}

int lua_World_GetTimeScale(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        lua_pushnumber(L, 1.0);
        return 1;
    }
    lua_pushnumber(L, world->GetTimeScale());
    return 1;
}

int lua_World_SetTimeMode(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        lua_pushboolean(L, 0);
        return 1;
    }
    const char* value = luaL_checkstring(L, 1);
    ::eyelib::world::WorldScheduler::TimeMode mode;
    if (!ParseTimeMode(value, mode)) {
        lua_pushboolean(L, 0);
        return 1;
    }
    world->SetTimeMode(mode);
    lua_pushboolean(L, 1);
    return 1;
}

int lua_World_GetTimeMode(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        lua_pushstring(L, "realtime");
        return 1;
    }
    lua_pushstring(L, TimeModeToString(world->GetTimeMode()));
    return 1;
}

int lua_World_SetReplayDelta(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        return 0;
    }
    const float value = static_cast<float>(luaL_checknumber(L, 1));
    world->SetReplayDelta(value);
    return 0;
}

int lua_World_GetComponent(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        lua_pushnil(L);
        return 1;
    }
    const auto entity = ReadEntity(L, 1);
    const char* component = luaL_checkstring(L, 2);
    std::string json;
    if (!component || !world->GetComponentJson(entity, component, json)) {
        lua_pushnil(L);
        return 1;
    }
    auto data = nlohmann::json::parse(json, nullptr, false);
    if (data.is_discarded()) {
        lua_pushnil(L);
        return 1;
    }
    JsonToLua(L, data);
    return 1;
}

int lua_World_SetComponent(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        lua_pushboolean(L, 0);
        return 1;
    }
    const auto entity = ReadEntity(L, 1);
    const char* component = luaL_checkstring(L, 2);
    if (!component || !lua_istable(L, 3)) {
        lua_pushboolean(L, 0);
        return 1;
    }
    const auto json = LuaToJson(L, 3).dump();
    const bool ok = world->SetComponentJson(entity, component, json);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

int lua_World_GetComponents(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        lua_pushnil(L);
        return 1;
    }
    const char* component = luaL_checkstring(L, 1);
    if (!component) {
        lua_pushnil(L);
        return 1;
    }
    std::vector<::eyelib::world::EntityId> entities;
    if (lua_gettop(L) >= 2 && lua_istable(L, 2)) {
        ReadEntityArray(L, 2, entities);
    } else {
        entities = world->GetEntitiesWithComponent(component);
    }

    lua_newtable(L);
    int idx = 1;
    for (const auto& entity : entities) {
        std::string json;
        if (!world->GetComponentJson(entity, component, json)) {
            continue;
        }
        auto data = nlohmann::json::parse(json, nullptr, false);
        if (data.is_discarded()) {
            continue;
        }
        lua_newtable(L);
        PushEntity(L, entity);
        lua_setfield(L, -2, "entity");
        JsonToLua(L, data);
        lua_setfield(L, -2, "data");
        lua_rawseti(L, -2, idx++);
    }
    return 1;
}

int lua_World_SetComponents(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        lua_pushinteger(L, 0);
        return 1;
    }
    const char* component = luaL_checkstring(L, 1);
    if (!component || !lua_istable(L, 2)) {
        lua_pushinteger(L, 0);
        return 1;
    }
    const int absIndex = lua_absindex(L, 2);
    const size_t len = lua_rawlen(L, absIndex);
    int updated = 0;
    for (size_t i = 1; i <= len; ++i) {
        lua_rawgeti(L, absIndex, static_cast<lua_Integer>(i));
        if (!lua_istable(L, -1)) {
            lua_pop(L, 1);
            continue;
        }
        ::eyelib::world::EntityId entity = ::eyelib::world::InvalidEntity;
        lua_getfield(L, -1, "entity");
        if (lua_istable(L, -1)) {
            ReadEntityTable(L, lua_gettop(L), entity);
        }
        lua_pop(L, 1);

        lua_getfield(L, -1, "data");
        if (lua_istable(L, -1)) {
            const auto json = LuaToJson(L, lua_gettop(L)).dump();
            if (world->SetComponentJson(entity, component, json)) {
                ++updated;
            }
        }
        lua_pop(L, 1);
        lua_pop(L, 1);
    }
    lua_pushinteger(L, updated);
    return 1;
}

int lua_World_GetMetadata(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        lua_pushnil(L);
        return 1;
    }
    const auto entity = ReadEntity(L, 1);
    std::string name;
    uint32_t flags = 0;
    if (!world->GetMetadata(entity, name, flags)) {
        lua_pushnil(L);
        return 1;
    }
    PushMetadata(L, name, flags);
    return 1;
}

int lua_World_GetEntityName(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        lua_pushnil(L);
        return 1;
    }
    const auto entity = ReadEntity(L, 1);
    std::string name;
    if (!world->GetEntityName(entity, name)) {
        lua_pushnil(L);
        return 1;
    }
    lua_pushstring(L, name.c_str());
    return 1;
}

int lua_World_SetEntityName(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        return 0;
    }
    const auto entity = ReadEntity(L, 1);
    const char* name = luaL_checkstring(L, 2);
    world->SetEntityName(entity, name ? name : "");
    return 0;
}

int lua_World_SetMetadata(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        return 0;
    }
    const auto entity = ReadEntity(L, 1);
    std::string name;
    uint32_t flags = 0;
    if (!ReadMetadata(L, 2, name, flags)) {
        return 0;
    }
    world->SetMetadata(entity, name, flags);
    return 0;
}

int lua_World_ClearMetadata(lua_State* L) {
    auto* world = GetWorld(L);
    if (!world) {
        return 0;
    }
    const auto entity = ReadEntity(L, 1);
    world->ClearMetadata(entity);
    return 0;
}

int lua_World_GetVelocity(lua_State* L) {
    auto* physics = GetPhysics(L);
    if (!physics) {
        lua_pushnil(L);
        return 1;
    }
    const auto entity = ReadEntity(L, 1);
    ::eyelib::math::Vec3 v(0.0f);
    if (!physics->GetVelocity(entity, v)) {
        lua_pushnil(L);
        return 1;
    }
    PushVec3(L, v);
    return 1;
}

int lua_World_SetVelocity(lua_State* L) {
    auto* physics = GetPhysics(L);
    if (!physics) {
        return 0;
    }
    const auto entity = ReadEntity(L, 1);
    ::eyelib::math::Vec3 v(0.0f);
    if (lua_istable(L, 2)) {
        if (!ReadVec3(L, 2, v)) {
            return 0;
        }
    } else {
        v.x = static_cast<float>(luaL_checknumber(L, 2));
        v.y = static_cast<float>(luaL_checknumber(L, 3));
        v.z = static_cast<float>(luaL_checknumber(L, 4));
    }
    physics->SetVelocity(entity, v);
    return 0;
}

int lua_World_GetAngularVelocity(lua_State* L) {
    auto* physics = GetPhysics(L);
    if (!physics) {
        lua_pushnil(L);
        return 1;
    }
    const auto entity = ReadEntity(L, 1);
    ::eyelib::math::Vec3 v(0.0f);
    if (!physics->GetAngularVelocity(entity, v)) {
        lua_pushnil(L);
        return 1;
    }
    PushVec3(L, v);
    return 1;
}

int lua_World_SetAngularVelocity(lua_State* L) {
    auto* physics = GetPhysics(L);
    if (!physics) {
        return 0;
    }
    const auto entity = ReadEntity(L, 1);
    ::eyelib::math::Vec3 v(0.0f);
    if (lua_istable(L, 2)) {
        if (!ReadVec3(L, 2, v)) {
            return 0;
        }
    } else {
        v.x = static_cast<float>(luaL_checknumber(L, 2));
        v.y = static_cast<float>(luaL_checknumber(L, 3));
        v.z = static_cast<float>(luaL_checknumber(L, 4));
    }
    physics->SetAngularVelocity(entity, v);
    return 0;
}

int lua_World_SetAnimFloat(lua_State* L) {
    auto* animation = GetAnimation(L);
    if (!animation) {
        return 0;
    }
    const auto entity = ReadEntity(L, 1);
    const char* name = luaL_checkstring(L, 2);
    const float value = static_cast<float>(luaL_checknumber(L, 3));
    animation->SetFloat(entity, name ? name : "", value);
    return 0;
}

int lua_World_SetAnimBool(lua_State* L) {
    auto* animation = GetAnimation(L);
    if (!animation) {
        return 0;
    }
    const auto entity = ReadEntity(L, 1);
    const char* name = luaL_checkstring(L, 2);
    const bool value = lua_toboolean(L, 3) != 0;
    animation->SetBool(entity, name ? name : "", value);
    return 0;
}

int lua_World_SetAnimTrigger(lua_State* L) {
    auto* animation = GetAnimation(L);
    if (!animation) {
        return 0;
    }
    const auto entity = ReadEntity(L, 1);
    const char* name = luaL_checkstring(L, 2);
    animation->SetTrigger(entity, name ? name : "");
    return 0;
}

void RegisterWorldBindings(lua_State* L,
                           eyelib::api::world::World* world,
                           WorldPhysicsBridge* physics,
                           WorldAnimationBridge* animation) {
    SetWorld(L, world);
    SetPhysics(L, physics);
    SetAnimation(L, animation);
    lua_getglobal(L, "World");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
    }

    // Transform (position, rotation, scale)
    lua_pushcfunction(L, lua_World_GetPosition);
    lua_setfield(L, -2, "GetPosition");
    lua_pushcfunction(L, lua_World_SetPosition);
    lua_setfield(L, -2, "SetPosition");
    lua_pushcfunction(L, lua_World_GetRotation);
    lua_setfield(L, -2, "GetRotation");
    lua_pushcfunction(L, lua_World_SetRotation);
    lua_setfield(L, -2, "SetRotation");
    lua_pushcfunction(L, lua_World_GetScale);
    lua_setfield(L, -2, "GetScale");
    lua_pushcfunction(L, lua_World_SetScale);
    lua_setfield(L, -2, "SetScale");

    // Camera
    lua_pushcfunction(L, lua_World_GetCamera);
    lua_setfield(L, -2, "GetCamera");
    lua_pushcfunction(L, lua_World_SetCamera);
    lua_setfield(L, -2, "SetCamera");

    // Light
    lua_pushcfunction(L, lua_World_GetLight);
    lua_setfield(L, -2, "GetLight");
    lua_pushcfunction(L, lua_World_SetLight);
    lua_setfield(L, -2, "SetLight");

    // Renderable
    lua_pushcfunction(L, lua_World_GetRenderable);
    lua_setfield(L, -2, "GetRenderable");
    lua_pushcfunction(L, lua_World_SetRenderable);
    lua_setfield(L, -2, "SetRenderable");

    // MeshRenderer
    lua_pushcfunction(L, lua_World_GetMeshRenderer);
    lua_setfield(L, -2, "GetMeshRenderer");
    lua_pushcfunction(L, lua_World_SetMeshRenderer);
    lua_setfield(L, -2, "SetMeshRenderer");

    // Collider
    lua_pushcfunction(L, lua_World_GetCollider);
    lua_setfield(L, -2, "GetCollider");
    lua_pushcfunction(L, lua_World_SetCollider);
    lua_setfield(L, -2, "SetCollider");

    // Script
    lua_pushcfunction(L, lua_World_GetScript);
    lua_setfield(L, -2, "GetScript");
    lua_pushcfunction(L, lua_World_SetScript);
    lua_setfield(L, -2, "SetScript");

    // RigidBody
    lua_pushcfunction(L, lua_World_GetRigidBody);
    lua_setfield(L, -2, "GetRigidBody");
    lua_pushcfunction(L, lua_World_SetRigidBody);
    lua_setfield(L, -2, "SetRigidBody");

    // Animator
    lua_pushcfunction(L, lua_World_GetAnimator);
    lua_setfield(L, -2, "GetAnimator");
    lua_pushcfunction(L, lua_World_SetAnimator);
    lua_setfield(L, -2, "SetAnimator");

    // Name
    lua_pushcfunction(L, lua_World_GetName);
    lua_setfield(L, -2, "GetName");
    lua_pushcfunction(L, lua_World_SetName);
    lua_setfield(L, -2, "SetName");

    // Region
    lua_pushcfunction(L, lua_World_GetRegion);
    lua_setfield(L, -2, "GetRegion");
    lua_pushcfunction(L, lua_World_SetRegion);
    lua_setfield(L, -2, "SetRegion");

    // RegionMembership
    lua_pushcfunction(L, lua_World_GetRegionMembership);
    lua_setfield(L, -2, "GetRegionMembership");
    lua_pushcfunction(L, lua_World_SetRegionMembership);
    lua_setfield(L, -2, "SetRegionMembership");

    // AABB
    lua_pushcfunction(L, lua_World_GetAABB);
    lua_setfield(L, -2, "GetAABB");
    lua_pushcfunction(L, lua_World_SetAABB);
    lua_setfield(L, -2, "SetAABB");

    // SystemBindings
    lua_pushcfunction(L, lua_World_GetSystemBindings);
    lua_setfield(L, -2, "GetSystemBindings");
    lua_pushcfunction(L, lua_World_SetSystemBindings);
    lua_setfield(L, -2, "SetSystemBindings");

    // ActiveSystems
    lua_pushcfunction(L, lua_World_GetActiveSystems);
    lua_setfield(L, -2, "GetActiveSystems");
    lua_pushcfunction(L, lua_World_SetActiveSystems);
    lua_setfield(L, -2, "SetActiveSystems");

    // Entity lifecycle
    lua_pushcfunction(L, lua_World_CreateEntity);
    lua_setfield(L, -2, "CreateEntity");
    lua_pushcfunction(L, lua_World_DestroyEntity);
    lua_setfield(L, -2, "DestroyEntity");
    lua_pushcfunction(L, lua_World_SetParent);
    lua_setfield(L, -2, "SetParent");
    lua_pushcfunction(L, lua_World_ClearParent);
    lua_setfield(L, -2, "ClearParent");
    lua_pushcfunction(L, lua_World_GetParent);
    lua_setfield(L, -2, "GetParent");
    lua_pushcfunction(L, lua_World_GetChildren);
    lua_setfield(L, -2, "GetChildren");
    lua_pushcfunction(L, lua_World_GetAncestors);
    lua_setfield(L, -2, "GetAncestors");
    lua_pushcfunction(L, lua_World_GetDescendants);
    lua_setfield(L, -2, "GetDescendants");
    lua_pushcfunction(L, lua_World_AddTag);
    lua_setfield(L, -2, "AddTag");
    lua_pushcfunction(L, lua_World_RemoveTag);
    lua_setfield(L, -2, "RemoveTag");
    lua_pushcfunction(L, lua_World_HasTag);
    lua_setfield(L, -2, "HasTag");
    lua_pushcfunction(L, lua_World_GetTags);
    lua_setfield(L, -2, "GetTags");
    lua_pushcfunction(L, lua_World_GetEntitiesWithTag);
    lua_setfield(L, -2, "GetEntitiesWithTag");
    lua_pushcfunction(L, lua_World_GetEntitiesWithTags);
    lua_setfield(L, -2, "GetEntitiesWithTags");
    lua_pushcfunction(L, lua_World_FindEntityByName);
    lua_setfield(L, -2, "FindEntityByName");
    lua_pushcfunction(L, lua_World_FindEntitiesByName);
    lua_setfield(L, -2, "FindEntitiesByName");
    lua_pushcfunction(L, lua_World_FindEntitiesByNameContains);
    lua_setfield(L, -2, "FindEntitiesByNameContains");
    lua_pushcfunction(L, lua_World_FindEntitiesByFlags);
    lua_setfield(L, -2, "FindEntitiesByFlags");
    lua_pushcfunction(L, lua_World_GetEntities);
    lua_setfield(L, -2, "GetEntities");
    lua_pushcfunction(L, lua_World_GetEntitiesWithComponent);
    lua_setfield(L, -2, "GetEntitiesWithComponent");
    lua_pushcfunction(L, lua_World_GetEntitiesWithComponents);
    lua_setfield(L, -2, "GetEntitiesWithComponents");
    lua_pushcfunction(L, lua_World_GetComponentNames);
    lua_setfield(L, -2, "GetComponentNames");
    lua_pushcfunction(L, lua_World_GetTransformsBatch);
    lua_setfield(L, -2, "GetTransformsBatch");
    lua_pushcfunction(L, lua_World_SetTransformsBatch);
    lua_setfield(L, -2, "SetTransformsBatch");
    lua_pushcfunction(L, lua_World_GetCamerasBatch);
    lua_setfield(L, -2, "GetCamerasBatch");
    lua_pushcfunction(L, lua_World_SetCamerasBatch);
    lua_setfield(L, -2, "SetCamerasBatch");
    lua_pushcfunction(L, lua_World_GetLightsBatch);
    lua_setfield(L, -2, "GetLightsBatch");
    lua_pushcfunction(L, lua_World_SetLightsBatch);
    lua_setfield(L, -2, "SetLightsBatch");
    lua_pushcfunction(L, lua_World_GetRenderablesBatch);
    lua_setfield(L, -2, "GetRenderablesBatch");
    lua_pushcfunction(L, lua_World_SetRenderablesBatch);
    lua_setfield(L, -2, "SetRenderablesBatch");
    lua_pushcfunction(L, lua_World_GetMeshRenderersBatch);
    lua_setfield(L, -2, "GetMeshRenderersBatch");
    lua_pushcfunction(L, lua_World_SetMeshRenderersBatch);
    lua_setfield(L, -2, "SetMeshRenderersBatch");
    lua_pushcfunction(L, lua_World_GetVelocitiesBatch);
    lua_setfield(L, -2, "GetVelocitiesBatch");
    lua_pushcfunction(L, lua_World_SetVelocitiesBatch);
    lua_setfield(L, -2, "SetVelocitiesBatch");

    // Scheduler controls
    lua_pushcfunction(L, lua_World_SetPaused);
    lua_setfield(L, -2, "SetPaused");
    lua_pushcfunction(L, lua_World_StepOnce);
    lua_setfield(L, -2, "StepOnce");
    lua_pushcfunction(L, lua_World_SetFixedTimestep);
    lua_setfield(L, -2, "SetFixedTimestep");
    lua_pushcfunction(L, lua_World_GetFixedTimestep);
    lua_setfield(L, -2, "GetFixedTimestep");
    lua_pushcfunction(L, lua_World_SetMaxSubsteps);
    lua_setfield(L, -2, "SetMaxSubsteps");
    lua_pushcfunction(L, lua_World_GetMaxSubsteps);
    lua_setfield(L, -2, "GetMaxSubsteps");
    lua_pushcfunction(L, lua_World_SetTimeScale);
    lua_setfield(L, -2, "SetTimeScale");
    lua_pushcfunction(L, lua_World_GetTimeScale);
    lua_setfield(L, -2, "GetTimeScale");
    lua_pushcfunction(L, lua_World_SetTimeMode);
    lua_setfield(L, -2, "SetTimeMode");
    lua_pushcfunction(L, lua_World_GetTimeMode);
    lua_setfield(L, -2, "GetTimeMode");
    lua_pushcfunction(L, lua_World_SetReplayDelta);
    lua_setfield(L, -2, "SetReplayDelta");
    lua_pushcfunction(L, lua_World_GetComponent);
    lua_setfield(L, -2, "GetComponent");
    lua_pushcfunction(L, lua_World_SetComponent);
    lua_setfield(L, -2, "SetComponent");
    lua_pushcfunction(L, lua_World_GetComponents);
    lua_setfield(L, -2, "GetComponents");
    lua_pushcfunction(L, lua_World_SetComponents);
    lua_setfield(L, -2, "SetComponents");
    lua_pushcfunction(L, lua_World_GetMetadata);
    lua_setfield(L, -2, "GetMetadata");
    lua_pushcfunction(L, lua_World_GetEntityName);
    lua_setfield(L, -2, "GetEntityName");
    lua_pushcfunction(L, lua_World_SetEntityName);
    lua_setfield(L, -2, "SetEntityName");
    lua_pushcfunction(L, lua_World_SetMetadata);
    lua_setfield(L, -2, "SetMetadata");
    lua_pushcfunction(L, lua_World_ClearMetadata);
    lua_setfield(L, -2, "ClearMetadata");

    // Physics (velocity)
    lua_pushcfunction(L, lua_World_GetVelocity);
    lua_setfield(L, -2, "GetVelocity");
    lua_pushcfunction(L, lua_World_SetVelocity);
    lua_setfield(L, -2, "SetVelocity");
    lua_pushcfunction(L, lua_World_GetAngularVelocity);
    lua_setfield(L, -2, "GetAngularVelocity");
    lua_pushcfunction(L, lua_World_SetAngularVelocity);
    lua_setfield(L, -2, "SetAngularVelocity");

    // Animation (parameters)
    lua_pushcfunction(L, lua_World_SetAnimFloat);
    lua_setfield(L, -2, "SetAnimFloat");
    lua_pushcfunction(L, lua_World_SetAnimBool);
    lua_setfield(L, -2, "SetAnimBool");
    lua_pushcfunction(L, lua_World_SetAnimTrigger);
    lua_setfield(L, -2, "SetAnimTrigger");

    lua_setglobal(L, "World");
}

bool BindScriptInstance(lua_State* L,
                        const std::string& scriptName,
                        const ::eyelib::world::EntityId& entity,
                        const ::eyelib::world::components::Script& component) {
    if (!L || scriptName.empty()) {
        return false;
    }
    lua_getglobal(L, scriptName.c_str());
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return false;
    }

    PushEntity(L, entity);
    lua_setfield(L, -2, "entity");

    lua_newtable(L);
    for (const auto& [key, value] : component.properties) {
        PushScriptValue(L, value);
        lua_setfield(L, -2, key.c_str());
    }
    lua_setfield(L, -2, "properties");

    lua_pop(L, 1);
    return true;
}

} // namespace

namespace scripting {

void RegisterWorldBindings(lua_State* L,
                           eyelib::api::world::World* world,
                           WorldPhysicsBridge* physics,
                           WorldAnimationBridge* animation) {
    ::openeye::player::RegisterWorldBindings(L, world, physics, animation);
}

bool BindScriptInstance(lua_State* L,
                        const std::string& scriptName,
                        const ::eyelib::world::EntityId& entity,
                        const ::eyelib::world::components::Script& component) {
    return ::openeye::player::BindScriptInstance(L, scriptName, entity, component);
}

bool ReadProperties(lua_State* L,
                    const std::string& scriptName,
                    std::unordered_map<std::string, ::eyelib::world::components::ScriptValue>& out) {
    return ::openeye::player::ReadProperties(L, scriptName, out);
}

bool PropertiesEqual(const std::unordered_map<std::string, ::eyelib::world::components::ScriptValue>& a,
                     const std::unordered_map<std::string, ::eyelib::world::components::ScriptValue>& b) {
    return ::openeye::player::PropertiesEqual(a, b);
}

} // namespace scripting
} // namespace openeye::player
