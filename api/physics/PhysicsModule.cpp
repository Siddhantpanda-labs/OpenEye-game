#include "api/physics/PhysicsModule.h"
#include "eyelib/physics/PhysicsGlobals.h"

namespace eyelib {
namespace api {

bool PhysicsModule::Initialize() {
    eyelib::physics::EnsureJoltGlobalsInitialized();
    return true;
}

void PhysicsModule::Shutdown() {
    eyelib::physics::ShutdownJoltGlobals();
}

} // namespace api
} // namespace eyelib
