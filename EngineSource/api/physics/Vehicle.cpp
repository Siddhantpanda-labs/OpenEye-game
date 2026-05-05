#include "api/physics/Vehicle.h"

namespace eyelib {
namespace api {
namespace physics {

VehicleHandle Vehicle::CreateVehicle(PhysicsWorld& /*world*/, RigidBodyHandle /*chassis*/,
                                     const std::vector<WheelDesc>& /*wheels*/) {
    return 0;
}

void Vehicle::DestroyVehicle(PhysicsWorld& /*world*/, VehicleHandle /*handle*/) {}

void Vehicle::SetEnginePower(PhysicsWorld& /*world*/, VehicleHandle /*handle*/, float /*power*/) {}

void Vehicle::SetBrakeForce(PhysicsWorld& /*world*/, VehicleHandle /*handle*/, float /*force*/) {}

void Vehicle::SetSteeringAngle(PhysicsWorld& /*world*/, VehicleHandle /*handle*/, float /*angle*/) {}

WheelTransform Vehicle::GetWheelTransform(PhysicsWorld& /*world*/, VehicleHandle /*handle*/, int /*wheelIndex*/) {
    return {};
}

} // namespace physics
} // namespace api
} // namespace eyelib
