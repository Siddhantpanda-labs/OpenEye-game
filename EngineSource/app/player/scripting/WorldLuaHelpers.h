#pragma once

#include "api/world/All.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

struct lua_State;

namespace openeye::player {

void PushVec3(lua_State* L, const ::eyelib::math::Vec3& v);
bool ReadVec3(lua_State* L, int index, ::eyelib::math::Vec3& out);

void PushQuat(lua_State* L, const ::eyelib::math::Quat& q);
bool ReadQuat(lua_State* L, int index, ::eyelib::math::Quat& out);

void PushEntity(lua_State* L, const ::eyelib::world::EntityId& entity);
void PushEntityArray(lua_State* L, const std::vector<::eyelib::world::EntityId>& entities);
bool ReadEntityArray(lua_State* L, int index, std::vector<::eyelib::world::EntityId>& out);
::eyelib::world::EntityId ReadEntity(lua_State* L, int index);
bool ReadEntityTable(lua_State* L, int index, ::eyelib::world::EntityId& out);

void PushScriptValue(lua_State* L, const ::eyelib::world::components::ScriptValue& value);
bool ReadScriptValue(lua_State* L, int index, ::eyelib::world::components::ScriptValue& out);

bool ReadBoolField(lua_State* L, int index, const char* field, bool& out);
bool ReadNumberField(lua_State* L, int index, const char* field, float& out);
bool ReadIntField(lua_State* L, int index, const char* field, int& out);
bool ReadStringField(lua_State* L, int index, const char* field, std::string& out);
bool ReadVec3Field(lua_State* L, int index, const char* field, ::eyelib::math::Vec3& out);

void PushStringArray(lua_State* L, const std::vector<std::string>& values);
bool ReadStringArray(lua_State* L, int index, std::vector<std::string>& out);

void PushCamera(lua_State* L, const ::eyelib::world::components::Camera& camera);
bool ReadCamera(lua_State* L, int index, ::eyelib::world::components::Camera& out);

void PushLight(lua_State* L, const ::eyelib::world::components::Light& light);
bool ReadLight(lua_State* L, int index, ::eyelib::world::components::Light& out);

void PushRenderable(lua_State* L, const ::eyelib::world::components::Renderable& renderable);
bool ReadRenderable(lua_State* L, int index, ::eyelib::world::components::Renderable& out);

void PushMeshRenderer(lua_State* L, const ::eyelib::world::components::MeshRenderer& renderer);
bool ReadMeshRenderer(lua_State* L, int index, ::eyelib::world::components::MeshRenderer& out);

void PushTransform(lua_State* L, const ::eyelib::world::components::Transform& transform);
bool ReadTransform(lua_State* L, int index, ::eyelib::world::components::Transform& out);

void PushCollider(lua_State* L, const ::eyelib::world::components::Collider& collider);
bool ReadCollider(lua_State* L, int index, ::eyelib::world::components::Collider& out);

void PushRigidBody(lua_State* L, const ::eyelib::world::components::RigidBody& rigidBody);
bool ReadRigidBody(lua_State* L, int index, ::eyelib::world::components::RigidBody& out);

void PushAnimator(lua_State* L, const ::eyelib::world::components::Animator& animator);
bool ReadAnimator(lua_State* L, int index, ::eyelib::world::components::Animator& out);

void PushName(lua_State* L, const ::eyelib::world::components::Name& name);
bool ReadName(lua_State* L, int index, ::eyelib::world::components::Name& out);

void PushRegion(lua_State* L, const ::eyelib::world::components::Region& region);
bool ReadRegion(lua_State* L, int index, ::eyelib::world::components::Region& out);

void PushRegionMembership(lua_State* L, const ::eyelib::world::components::RegionMembership& membership);
bool ReadRegionMembership(lua_State* L, int index, ::eyelib::world::components::RegionMembership& out);

void PushAABB(lua_State* L, const ::eyelib::world::components::AABB& aabb);
bool ReadAABB(lua_State* L, int index, ::eyelib::world::components::AABB& out);

void PushSystemBindings(lua_State* L, const ::eyelib::world::components::SystemBindings& bindings);
bool ReadSystemBindings(lua_State* L, int index, ::eyelib::world::components::SystemBindings& out);

void PushActiveSystems(lua_State* L, const ::eyelib::world::components::ActiveSystems& systems);
bool ReadActiveSystems(lua_State* L, int index, ::eyelib::world::components::ActiveSystems& out);

void PushMetadata(lua_State* L, const std::string& name, uint32_t flags);
bool ReadMetadata(lua_State* L, int index, std::string& name, uint32_t& flags);

bool ReadPropertiesTable(lua_State* L, int index,
                         std::unordered_map<std::string, ::eyelib::world::components::ScriptValue>& out);
bool ReadProperties(lua_State* L,
                    const std::string& scriptName,
                    std::unordered_map<std::string, ::eyelib::world::components::ScriptValue>& out);
bool PropertiesEqual(const std::unordered_map<std::string, ::eyelib::world::components::ScriptValue>& a,
                     const std::unordered_map<std::string, ::eyelib::world::components::ScriptValue>& b);

} // namespace openeye::player
