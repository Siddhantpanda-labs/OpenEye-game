#include "scripting/WorldLuaHelpers.h"

#include <variant>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

namespace openeye::player {

static bool ReadVec3Table(lua_State* L, int index, ::eyelib::math::Vec3& out);

void PushVec3(lua_State* L, const ::eyelib::math::Vec3& v) {
    lua_newtable(L);
    lua_pushnumber(L, v.x);
    lua_setfield(L, -2, "x");
    lua_pushnumber(L, v.y);
    lua_setfield(L, -2, "y");
    lua_pushnumber(L, v.z);
    lua_setfield(L, -2, "z");
}

bool ReadVec3(lua_State* L, int index, ::eyelib::math::Vec3& out) {
    const int absIndex = lua_absindex(L, index);
    if (!lua_istable(L, absIndex)) {
        return false;
    }
    lua_getfield(L, absIndex, "x");
    lua_getfield(L, absIndex, "y");
    lua_getfield(L, absIndex, "z");
    if (lua_isnumber(L, -3) && lua_isnumber(L, -2) && lua_isnumber(L, -1)) {
        out.x = static_cast<float>(lua_tonumber(L, -3));
        out.y = static_cast<float>(lua_tonumber(L, -2));
        out.z = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 3);
        return true;
    }
    lua_pop(L, 3);
    return false;
}

void PushQuat(lua_State* L, const ::eyelib::math::Quat& q) {
    lua_newtable(L);
    lua_pushnumber(L, q.w);
    lua_setfield(L, -2, "w");
    lua_pushnumber(L, q.x);
    lua_setfield(L, -2, "x");
    lua_pushnumber(L, q.y);
    lua_setfield(L, -2, "y");
    lua_pushnumber(L, q.z);
    lua_setfield(L, -2, "z");
}

bool ReadQuat(lua_State* L, int index, ::eyelib::math::Quat& out) {
    const int absIndex = lua_absindex(L, index);
    if (!lua_istable(L, absIndex)) {
        return false;
    }
    lua_getfield(L, absIndex, "w");
    lua_getfield(L, absIndex, "x");
    lua_getfield(L, absIndex, "y");
    lua_getfield(L, absIndex, "z");
    if (lua_isnumber(L, -4) && lua_isnumber(L, -3) && lua_isnumber(L, -2) && lua_isnumber(L, -1)) {
        out.w = static_cast<float>(lua_tonumber(L, -4));
        out.x = static_cast<float>(lua_tonumber(L, -3));
        out.y = static_cast<float>(lua_tonumber(L, -2));
        out.z = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 4);
        return true;
    }
    lua_pop(L, 4);
    return false;
}

void PushEntity(lua_State* L, const ::eyelib::world::EntityId& entity) {
    lua_newtable(L);
    lua_pushinteger(L, static_cast<lua_Integer>(entity.index));
    lua_setfield(L, -2, "index");
    lua_pushinteger(L, static_cast<lua_Integer>(entity.generation));
    lua_setfield(L, -2, "generation");
}

void PushEntityArray(lua_State* L, const std::vector<::eyelib::world::EntityId>& entities) {
    lua_newtable(L);
    int idx = 1;
    for (const auto& entity : entities) {
        PushEntity(L, entity);
        lua_rawseti(L, -2, idx++);
    }
}

bool ReadEntityArray(lua_State* L, int index, std::vector<::eyelib::world::EntityId>& out) {
    if (!lua_istable(L, index)) {
        return false;
    }
    const int absIndex = lua_absindex(L, index);
    const size_t len = lua_rawlen(L, absIndex);
    out.clear();
    out.reserve(len);
    for (size_t i = 1; i <= len; ++i) {
        lua_rawgeti(L, absIndex, static_cast<lua_Integer>(i));
        if (lua_istable(L, -1)) {
            ::eyelib::world::EntityId entity = ::eyelib::world::InvalidEntity;
            if (ReadEntityTable(L, lua_gettop(L), entity)) {
                out.push_back(entity);
            }
        }
        lua_pop(L, 1);
    }
    return true;
}

