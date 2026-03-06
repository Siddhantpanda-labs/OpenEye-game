#pragma once

#include "eyelib/world/components/Script.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace openeye::player {
namespace scripting {

/**
 * Single property entry from a script's .schema table.
 * Types: "number", "string", "boolean", "entity", "vec3"
 */
struct ScriptPropertySchemaEntry {
    std::string name;
    std::string type;
    ::eyelib::world::components::ScriptValue defaultValue;
    double minVal = 0.0;
    double maxVal = 0.0;
    bool hasMinMax = false;
};

/**
 * Load script from path (resolved against assetRoots), run it, read .schema from
 * the returned table, then unload. Returns empty vector on failure or if script
 * has no .schema table.
 */
std::vector<ScriptPropertySchemaEntry> GetScriptSchema(
    const std::string& scriptPathOrName,
    const std::vector<std::string>& assetRoots);

/**
 * Build default script.properties map from schema (for new Script components).
 */
void ApplySchemaDefaults(
    const std::vector<ScriptPropertySchemaEntry>& schema,
    std::unordered_map<std::string, ::eyelib::world::components::ScriptValue>& properties);

} // namespace scripting
} // namespace openeye::player
