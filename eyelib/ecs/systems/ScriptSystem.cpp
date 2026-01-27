#include "ScriptSystem.h"

#include "eyelib/ecs/World.h"
#include "eyelib/ecs/components/ScriptComponent.h"
#include "eyelib/scripting/ScriptManager.h"
#include "eyelib/scripting/bindings/BindingHelpers.h"
#include "eyelib/scripting/bindings/BindingRegistry.h"
#include "eyelib/scripting/bindings/TimeBindings.h"
#include "eyelib/scripting/bindings/EntityBindings.h"
#include "eyelib/scripting/bindings/TransformBindings.h"
#include "eyelib/core/Log.h"
#include <unordered_set>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

namespace eyelib {
namespace ecs {

void ScriptSystem::Update(World& world, float deltaTime) {
    m_world = &world;
    
    // Check for script changes (hot-reload)
    scripting::ScriptManager::Get().CheckForChanges();

    // Register bindings on first update
    static bool bindingsRegistered = false;
    if (!bindingsRegistered) {
        // Register bindings for all loaded scripts' VMs
        // For now, we'll register when we get a VM
        bindingsRegistered = true;
    }

    auto view = world.Registry().view<components::ScriptComponent>();
    for (auto entity : view) {
        auto& script = view.get<components::ScriptComponent>(entity);
        
        if (!script.isEnabled) continue;

        // Set entity reference
        script.entity = entity;

        // Initialize if needed
        if (!script.isInitialized) {
            InitializeScript(entity, script);
        }

        // Reload if needed
        if (script.needsReload) {
            ReloadScript(entity);
            script.needsReload = false;
        }

        // Update script
        if (script.isInitialized) {
            UpdateScript(entity, script, deltaTime);
        }
    }
}

void ScriptSystem::InitializeScript(Entity entity, components::ScriptComponent& script) {
    auto vm = scripting::ScriptManager::Get().GetScriptVM(script.scriptHandle);
    if (!vm || !vm->IsValid()) {
        core::Log::WarnF("ScriptSystem: Invalid VM for script handle %zu", script.scriptHandle);
        return;
    }

    lua_State* L = vm->GetState();
    
    // Register bindings (only once per VM)
    static std::unordered_set<lua_State*> registeredVMs;
    if (registeredVMs.find(L) == registeredVMs.end()) {
        if (m_world) {
            scripting::bindings::RegisterTimeBindings(L);
            scripting::bindings::RegisterEntityBindings(L, m_world);
            scripting::bindings::RegisterTransformBindings(L, m_world);
            registeredVMs.insert(L);
        }
    }
    
    // Get script name from ScriptManager
    std::string scriptName = scripting::ScriptManager::Get().GetScriptName(script.scriptHandle);
    if (scriptName.empty()) {
        scriptName = script.scriptName; // Fallback to component's scriptName
    }
    
    // Get script table from global
    lua_getglobal(L, scriptName.c_str());
    if (lua_istable(L, -1)) {
        // Inject properties from ScriptComponent into the script table
        // This allows C++ code to assign values/entities to script fields
        for (const auto& [key, value] : script.properties) {
            scripting::bindings::PushScriptValue(L, value, m_world);
            lua_setfield(L, -2, key.c_str());
        }
        
        lua_getfield(L, -1, "OnStart");
        if (lua_isfunction(L, -1)) {
            // Push self (the table)
            lua_pushvalue(L, -2);
            // Push entity
            lua_pushinteger(L, static_cast<lua_Integer>(entity));
            
            if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
                const char* error = lua_tostring(L, -1);
                core::Log::ErrorF("ScriptSystem: OnStart error for %s: %s", script.scriptName.c_str(), error);
                lua_pop(L, 1);
            }
        } else {
            lua_pop(L, 1); // Pop non-function
        }
    }
    lua_pop(L, 1); // Pop table

    script.isInitialized = true;
    core::Log::InfoF("ScriptSystem: Initialized script %s for entity %zu", script.scriptName.c_str(), entity);
}

void ScriptSystem::UpdateScript(Entity entity, components::ScriptComponent& script, float deltaTime) {
    auto vm = scripting::ScriptManager::Get().GetScriptVM(script.scriptHandle);
    if (!vm || !vm->IsValid()) {
        return;
    }

    lua_State* L = vm->GetState();
    
    // Get script name from ScriptManager
    std::string scriptName = scripting::ScriptManager::Get().GetScriptName(script.scriptHandle);
    if (scriptName.empty()) {
        scriptName = script.scriptName; // Fallback to component's scriptName
    }
    
    // Get script table
    lua_getglobal(L, scriptName.c_str());
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return;
    }

    // Call OnUpdate if it exists
    lua_getfield(L, -1, "OnUpdate");
    if (lua_isfunction(L, -1)) {
        // Push self (the table)
        lua_pushvalue(L, -2);
        // Push deltaTime
        lua_pushnumber(L, deltaTime);
        
        if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
            const char* error = lua_tostring(L, -1);
            core::Log::ErrorF("ScriptSystem: OnUpdate error for %s: %s", script.scriptName.c_str(), error);
            lua_pop(L, 1);
        }
    } else {
        lua_pop(L, 1); // Pop non-function
    }
    
    lua_pop(L, 1); // Pop table
}

void ScriptSystem::ShutdownScript(Entity entity, components::ScriptComponent& script) {
    auto vm = scripting::ScriptManager::Get().GetScriptVM(script.scriptHandle);
    if (!vm || !vm->IsValid()) {
        return;
    }

    lua_State* L = vm->GetState();
    
    // Get script name from ScriptManager
    std::string scriptName = scripting::ScriptManager::Get().GetScriptName(script.scriptHandle);
    if (scriptName.empty()) {
        scriptName = script.scriptName; // Fallback to component's scriptName
    }
    
    // Get script table
    lua_getglobal(L, scriptName.c_str());
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return;
    }

    // Call OnDestroy if it exists
    lua_getfield(L, -1, "OnDestroy");
    if (lua_isfunction(L, -1)) {
        // Push self (the table)
        lua_pushvalue(L, -2);
        // Push entity
        lua_pushinteger(L, static_cast<lua_Integer>(entity));
        
        if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
            const char* error = lua_tostring(L, -1);
            core::Log::ErrorF("ScriptSystem: OnDestroy error for %s: %s", script.scriptName.c_str(), error);
            lua_pop(L, 1);
        }
    } else {
        lua_pop(L, 1); // Pop non-function
    }
    
    lua_pop(L, 1); // Pop table

    script.isInitialized = false;
}

void ScriptSystem::OnCollision(Entity entity, Entity other) {
    // TODO: Implement collision event handling
}

void ScriptSystem::OnTriggerEnter(Entity entity, Entity other) {
    // TODO: Implement trigger event handling
}

void ScriptSystem::OnTriggerExit(Entity entity, Entity other) {
    // TODO: Implement trigger event handling
}

void ScriptSystem::ReloadScript(Entity entity) {
    // TODO: Implement script reloading
}

} // namespace ecs
} // namespace eyelib
