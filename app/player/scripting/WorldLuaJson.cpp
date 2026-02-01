#include "scripting/WorldLuaJson.h"

#include <cstddef>

extern "C" {
#include <lua.h>
}

namespace openeye::player {

namespace {

bool IsArrayTable(lua_State* L, int index, size_t& maxIndex) {
    const int absIndex = lua_absindex(L, index);
    bool isArray = true;
    maxIndex = 0;
    lua_pushnil(L);
    while (lua_next(L, absIndex) != 0) {
        if (!lua_isinteger(L, -2)) {
            isArray = false;
            lua_pop(L, 1);
            break;
        }
        const lua_Integer key = lua_tointeger(L, -2);
        if (key < 1) {
            isArray = false;
            lua_pop(L, 1);
            break;
        }
        if (static_cast<size_t>(key) > maxIndex) {
            maxIndex = static_cast<size_t>(key);
        }
        lua_pop(L, 1);
    }
    return isArray;
}

} // namespace

void JsonToLua(lua_State* L, const nlohmann::json& value) {
    if (value.is_null()) {
        lua_pushnil(L);
        return;
    }
    if (value.is_boolean()) {
        lua_pushboolean(L, value.get<bool>() ? 1 : 0);
        return;
    }
    if (value.is_number()) {
        lua_pushnumber(L, value.get<double>());
        return;
    }
    if (value.is_string()) {
        const auto& s = value.get_ref<const std::string&>();
        lua_pushstring(L, s.c_str());
        return;
    }
    if (value.is_array()) {
        lua_newtable(L);
        int i = 1;
        for (const auto& item : value) {
            JsonToLua(L, item);
            lua_rawseti(L, -2, i++);
        }
        return;
    }
    if (value.is_object()) {
        lua_newtable(L);
        for (const auto& item : value.items()) {
            JsonToLua(L, item.value());
            lua_setfield(L, -2, item.key().c_str());
        }
        return;
    }
    lua_pushnil(L);
}

nlohmann::json LuaToJson(lua_State* L, int index) {
    if (lua_isnil(L, index)) {
        return nullptr;
    }
    if (lua_isboolean(L, index)) {
        return static_cast<bool>(lua_toboolean(L, index));
    }
    if (lua_isnumber(L, index)) {
        return static_cast<double>(lua_tonumber(L, index));
    }
    if (lua_isstring(L, index)) {
        return std::string(lua_tostring(L, index));
    }
    if (!lua_istable(L, index)) {
        return nullptr;
    }

    size_t maxIndex = 0;
    const bool isArray = IsArrayTable(L, index, maxIndex);
    const int absIndex = lua_absindex(L, index);

    if (isArray) {
        nlohmann::json arr = nlohmann::json::array();
        for (size_t i = 1; i <= maxIndex; ++i) {
            lua_rawgeti(L, absIndex, static_cast<lua_Integer>(i));
            arr.push_back(LuaToJson(L, lua_gettop(L)));
            lua_pop(L, 1);
        }
        return arr;
    }

    nlohmann::json obj = nlohmann::json::object();
    lua_pushnil(L);
    while (lua_next(L, absIndex) != 0) {
        std::string key;
        if (lua_isstring(L, -2)) {
            key = lua_tostring(L, -2);
        } else if (lua_isinteger(L, -2)) {
            key = std::to_string(lua_tointeger(L, -2));
        } else {
            lua_pop(L, 1);
            continue;
        }
        obj[key] = LuaToJson(L, lua_gettop(L));
        lua_pop(L, 1);
    }
    return obj;
}

} // namespace openeye::player