::eyelib::world::EntityId ReadEntity(lua_State* L, int index) {
    ::eyelib::world::EntityId entity = ::eyelib::world::InvalidEntity;
    const int absIndex = lua_absindex(L, index);
    if (!lua_istable(L, absIndex)) {
        return entity;
    }
    lua_getfield(L, absIndex, "index");
    lua_getfield(L, absIndex, "generation");
    if (lua_isnumber(L, -2) && lua_isnumber(L, -1)) {
        entity.index = static_cast<uint32_t>(lua_tointeger(L, -2));
        entity.generation = static_cast<uint32_t>(lua_tointeger(L, -1));
    }
    lua_pop(L, 2);
    return entity;
}

bool ReadEntityTable(lua_State* L, int index, ::eyelib::world::EntityId& out) {
    const int absIndex = lua_absindex(L, index);
    if (!lua_istable(L, absIndex)) {
        return false;
    }
    lua_getfield(L, absIndex, "index");
    lua_getfield(L, absIndex, "generation");
    if (lua_isnumber(L, -2) && lua_isnumber(L, -1)) {
        out.index = static_cast<uint32_t>(lua_tointeger(L, -2));
        out.generation = static_cast<uint32_t>(lua_tointeger(L, -1));
        lua_pop(L, 2);
        return true;
    }
    lua_pop(L, 2);
    return false;
}

void PushScriptValue(lua_State* L, const ::eyelib::world::components::ScriptValue& value) {
    using Type = ::eyelib::world::components::ScriptValue::Type;
    switch (value.type) {
        case Type::Bool:
            lua_pushboolean(L, std::get<bool>(value.value) ? 1 : 0);
            break;
        case Type::Number:
            lua_pushnumber(L, std::get<double>(value.value));
            break;
        case Type::String:
            lua_pushstring(L, std::get<std::string>(value.value).c_str());
            break;
        case Type::Vec3:
            PushVec3(L, std::get<::eyelib::math::Vec3>(value.value));
            break;
        case Type::Entity:
            if (std::holds_alternative<::eyelib::world::EntityId>(value.value))
                PushEntity(L, std::get<::eyelib::world::EntityId>(value.value));
            else
                lua_pushnil(L);
            break;
        case Type::Nil:
        default:
            lua_pushnil(L);
            break;
    }
}

bool ReadScriptValue(lua_State* L, int index, ::eyelib::world::components::ScriptValue& out) {
    if (lua_isnil(L, index)) {
        out = {};
        return true;
    }
    if (lua_isboolean(L, index)) {
        out = ::eyelib::world::components::ScriptValue(lua_toboolean(L, index) != 0);
        return true;
    }
    if (lua_isnumber(L, index)) {
        out = ::eyelib::world::components::ScriptValue(static_cast<double>(lua_tonumber(L, index)));
        return true;
    }
    if (lua_isstring(L, index)) {
        out = ::eyelib::world::components::ScriptValue(std::string(lua_tostring(L, index)));
        return true;
    }
    if (lua_istable(L, index)) {
        ::eyelib::world::EntityId entity = ::eyelib::world::InvalidEntity;
        if (ReadEntityTable(L, index, entity)) {
            out = ::eyelib::world::components::ScriptValue(entity);
            return true;
        }
        ::eyelib::math::Vec3 vec(0.0f);
        if (ReadVec3Table(L, index, vec)) {
            out = ::eyelib::world::components::ScriptValue(vec);
            return true;
        }
    }
    return false;
}

bool ReadBoolField(lua_State* L, int index, const char* field, bool& out) {
    const int absIndex = lua_absindex(L, index);
    lua_getfield(L, absIndex, field);
    if (!lua_isnil(L, -1)) {
        out = lua_toboolean(L, -1) != 0;
        lua_pop(L, 1);
        return true;
    }
    lua_pop(L, 1);
    return false;
}

bool ReadNumberField(lua_State* L, int index, const char* field, float& out) {
    const int absIndex = lua_absindex(L, index);
    lua_getfield(L, absIndex, field);
    if (lua_isnumber(L, -1)) {
        out = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);
        return true;
    }
    lua_pop(L, 1);
    return false;
}

