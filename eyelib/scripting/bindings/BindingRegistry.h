#pragma once

struct lua_State;

namespace eyelib {
namespace ecs { class World; }
namespace scripting {
namespace bindings {

/**
 * @brief Manages binding context (World pointer) per Lua VM using Lua registry.
 * Thread-safe and supports multiple worlds.
 */
class BindingRegistry {
public:
    // Store World pointer in Lua registry (per-VM, thread-safe)
    static void SetWorld(lua_State* L, ecs::World* world);
    
    // Retrieve World pointer from Lua registry
    static ecs::World* GetWorld(lua_State* L);
    
    // Clear World pointer from registry
    static void ClearWorld(lua_State* L);

private:
    // Unique key for storing World pointer in Lua registry
    static const char* WORLD_REGISTRY_KEY;
};

} // namespace bindings
} // namespace scripting
} // namespace eyelib
