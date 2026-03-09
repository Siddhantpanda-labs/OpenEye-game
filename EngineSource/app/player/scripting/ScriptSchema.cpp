#include "scripting/ScriptSchema.h"

#include "api/core/All.h"
#include "api/platform/All.h"
#include "api/scripting/Script.h"

#include "scripting/WorldLuaHelpers.h"

#include "eyelib/world/EntityId.h"
#include "eyelib/math/Types.h"

#include <cmath>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

namespace openeye::player {
namespace scripting {

namespace {

std::string JoinPath(const std::string& base, const std::string& leaf) {
    if (base.empty()) return leaf;
    char last = base.back();
    if (last == '/' || last == '\\') return base + leaf;
    return base + "/" + leaf;
}

std::string ResolveScriptPath(const std::string& name,
                              const std::vector<std::string>& assetRoots) {
    using eyelib::api::platform::filesystem::FileExists;
    if (name.empty()) return {};
    if (FileExists(name)) return name;
    for (const auto& root : assetRoots) {
        std::string candidate = JoinPath(root, name);
        if (FileExists(candidate)) return candidate;
    }
    return {};
}

using ScriptValue = ::eyelib::world::components::ScriptValue;
using ScriptValueType = ScriptValue::Type;

bool ReadDefaultFromLua(lua_State* L, int tableIndex, const std::string& typeStr,
                        ScriptValue& out) {
    lua_getfield(L, tableIndex, "default");
    int idx = lua_gettop(L);
    if (lua_isnil(L, idx)) {
        lua_pop(L, 1);
        if (typeStr == "number") { out = ScriptValue(0.0); return true; }
        if (typeStr == "string") { out = ScriptValue(std::string()); return true; }
        if (typeStr == "boolean") { out = ScriptValue(false); return true; }
        if (typeStr == "entity") { out = ScriptValue(::eyelib::world::InvalidEntity); return true; }
        if (typeStr == "vec3") { out = ScriptValue(::eyelib::math::Vec3{0,0,0}); return true; }
        return false;
    }
    if (typeStr == "number" && lua_isnumber(L, idx)) {
        out = ScriptValue(lua_tonumber(L, idx));
        lua_pop(L, 1);
        return true;
    }
    if (typeStr == "string" && lua_isstring(L, idx)) {
        out = ScriptValue(std::string(lua_tostring(L, idx)));
        lua_pop(L, 1);
        return true;
    }
    if (typeStr == "boolean") {
        out = ScriptValue(lua_toboolean(L, idx) != 0);
        lua_pop(L, 1);
        return true;
    }
    if (typeStr == "entity") {
        if (lua_isnil(L, idx)) {
            out = ScriptValue(::eyelib::world::InvalidEntity);
            lua_pop(L, 1);
            return true;
        }
        if (lua_istable(L, idx)) {
            ::eyelib::world::EntityId id;
            lua_getfield(L, idx, "index");
            if (lua_isnumber(L, -1)) id.index = static_cast<uint32_t>(lua_tointeger(L, -1));
            lua_pop(L, 1);
            lua_getfield(L, idx, "generation");
            if (lua_isnumber(L, -1)) id.generation = static_cast<uint32_t>(lua_tointeger(L, -1));
            lua_pop(L, 1);
            out = ScriptValue(id);
            lua_pop(L, 1);
            return true;
        }
        lua_pop(L, 1);
        return false;
    }
    if (typeStr == "vec3" && lua_istable(L, idx)) {
        ::eyelib::math::Vec3 v{};
        if (::openeye::player::ReadVec3(L, idx, v)) {
            out = ScriptValue(v);
            lua_pop(L, 1);
            return true;
        }
        lua_pop(L, 1);
        return false;
    }
    lua_pop(L, 1);
    return false;
}

} // namespace

std::vector<ScriptPropertySchemaEntry> GetScriptSchema(
    const std::string& scriptPathOrName,
    const std::vector<std::string>& assetRoots) {

    std::vector<ScriptPropertySchemaEntry> result;
    std::string path = ResolveScriptPath(scriptPathOrName, assetRoots);
    if (path.empty()) return result;

    ::eyelib::scripting::ScriptHandle handle = eyelib::api::Script::Load(path);
    if (!eyelib::api::Script::IsValid(handle)) return result;

    lua_State* L = eyelib::api::Script::GetState(handle);
    if (!L) {
        eyelib::api::Script::Unload(handle);
        return result;
    }

    std::string scriptName = eyelib::api::Script::GetName(handle);
    lua_getglobal(L, scriptName.c_str());
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        eyelib::api::Script::Unload(handle);
        return result;
    }

    lua_getfield(L, -1, "schema");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 2);
        eyelib::api::Script::Unload(handle);
        return result;
    }

    const int schemaIdx = lua_gettop(L);
    const size_t len = lua_rawlen(L, schemaIdx);
    result.reserve(len);

    for (size_t i = 1; i <= len; ++i) {
        lua_rawgeti(L, schemaIdx, static_cast<int>(i));
        if (!lua_istable(L, -1)) {
            lua_pop(L, 1);
            continue;
        }
        int entryIdx = lua_gettop(L);

        ScriptPropertySchemaEntry entry;
        if (!::openeye::player::ReadStringField(L, entryIdx, "name", entry.name) || entry.name.empty()) {
            lua_pop(L, 1);
            continue;
        }
        if (!::openeye::player::ReadStringField(L, entryIdx, "type", entry.type)) {
            lua_pop(L, 1);
            continue;
        }
        if (!ReadDefaultFromLua(L, entryIdx, entry.type, entry.defaultValue)) {
            lua_pop(L, 1);
            continue;
        }
        float minF = 0.f, maxF = 0.f;
        if (::openeye::player::ReadNumberField(L, entryIdx, "min", minF) && ::openeye::player::ReadNumberField(L, entryIdx, "max", maxF)) {
            entry.hasMinMax = true;
            entry.minVal = static_cast<double>(minF);
            entry.maxVal = static_cast<double>(maxF);
        }
        result.push_back(std::move(entry));
        lua_pop(L, 1);
    }

    lua_pop(L, 2);
    eyelib::api::Script::Unload(handle);
    return result;
}

void ApplySchemaDefaults(
    const std::vector<ScriptPropertySchemaEntry>& schema,
    std::unordered_map<std::string, ScriptValue>& properties) {
    for (const auto& entry : schema)
        properties[entry.name] = entry.defaultValue;
}

} // namespace scripting
} // namespace openeye::player