bool ReadStringField(lua_State* L, int index, const char* field, std::string& out) {
    const int absIndex = lua_absindex(L, index);
    lua_getfield(L, absIndex, field);
    if (lua_isstring(L, -1)) {
        out = lua_tostring(L, -1);
        lua_pop(L, 1);
        return true;
    }
    lua_pop(L, 1);
    return false;
}

bool ReadIntField(lua_State* L, int index, const char* field, int& out) {
    const int absIndex = lua_absindex(L, index);
    lua_getfield(L, absIndex, field);
    if (lua_isnumber(L, -1)) {
        out = static_cast<int>(lua_tointeger(L, -1));
        lua_pop(L, 1);
        return true;
    }
    lua_pop(L, 1);
    return false;
}

bool ReadVec3Field(lua_State* L, int index, const char* field, ::eyelib::math::Vec3& out) {
    const int absIndex = lua_absindex(L, index);
    lua_getfield(L, absIndex, field);
    if (lua_istable(L, -1)) {
        const bool ok = ReadVec3Table(L, lua_gettop(L), out);
        lua_pop(L, 1);
        return ok;
    }
    lua_pop(L, 1);
    return false;
}

void PushStringArray(lua_State* L, const std::vector<std::string>& values) {
    lua_newtable(L);
    int idx = 1;
    for (const auto& value : values) {
        lua_pushstring(L, value.c_str());
        lua_rawseti(L, -2, idx++);
    }
}

bool ReadStringArray(lua_State* L, int index, std::vector<std::string>& out) {
    const int absIndex = lua_absindex(L, index);
    if (!lua_istable(L, absIndex)) {
        return false;
    }
    out.clear();
    const size_t len = lua_rawlen(L, absIndex);
    out.reserve(len);
    for (size_t i = 1; i <= len; ++i) {
        lua_rawgeti(L, absIndex, static_cast<lua_Integer>(i));
        if (lua_isstring(L, -1)) {
            out.emplace_back(lua_tostring(L, -1));
        }
        lua_pop(L, 1);
    }
    return true;
}

static bool ReadVec3Table(lua_State* L, int index, ::eyelib::math::Vec3& out) {
    const int absIndex = lua_absindex(L, index);
    if (!lua_istable(L, absIndex)) {
        return false;
    }
    lua_getfield(L, absIndex, "x");
    lua_getfield(L, absIndex, "y");
    lua_getfield(L, absIndex, "z");
    if (lua_isnumber(L, -3) && lua_isnumber(L, -2) && lua_isnumber(L, -1)) {
        out.x = static_cast<float>(lua_tonumber(L, -3));
        out.y = static_cast<float>(lua_tonumber(L, -2));
        out.z = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 3);
        return true;
    }
    lua_pop(L, 3);
    return false;
}

void PushCamera(lua_State* L, const ::eyelib::world::components::Camera& camera) {
    lua_newtable(L);
    lua_pushboolean(L, camera.isPrimary ? 1 : 0);
    lua_setfield(L, -2, "isPrimary");
    lua_pushboolean(L, camera.isPerspective ? 1 : 0);
    lua_setfield(L, -2, "isPerspective");
    lua_pushnumber(L, camera.fov);
    lua_setfield(L, -2, "fov");
    lua_pushnumber(L, camera.aspect);
    lua_setfield(L, -2, "aspect");
    lua_pushnumber(L, camera.nearPlane);
    lua_setfield(L, -2, "nearPlane");
    lua_pushnumber(L, camera.farPlane);
    lua_setfield(L, -2, "farPlane");
    lua_pushnumber(L, camera.orthoSize);
    lua_setfield(L, -2, "orthoSize");
}

bool ReadCamera(lua_State* L, int index, ::eyelib::world::components::Camera& out) {
    if (!lua_istable(L, index)) {
        return false;
    }
    ReadBoolField(L, index, "isPrimary", out.isPrimary);
    ReadBoolField(L, index, "isPerspective", out.isPerspective);
    ReadNumberField(L, index, "fov", out.fov);
    ReadNumberField(L, index, "aspect", out.aspect);
    ReadNumberField(L, index, "nearPlane", out.nearPlane);
    ReadNumberField(L, index, "farPlane", out.farPlane);
    ReadNumberField(L, index, "orthoSize", out.orthoSize);
    return true;
}

