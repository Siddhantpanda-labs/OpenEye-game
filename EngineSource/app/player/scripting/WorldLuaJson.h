#pragma once

#include <json.hpp>

struct lua_State;

namespace openeye::player {

void JsonToLua(lua_State* L, const nlohmann::json& value);
nlohmann::json LuaToJson(lua_State* L, int index);

} // namespace openeye::player
