#pragma once

#include "api/physics/IPhysicsWorld.h"

#include <memory>

namespace eyelib {
namespace api {
namespace physics {

enum class PhysicsBackendType { Jolt };

/** Creates a physics backend. Used by PhysicsWorld; client code uses PhysicsWorld(BackendType), not this. */
std::unique_ptr<IPhysicsWorld> CreatePhysicsBackend(PhysicsBackendType type);

} // namespace physics
} // namespace api
} // namespace eyelib
