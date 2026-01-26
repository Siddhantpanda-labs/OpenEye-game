#pragma once

#include "eyelib/ecs/Entity.h"
#include "eyelib/scripting/ScriptValue.h"
#include <string>
#include <unordered_map>

namespace eyelib {
namespace scripting {
    using ScriptHandle = size_t;
}

namespace ecs {
namespace components {

/**
 * @brief Component that attaches a Lua script to an entity.
 */
struct ScriptComponent {
    scripting::ScriptHandle scriptHandle = 0;
    std::string scriptName;        // "NPCBehavior", "WeaponController", etc.
    bool isEnabled = true;
    bool needsReload = false;
    
    // Script instance data (Lua table)
    // This is serialized and passed to Lua
    std::unordered_map<std::string, scripting::ScriptValue> properties;
    
    // Runtime state (internal, not serialized)
    bool isInitialized = false;
    float lastUpdateTime = 0.0f;
    
    // Entity reference (set by ScriptSystem)
    Entity entity = NullEntity;
};

} // namespace components
} // namespace ecs
} // namespace eyelib
