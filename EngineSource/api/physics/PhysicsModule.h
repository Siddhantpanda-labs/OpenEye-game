#pragma once

#include "api/core/Engine.h"

namespace eyelib {
namespace api {

/**
 * Engine subsystem that owns Jolt global state (allocator, Factory, type registration).
 * Registered with Engine before Initialize(); globals are initialized once and shut down
 * once at engine exit. Play/Stop must not reinitialize or tear down these globals.
 */
class PhysicsModule : public ISubsystem {
public:
    bool Initialize() override;
    void Shutdown() override;
    const char* GetName() const override { return "PhysicsModule"; }
};

} // namespace api
} // namespace eyelib
