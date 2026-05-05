#include "api/physics/PhysicsBackendFactory.h"
#include "eyelib/physics/backend/jolt/JoltBackend.h"

namespace eyelib {
namespace api {
namespace physics {

std::unique_ptr<IPhysicsWorld> CreatePhysicsBackend(PhysicsBackendType type) {
    switch (type) {
    case PhysicsBackendType::Jolt:
        return std::make_unique<eyelib::physics::JoltBackend>();
    }
    return nullptr;
}

} // namespace physics
} // namespace api
} // namespace eyelib
