#pragma once

#include <string>
#include "eyelib/core/plugin/Plugin.h"

namespace api {
namespace plugin {

/** Re-export core plugin interface. Plugins implement this and export CreatePlugin(). */
using IPlugin = eyelib::core::plugin::IPlugin;

/**
 * @brief Loads plugins (DLL/shared lib), tracks by name, holds registered interfaces.
 */
class PluginManager {
public:
    PluginManager();
    ~PluginManager();
    PluginManager(PluginManager&&) noexcept;
    PluginManager& operator=(PluginManager&&) noexcept;
    PluginManager(const PluginManager&) = delete;
    PluginManager& operator=(const PluginManager&) = delete;

    bool LoadPlugin(const std::string& path);
    void UnloadPlugin(const std::string& name);
    IPlugin* GetPlugin(const std::string& name) const;
    void RegisterInterface(const std::string& name, void* interfacePtr);
    void* GetInterface(const std::string& name) const;

private:
    struct Impl;
    Impl* m_impl = nullptr;
};

} // namespace plugin
} // namespace api