void PushLight(lua_State* L, const ::eyelib::world::components::Light& light) {
    lua_newtable(L);
    lua_pushinteger(L, static_cast<int>(light.type));
    lua_setfield(L, -2, "type");
    PushVec3(L, light.color);
    lua_setfield(L, -2, "color");
    lua_pushnumber(L, light.intensity);
    lua_setfield(L, -2, "intensity");
    lua_pushnumber(L, light.range);
    lua_setfield(L, -2, "range");
    lua_pushnumber(L, light.innerConeAngle);
    lua_setfield(L, -2, "innerConeAngle");
    lua_pushnumber(L, light.outerConeAngle);
    lua_setfield(L, -2, "outerConeAngle");
    lua_pushboolean(L, light.castShadows ? 1 : 0);
    lua_setfield(L, -2, "castShadows");
}

bool ReadLight(lua_State* L, int index, ::eyelib::world::components::Light& out) {
    if (!lua_istable(L, index)) {
        return false;
    }
    int typeValue = static_cast<int>(out.type);
    if (ReadIntField(L, index, "type", typeValue)) {
        if (typeValue < 0) {
            typeValue = 0;
        } else if (typeValue > 2) {
            typeValue = 2;
        }
        out.type = static_cast<::eyelib::world::components::Light::Type>(typeValue);
    }
    ReadVec3Field(L, index, "color", out.color);
    ReadNumberField(L, index, "intensity", out.intensity);
    ReadNumberField(L, index, "range", out.range);
    ReadNumberField(L, index, "innerConeAngle", out.innerConeAngle);
    ReadNumberField(L, index, "outerConeAngle", out.outerConeAngle);
    ReadBoolField(L, index, "castShadows", out.castShadows);
    return true;
}

void PushRenderable(lua_State* L, const ::eyelib::world::components::Renderable& renderable) {
    lua_newtable(L);
    lua_pushstring(L, renderable.meshAssetId.c_str());
    lua_setfield(L, -2, "meshAssetId");
    lua_pushstring(L, renderable.materialAssetId.c_str());
    lua_setfield(L, -2, "materialAssetId");
    lua_pushboolean(L, renderable.visible ? 1 : 0);
    lua_setfield(L, -2, "visible");
    PushStringArray(L, renderable.lodMeshAssetIds);
    lua_setfield(L, -2, "lodMeshAssetIds");
    lua_pushinteger(L, static_cast<lua_Integer>(renderable.activeLOD));
    lua_setfield(L, -2, "activeLOD");
}

bool ReadRenderable(lua_State* L, int index, ::eyelib::world::components::Renderable& out) {
    if (!lua_istable(L, index)) {
        return false;
    }
    ReadStringField(L, index, "meshAssetId", out.meshAssetId);
    ReadStringField(L, index, "materialAssetId", out.materialAssetId);
    ReadBoolField(L, index, "visible", out.visible);
    lua_getfield(L, index, "lodMeshAssetIds");
    if (lua_istable(L, -1)) {
        ReadStringArray(L, lua_gettop(L), out.lodMeshAssetIds);
    }
    lua_pop(L, 1);
    int lod = static_cast<int>(out.activeLOD);
    if (ReadIntField(L, index, "activeLOD", lod) && lod >= 0) {
        out.activeLOD = static_cast<size_t>(lod);
    }
    return true;
}

void PushMeshRenderer(lua_State* L, const ::eyelib::world::components::MeshRenderer& renderer) {
    lua_newtable(L);
    lua_pushstring(L, renderer.meshAssetId.c_str());
    lua_setfield(L, -2, "meshAssetId");
    lua_pushstring(L, renderer.materialAssetId.c_str());
    lua_setfield(L, -2, "materialAssetId");
    lua_pushboolean(L, renderer.visible ? 1 : 0);
    lua_setfield(L, -2, "visible");
}

