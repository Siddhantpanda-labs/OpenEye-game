#pragma once

#include <string>

namespace eyelib {
namespace core {
namespace plugin {

/** Base plugin interface. Plugins implement this and export CreatePlugin(). */
class IPlugin {
public:
    virtual ~IPlugin() = default;
    virtual const char* GetName() const = 0;
    virtual const char* GetVersion() const = 0;
    virtual bool Initialize() = 0;
    virtual void Shutdown() = 0;
};

} // namespace plugin
} // namespace core
} // namespace eyelib
