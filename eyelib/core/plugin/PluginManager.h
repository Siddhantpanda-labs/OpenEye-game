#pragma once

#include "core/plugin/Plugin.h"
#include <string>
#include <unordered_map>
#include <memory>
#include <vector>

namespace eyelib {
namespace core {
namespace plugin {

/** Loads plugins (DLL/shared lib), tracks by name, holds registered interfaces. */
class PluginManager {
public:
    PluginManager() = default;
    ~PluginManager();

    /** Load plugin from path (e.g. "mylib.dll" or "libmylib.so"). Returns false on failure. */
    bool LoadPlugin(const std::string& path);
    /** Unload plugin by name. Calls Shutdown() then unloads. */
    void UnloadPlugin(const std::string& name);
    /** Get loaded plugin by name. Returns nullptr if not loaded. */
    IPlugin* GetPlugin(const std::string& name) const;
    /** Register a named interface (e.g. "IRenderer"). Overwrites existing. */
    void RegisterInterface(const std::string& name, void* interfacePtr);
    /** Get registered interface by name. Returns nullptr if not found. */
    void* GetInterface(const std::string& name) const;

private:
    struct LoadedPlugin {
        void* handle = nullptr;
        std::unique_ptr<IPlugin> plugin;
    };
    std::unordered_map<std::string, LoadedPlugin> m_plugins;
    std::unordered_map<std::string, void*> m_interfaces;
};

} // namespace plugin
} // namespace core
} // namespace eyelib