bool ReadMeshRenderer(lua_State* L, int index, ::eyelib::world::components::MeshRenderer& out) {
    if (!lua_istable(L, index)) {
        return false;
    }
    ReadStringField(L, index, "meshAssetId", out.meshAssetId);
    ReadStringField(L, index, "materialAssetId", out.materialAssetId);
    ReadBoolField(L, index, "visible", out.visible);
    return true;
}

void PushTransform(lua_State* L, const ::eyelib::world::components::Transform& transform) {
    lua_newtable(L);
    PushVec3(L, transform.position);
    lua_setfield(L, -2, "position");
    PushQuat(L, transform.rotation);
    lua_setfield(L, -2, "rotation");
    PushVec3(L, transform.scale);
    lua_setfield(L, -2, "scale");
}

bool ReadTransform(lua_State* L, int index, ::eyelib::world::components::Transform& out) {
    if (!lua_istable(L, index)) {
        return false;
    }
    ReadVec3Field(L, index, "position", out.position);
    lua_getfield(L, index, "rotation");
    if (lua_istable(L, -1)) {
        ReadQuat(L, lua_gettop(L), out.rotation);
    }
    lua_pop(L, 1);
    ReadVec3Field(L, index, "scale", out.scale);
    return true;
}

void PushCollider(lua_State* L, const ::eyelib::world::components::Collider& collider) {
    lua_newtable(L);
    lua_pushinteger(L, static_cast<int>(collider.type));
    lua_setfield(L, -2, "type");
    PushVec3(L, collider.size);
    lua_setfield(L, -2, "size");
    lua_pushnumber(L, collider.radius);
    lua_setfield(L, -2, "radius");
    lua_pushnumber(L, collider.height);
    lua_setfield(L, -2, "height");
}

bool ReadCollider(lua_State* L, int index, ::eyelib::world::components::Collider& out) {
    if (!lua_istable(L, index)) {
        return false;
    }
    int typeValue = static_cast<int>(out.type);
    if (ReadIntField(L, index, "type", typeValue)) {
        if (typeValue < 0) {
            typeValue = 0;
        } else if (typeValue > 2) {
            typeValue = 2;
        }
        out.type = static_cast<::eyelib::world::components::Collider::Type>(typeValue);
    }
    ReadVec3Field(L, index, "size", out.size);
    ReadNumberField(L, index, "radius", out.radius);
    ReadNumberField(L, index, "height", out.height);
    return true;
}

void PushRigidBody(lua_State* L, const ::eyelib::world::components::RigidBody& rigidBody) {
    lua_newtable(L);
    lua_pushnumber(L, static_cast<lua_Number>(rigidBody.mass));
    lua_setfield(L, -2, "mass");
    lua_pushboolean(L, rigidBody.useGravity ? 1 : 0);
    lua_setfield(L, -2, "useGravity");
    lua_pushboolean(L, rigidBody.isKinematic ? 1 : 0);
    lua_setfield(L, -2, "isKinematic");
    lua_pushboolean(L, rigidBody.syncTransform ? 1 : 0);
    lua_setfield(L, -2, "syncTransform");
}

bool ReadRigidBody(lua_State* L, int index, ::eyelib::world::components::RigidBody& out) {
    if (!lua_istable(L, index)) {
        return false;
    }
    ReadBoolField(L, index, "syncTransform", out.syncTransform);
    return true;
}

void PushAnimator(lua_State* L, const ::eyelib::world::components::Animator& animator) {
    lua_newtable(L);
    lua_pushstring(L, animator.animatorAssetId.c_str());
    lua_setfield(L, -2, "animatorAssetId");
    lua_pushstring(L, animator.skeletonAssetId.c_str());
    lua_setfield(L, -2, "skeletonAssetId");
}

bool ReadAnimator(lua_State* L, int index, ::eyelib::world::components::Animator& out) {
    if (!lua_istable(L, index)) {
        return false;
    }
    ReadStringField(L, index, "animatorAssetId", out.animatorAssetId);
    ReadStringField(L, index, "skeletonAssetId", out.skeletonAssetId);
    return true;
}

void PushName(lua_State* L, const ::eyelib::world::components::Name& name) {
    lua_newtable(L);
    lua_pushstring(L, name.value.c_str());
    lua_setfield(L, -2, "value");
}

bool ReadName(lua_State* L, int index, ::eyelib::world::components::Name& out) {
    if (!lua_istable(L, index)) {
        return false;
    }
    ReadStringField(L, index, "value", out.value);
    return true;
}

void PushRegion(lua_State* L, const ::eyelib::world::components::Region& region) {
    lua_newtable(L);
    lua_pushinteger(L, static_cast<int>(region.type));
    lua_setfield(L, -2, "type");
    PushVec3(L, region.center);
    lua_setfield(L, -2, "center");
    PushVec3(L, region.size);
    lua_setfield(L, -2, "size");
    lua_pushnumber(L, region.radius);
    lua_setfield(L, -2, "radius");
    lua_pushinteger(L, region.priority);
    lua_setfield(L, -2, "priority");
    lua_pushboolean(L, region.enabled ? 1 : 0);
    lua_setfield(L, -2, "enabled");
    PushStringArray(L, region.ruleIds);
    lua_setfield(L, -2, "ruleIds");
}

bool ReadRegion(lua_State* L, int index, ::eyelib::world::components::Region& out) {
    if (!lua_istable(L, index)) {
        return false;
    }
    int typeValue = static_cast<int>(out.type);
    if (ReadIntField(L, index, "type", typeValue)) {
        if (typeValue < 0) {
            typeValue = 0;
        } else if (typeValue > 1) {
            typeValue = 1;
        }
        out.type = static_cast<::eyelib::world::components::Region::Type>(typeValue);
    }
    ReadVec3Field(L, index, "center", out.center);
    ReadVec3Field(L, index, "size", out.size);
    ReadNumberField(L, index, "radius", out.radius);
    ReadIntField(L, index, "priority", out.priority);
    ReadBoolField(L, index, "enabled", out.enabled);
    lua_getfield(L, index, "ruleIds");
    if (lua_istable(L, -1)) {
        ReadStringArray(L, lua_gettop(L), out.ruleIds);
    }
    lua_pop(L, 1);
    return true;
}

void PushRegionMembership(lua_State* L, const ::eyelib::world::components::RegionMembership& membership) {
    lua_newtable(L);
    PushEntity(L, membership.region);
    lua_setfield(L, -2, "region");
}

bool ReadRegionMembership(lua_State* L, int index, ::eyelib::world::components::RegionMembership& out) {
    if (!lua_istable(L, index)) {
        return false;
    }
    lua_getfield(L, index, "region");
    if (lua_istable(L, -1)) {
        ::eyelib::world::EntityId region = ::eyelib::world::InvalidEntity;
        if (ReadEntityTable(L, lua_gettop(L), region)) {
            out.region = region;
        }
    }
    lua_pop(L, 1);
    return true;
}

void PushAABB(lua_State* L, const ::eyelib::world::components::AABB& aabb) {
    lua_newtable(L);
    PushVec3(L, aabb.min);
    lua_setfield(L, -2, "min");
    PushVec3(L, aabb.max);
    lua_setfield(L, -2, "max");
    lua_pushboolean(L, aabb.isVisible ? 1 : 0);
    lua_setfield(L, -2, "isVisible");
}

bool ReadAABB(lua_State* L, int index, ::eyelib::world::components::AABB& out) {
    if (!lua_istable(L, index)) {
        return false;
    }
    ReadVec3Field(L, index, "min", out.min);
    ReadVec3Field(L, index, "max", out.max);
    ReadBoolField(L, index, "isVisible", out.isVisible);
    return true;
}

void PushSystemBindings(lua_State* L, const ::eyelib::world::components::SystemBindings& bindings) {
    lua_newtable(L);
    PushStringArray(L, bindings.enabledSystems);
    lua_setfield(L, -2, "enabledSystems");
}

bool ReadSystemBindings(lua_State* L, int index, ::eyelib::world::components::SystemBindings& out) {
    if (!lua_istable(L, index)) {
        return false;
    }
    lua_getfield(L, index, "enabledSystems");
    if (lua_istable(L, -1)) {
        ReadStringArray(L, lua_gettop(L), out.enabledSystems);
    }
    lua_pop(L, 1);
    return true;
}

void PushActiveSystems(lua_State* L, const ::eyelib::world::components::ActiveSystems& systems) {
    lua_newtable(L);
    PushStringArray(L, systems.enabledSystems);
    lua_setfield(L, -2, "enabledSystems");
}

bool ReadActiveSystems(lua_State* L, int index, ::eyelib::world::components::ActiveSystems& out) {
    if (!lua_istable(L, index)) {
        return false;
    }
    lua_getfield(L, index, "enabledSystems");
    if (lua_istable(L, -1)) {
        ReadStringArray(L, lua_gettop(L), out.enabledSystems);
    }
    lua_pop(L, 1);
    return true;
}

void PushMetadata(lua_State* L, const std::string& name, uint32_t flags) {
    lua_newtable(L);
    lua_pushstring(L, name.c_str());
    lua_setfield(L, -2, "name");
    lua_pushinteger(L, static_cast<lua_Integer>(flags));
    lua_setfield(L, -2, "flags");
}

bool ReadMetadata(lua_State* L, int index, std::string& name, uint32_t& flags) {
    const int absIndex = lua_absindex(L, index);
    if (!lua_istable(L, absIndex)) {
        return false;
    }
    ReadStringField(L, absIndex, "name", name);
    int flagsValue = static_cast<int>(flags);
    if (ReadIntField(L, absIndex, "flags", flagsValue) && flagsValue >= 0) {
        flags = static_cast<uint32_t>(flagsValue);
    }
    return true;
}

bool ReadPropertiesTable(lua_State* L, int index,
                         std::unordered_map<std::string, ::eyelib::world::components::ScriptValue>& out) {
    const int absIndex = lua_absindex(L, index);
    if (!lua_istable(L, absIndex)) {
        return false;
    }
    out.clear();
    lua_pushnil(L);
    while (lua_next(L, absIndex) != 0) {
        if (lua_isstring(L, -2)) {
            const char* key = lua_tostring(L, -2);
            if (key) {
                ::eyelib::world::components::ScriptValue value;
                if (ReadScriptValue(L, -1, value)) {
                    out.emplace(key, value);
                }
            }
        }
        lua_pop(L, 1);
    }
    return true;
}

bool ReadProperties(lua_State* L,
                    const std::string& scriptName,
                    std::unordered_map<std::string, ::eyelib::world::components::ScriptValue>& out) {
    out.clear();
    if (!L || scriptName.empty()) {
        return false;
    }

    lua_getglobal(L, scriptName.c_str());
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return false;
    }

    lua_getfield(L, -1, "properties");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 2);
        return false;
    }

    lua_pushnil(L);
    while (lua_next(L, -2) != 0) {
        if (lua_isstring(L, -2)) {
            const char* key = lua_tostring(L, -2);
            if (key) {
                ::eyelib::world::components::ScriptValue value;
                if (ReadScriptValue(L, -1, value)) {
                    out.emplace(key, value);
                }
            }
        }
        lua_pop(L, 1);
    }

    lua_pop(L, 2);
    return true;
}

static bool ScriptValuesEqual(const ::eyelib::world::components::ScriptValue& a,
                              const ::eyelib::world::components::ScriptValue& b) {
    if (a.type != b.type) {
        return false;
    }
    return a.value == b.value;
}

bool PropertiesEqual(const std::unordered_map<std::string, ::eyelib::world::components::ScriptValue>& a,
                     const std::unordered_map<std::string, ::eyelib::world::components::ScriptValue>& b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (const auto& [key, value] : a) {
        auto it = b.find(key);
        if (it == b.end()) {
            return false;
        }
        if (!ScriptValuesEqual(value, it->second)) {
            return false;
        }
    }
    return true;
}

} // namespace openeye::player
